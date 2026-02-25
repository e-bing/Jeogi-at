#ifndef PI_NODE_HPP
#define PI_NODE_HPP

#include <mqtt/async_client.h>

#include <csignal>
#include <mutex>
#include <string>
#include <vector>

#include "shared_data.hpp"

extern "C" {
#include <SDL2/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

/**
 * @brief 라즈베리 파이(Edge) 노드 관리 클래스
 * 영상 수신(TCP) 및 분석 데이터 수신(MQTT)을 담당합니다.
 */
class PiNode {
 public:
  /**
   * @param ip 라즈베리 파이의 IP 주소
   */
  PiNode(const std::string& ip, const std::string& topic,
         const std::string& id);
  ~PiNode();

  /**
   * @brief 노드 실행 (스레드에서 호출됨)
   */
  void run();
  void process_loop();

  /**
   * @brief 현재 디코딩된 프레임을 SDL 텍스처로 업데이트
   * @param texture 업데이트할 SDL 텍스처
   */
  void update_texture(SDL_Texture* texture);

 private:
  std::string pi_ip;
  std::string mqtt_topic;
  std::string node_id;

  // FFmpeg 관련 자원
  AVFormatContext* fmtCtx = nullptr;
  AVCodecContext* codecCtx = nullptr;
  AVFrame* frame = nullptr;
  AVPacket* pkt = nullptr;
  int video_idx = -1;

  // 내부 처리 함수
  bool init_mqtt();
  bool init_ffmpeg();

  // MQTT 클라이언트 및 콜백은 내부 클래스로 구현하거나 멤버로 관리
  mqtt::async_client* mqtt_client = nullptr;
  void* cb = nullptr;
};

#endif  // PI_NODE_HPP