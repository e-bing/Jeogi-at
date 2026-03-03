#include "../includes/hanwha_node.hpp"

#include "../includes/shared_data.hpp"

extern volatile sig_atomic_t stop_flag;

HanwhaNode::HanwhaNode(const std::string& rtsp_url) : url(rtsp_url) {}

HanwhaNode::~HanwhaNode() {
  if (frame) av_frame_free(&frame);
  if (pkt) av_packet_free(&pkt);
  if (codecCtx) avcodec_free_context(&codecCtx);
  if (fmtCtx) avformat_close_input(&fmtCtx);
}

void HanwhaNode::run() {
  avformat_network_init();
  AVDictionary* opts = nullptr;
  av_dict_set(&opts, "rtsp_transport", "tcp", 0);
  av_dict_set(&opts, "flags", "low_delay", 0);

  if (avformat_open_input(&fmtCtx, url.c_str(), nullptr, &opts) < 0) {
    std::cerr << "[Hanwha Node] 연결 실패: " << url << std::endl;
    return;
  }

  avformat_find_stream_info(fmtCtx, nullptr);

  for (int i = 0; i < fmtCtx->nb_streams; i++) {
    if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_idx = i;
      std::cout << "[DEBUG] Video Stream Found: " << i << std::endl;
    } else if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_DATA) {
      data_idx = i;
      std::cout << "[DEBUG] Metadata Stream Found: " << i << std::endl;
    }
  }

  if (video_idx == -1) return;

  // 비디오 코덱 설정
  AVCodecParameters* codecParams = fmtCtx->streams[video_idx]->codecpar;
  const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
  codecCtx = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(codecCtx, codecParams);
  avcodec_open2(codecCtx, codec, nullptr);

  pkt = av_packet_alloc();
  frame = av_frame_alloc();

  std::cout << "[Hanwha Node] 접속 성공 및 스트리밍 시작" << std::endl;
  process_loop();
}

void HanwhaNode::process_loop() {
  while (!stop_flag) {
    if (av_read_frame(fmtCtx, pkt) < 0) break;

    // 1. 영상 패킷 처리
    if (pkt->stream_index == video_idx) {
      if (avcodec_send_packet(codecCtx, pkt) == 0) {
        while (avcodec_receive_frame(codecCtx, frame) == 0) {
          size_t y_size = frame->width * frame->height;
          size_t uv_size = (frame->width / 2) * (frame->height / 2);

          std::lock_guard<std::mutex> lock(g_hw_frame_mutex);
          g_hw_frame_buffer.resize(y_size + uv_size * 2);
          memcpy(g_hw_frame_buffer.data(), frame->data[0], y_size);
          memcpy(g_hw_frame_buffer.data() + y_size, frame->data[1], uv_size);
          memcpy(g_hw_frame_buffer.data() + y_size + uv_size, frame->data[2],
                 uv_size);
        }
      }
    }
    // 2. 메타데이터(XML) 패킷 처리
    else if (pkt->stream_index == data_idx) {
      metaParser.processPacket(pkt);
      std::vector<MetadataResult> results = metaParser.getCompletedResults();

      if (!results.empty()) {
        auto latest_objects = results.back().objects;
        if (!latest_objects.empty()) {
          std::lock_guard<std::mutex> lock(g_hw_data_mutex);
          g_hw_objects = latest_objects;
        }
      }
    }
    av_packet_unref(pkt);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  std::cout << "[Hanwha Node] 루프 종료 중..." << std::endl;
}