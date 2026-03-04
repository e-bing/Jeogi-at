#include <termios.h>
#include <unistd.h>

#include <iostream>
#include <regex>
#include <string>

#include "../includes/config_manager.hpp"

// IP 유효성 검사 함수
bool isValidIP(const std::string& ip) {
  std::regex ip_pattern(
      "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]"
      "?[0-9][0-9]?)$");
  return std::regex_match(ip, ip_pattern);
}

void setEcho(bool enable) {
  struct termios tty;
  tcgetattr(STDIN_FILENO, &tty);
  if (!enable)
    tty.c_lflag &= ~ECHO;
  else
    tty.c_lflag |= ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

int main() {
  json config;
  std::string input;

  std::cout << "=================================\n";
  std::cout << "     AI Camera Monitor Setup     \n";
  std::cout << "=================================\n\n";

  // 1. 한화 카메라 설정
  std::cout << "[1] Hanwha Camera Settings\n";
  while (true) {
    std::cout << " - Camera IP: ";
    std::cin >> input;
    if (isValidIP(input)) {
      config["hanwha"]["ip"] = input;
      break;
    }
    std::cerr << "   [Error] Invalid IP format.\n";
  }

  std::cout << " - Camera User ID: ";
  std::cin >> input;
  config["hanwha"]["user"] = input;

  std::cout << " - Camera Password (Input will be hidden): ";
  setEcho(false);  // 화면 출력 중지
  std::cin >> input;
  setEcho(true);      // 화면 출력 복구
  std::cout << "\n";  // 입력 후 줄바꿈
  config["hanwha"]["pw"] = input;

  std::cout << " - RTSP Profile (default: profile2): ";
  std::cin.ignore();  // 버퍼 비우기
  std::getline(std::cin, input);
  config["hanwha"]["profile"] = input.empty() ? "profile2" : input;

  std::cout << "\n";

  // 2. 라즈베리 파이 설정
  json pi_list = json::array();
  int camera_count = 1;
  std::cout << "[2] Raspberry Pi Settings\n";
  while (true) {
    std::cout << "\n[Pi Node #" << pi_list.size() + 1
              << " Settings] (Type 'q' in IP to finish)\n";
    std::string ip, id, desc;

    std::cout << " - Pi IP: ";
    std::cin >> ip;
    if (ip == "q") break;
    if (!isValidIP(ip)) {
      std::cerr << "   [Error] Invalid IP format.\n";
      continue;
    }

    json pi_node;
    pi_node["ip"] = ip;

    char id_buf[10];
    sprintf(id_buf, "CAM_%02d", camera_count);
    pi_node["id"] = std::string(id_buf);

    // 구역 정보 나중에 qt에서 받아오게 설정
    pi_node["platforms"] = json::array();

    char num_str[3];
    sprintf(num_str, "%02d", camera_count);
    pi_node["mqtt_topic"] = std::string("iot/pi") + num_str + "/sensor/camera";

    pi_list.push_back(pi_node);
    std::cout << "   [Added] " << pi_node["id"] << " (" << ip << ")\n";
    camera_count++;
  }

  // 3. MQTT 브로커 설정 - 현재 서버 IP 자동 감지
  std::cout << "[3] MQTT Broker Settings\n";

  // 현재 머신의 IP 자동 감지
  FILE* pipe = popen("hostname -I | awk '{print $1}'", "r");
  std::string server_ip = "localhost";
  if (pipe) {
      char buf[64];
      if (fgets(buf, sizeof(buf), pipe)) {
          server_ip = buf;
          server_ip.erase(server_ip.find_last_not_of(" \n\r\t") + 1); // trim
      }
      pclose(pipe);
  }

  config["mqtt"]["broker"] = "tcp://" + server_ip + ":1883";
  std::cout << " - MQTT Broker: " << config["mqtt"]["broker"] << " (자동 감지)\n";

  // 3. 파일 저장
  config["pi_nodes"] = pi_list;
  ConfigManager::save(config);

  std::cout << "\n---------------------------------\n";
  std::cout << "[Success] Settings saved to config.json\n";
  std::cout << "---------------------------------\n";

  return 0;
}
