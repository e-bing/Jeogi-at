#include "../includes/pi_node.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

extern volatile sig_atomic_t stop_flag;

class PiMqttCallback : public virtual mqtt::callback {
 public:
  void message_arrived(mqtt::const_message_ptr msg) override {
    try {
      auto j = json::parse(msg->get_payload_str());
      std::lock_guard<std::mutex> lock(g_pi_data_mutex);
      g_pi_shared_objects.clear();
      for (auto& obj : j["blocks"]) {
        DetectedObject res;
        res.x = obj["x"].get<float>();
        res.y = obj["y"].get<float>();
        res.w = obj["w"].get<float>();
        res.h = obj["h"].get<float>();
        res.typeName = "Person";
        g_pi_shared_objects.push_back(res);
      }
    } catch (...) {
    }
  }
};

PiNode::PiNode(const std::string& ip) : pi_ip(ip) {}

void PiNode::run() {
  // 1. MQTT 초기화 (지역변수 client 대신 멤버변수 mqtt_client 사용)
  mqtt_client =
      new mqtt::async_client("tcp://" + pi_ip + ":1883", "Monitor_Server_Pi");
  this->cb = new PiMqttCallback();
  mqtt_client->set_callback(*(PiMqttCallback*)this->cb);

  mqtt::connect_options connOpts;
  connOpts.set_clean_session(true);

  try {
    mqtt_client->connect(connOpts)->wait();
    mqtt_client->subscribe("iot/pi01/sensor/camera", 1)->wait();
    std::cout << "[Pi Node] MQTT 연결 성공!" << std::endl;
  } catch (const mqtt::exception& exc) {
    std::cerr << "[Pi Node] MQTT 에러: " << exc.what() << std::endl;
    return;
  }

  // 2. FFmpeg 설정 (지역변수 선언문 제거 -> 멤버 변수 사용)
  avformat_network_init();
  AVDictionary* opts = nullptr;
  std::string url = "tcp://" + pi_ip + ":5000";

  av_dict_set(&opts, "probesize", "32", 0);       // 축소 테스트 중
  av_dict_set(&opts, "analyzeduration", "0", 0);  // 분석 시간 0
  av_dict_set(&opts, "fflags", "nobuffer", 0);    // 버퍼링 제거
  av_dict_set(&opts, "flags", "low_delay", 0);    // 저지연 모드 활성화
  av_dict_set(&opts, "err_detect", "ignore_err", 0);
  AVInputFormat* ifmt = av_find_input_format("h264");

  if (avformat_open_input(&this->fmtCtx, url.c_str(), ifmt, &opts) < 0) {
    std::cerr << "[Pi Node] 영상 접속 실패" << std::endl;
    return;
  }

  avformat_find_stream_info(fmtCtx, nullptr);
  this->video_idx =
      av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

  // 3. 코덱 설정 (멤버 변수에 할당)
  AVCodecParameters* codecParams = fmtCtx->streams[video_idx]->codecpar;
  const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
  this->codecCtx = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(codecCtx, codecParams);
  avcodec_open2(codecCtx, codec, nullptr);

  this->pkt = av_packet_alloc();
  this->frame = av_frame_alloc();

  // 4. SDL 초기화

  process_loop();
}

void PiNode::process_loop() {
  if (!fmtCtx || !codecCtx || !pkt || !frame) {
    std::cerr << "[Pi Node] 에러: 유효하지 않은 자원 참조" << std::endl;
    return;
  }

  while (!stop_flag) {
    // av_read_frame 결과값 체크 강화
    int ret = av_read_frame(fmtCtx, pkt);
    if (ret < 0) {
      if (ret == AVERROR_EOF) std::cout << "End of stream" << std::endl;
      break;
    }

    if (pkt->stream_index == video_idx) {
      if (avcodec_send_packet(codecCtx, pkt) == 0) {
        while (avcodec_receive_frame(codecCtx, frame) == 0) {
          int w = frame->width;
          int h = frame->height;
          size_t y_size = w * h;
          size_t uv_size = (w / 2) * (h / 2);

          {
            std::lock_guard<std::mutex> lock(g_pi_frame_mutex);
            g_pi_frame_buffer.resize(y_size + uv_size * 2);

            // Y, U, V 플레인을 하나의 벡터에 순서대로 복사
            memcpy(g_pi_frame_buffer.data(), frame->data[0], y_size);
            memcpy(g_pi_frame_buffer.data() + y_size, frame->data[1], uv_size);
            memcpy(g_pi_frame_buffer.data() + y_size + uv_size, frame->data[2],
                   uv_size);
          }
        }
      }
    }
    av_packet_unref(pkt);

    // 너무 빠르게 돌지 않도록 아주 미세한 휴식 (스레드 점유 조절)
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  std::cout << "[Pi Node] 루프 종료 중..." << std::endl;
}

PiNode::~PiNode() {
  if (frame) av_frame_free(&frame);
  if (pkt) av_packet_free(&pkt);
  if (codecCtx) avcodec_free_context(&codecCtx);
  if (fmtCtx) avformat_close_input(&fmtCtx);
  if (mqtt_client) {
    mqtt_client->disconnect();
    delete mqtt_client;
  }
  if (cb) delete (PiMqttCallback*)cb;
}