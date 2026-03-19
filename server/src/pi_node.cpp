#include "../includes/pi_node.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

extern volatile sig_atomic_t stop_flag;

class PiMqttCallback : public virtual mqtt::callback {
 private:
  std::string node_id;

 public:
  PiMqttCallback(const std::string& id) : node_id(id) {}
  void message_arrived(mqtt::const_message_ptr msg) override {
    try {
      auto j = json::parse(msg->get_payload_str());

      auto it = g_pi_node_map.find(node_id);
      if (it == g_pi_node_map.end()) return;

      auto& camData = it->second;
      std::lock_guard<std::mutex> lock(camData->data_mutex);
      camData->objects.clear();

      for (auto& obj : j["blocks"]) {
        DetectedObject res;
        res.x = obj.value("x", 0.0f);
        res.y = obj.value("y", 0.0f);
        res.w = obj.value("w", 0.0f);
        res.h = obj.value("h", 0.0f);
        res.typeName = "Person";
        camData->objects.push_back(res);
      }
    } catch (const std::exception& e) {
      std::cerr << "[" << node_id << "] JSON Parsing Error: " << e.what()
                << std::endl;
    }
  }
};

PiNode::PiNode(const std::string& ip, const std::string& topic,
               const std::string& id)
    : pi_ip(ip), mqtt_topic(topic), node_id(id) {}

void PiNode::run() {
  av_log_set_level(AV_LOG_FATAL);
  // 1. MQTT 초기화
  mqtt_client =
      new mqtt::async_client("tcp://" + pi_ip + ":1883", "Monitor_" + node_id);
  this->cb = new PiMqttCallback(node_id);
  mqtt_client->set_callback(*(PiMqttCallback*)this->cb);

  mqtt::connect_options connOpts;
  connOpts.set_clean_session(true);

  try {
    mqtt_client->connect(connOpts)->wait();
    mqtt_client->subscribe(mqtt_topic, 1)->wait();
    std::cout << "[Pi Node " + node_id + "] MQTT 연결 성공!" << std::endl;
  } catch (const mqtt::exception& exc) {
    std::cerr << "[Pi Node " + node_id + "] MQTT 에러: " << exc.what()
              << std::endl;
    return;
  }

  // 2. FFmpeg 설정
  avformat_network_init();
  AVDictionary* opts = nullptr;
  std::string url = "tcp://" + pi_ip + ":5000";

  av_dict_set(&opts, "probesize", "100000", 0);
  av_dict_set(&opts, "analyzeduration", "100000", 0);
  av_dict_set(&opts, "fflags", "nobuffer", 0);  // 버퍼링 제거
  av_dict_set(&opts, "flags", "low_delay", 0);  // 저지연 모드 활성화
  av_dict_set(&opts, "err_detect", "ignore_err", 0);
  AVInputFormat* ifmt = av_find_input_format("h264");

  if (avformat_open_input(&this->fmtCtx, url.c_str(), ifmt, &opts) < 0) {
    std::cerr << "[Pi Node " + node_id + "] 영상 접속 실패" << std::endl;
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
    std::cerr << "[Pi Node " + node_id + "] 에러: 유효하지 않은 자원 참조"
              << std::endl;
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

          auto it = g_pi_node_map.find(node_id);
          if (it != g_pi_node_map.end()) {
            auto& camData = it->second;
            std::lock_guard<std::mutex> lock(camData->data_mutex);

            camData->width = frame->width;
            camData->height = frame->height;
            camData->frame_buffer.resize(y_size + uv_size * 2);

            // Y 플레인
            for (int i = 0; i < h; i++) {
              memcpy(camData->frame_buffer.data() + i * w,
                     frame->data[0] + i * frame->linesize[0], w);
            }
            // U 플레인
            for (int i = 0; i < h / 2; i++) {
              memcpy(camData->frame_buffer.data() + y_size + i * (w / 2),
                     frame->data[1] + i * frame->linesize[1], w / 2);
            }
            // V 플레인
            for (int i = 0; i < h / 2; i++) {
              memcpy(
                  camData->frame_buffer.data() + y_size + uv_size + i * (w / 2),
                  frame->data[2] + i * frame->linesize[2], w / 2);
            }
          }
        }
      }
    }
    av_packet_unref(pkt);

    // 너무 빠르게 돌지 않도록 아주 미세한 휴식 (스레드 점유 조절)
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  std::cout << "[Pi Node " + node_id + "] 루프 종료 중..." << std::endl;
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