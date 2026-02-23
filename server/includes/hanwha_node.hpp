#ifndef HANWHA_NODE_HPP
#define HANWHA_NODE_HPP

#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "parser.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class HanwhaNode {
 public:
  HanwhaNode(const std::string& rtsp_url);
  ~HanwhaNode();

  void run();  // 별도 스레드에서 실행될 함수

 private:
  void process_loop();

  std::string url;
  AVFormatContext* fmtCtx = nullptr;
  AVCodecContext* codecCtx = nullptr;
  AVFrame* frame = nullptr;
  AVPacket* pkt = nullptr;
  // struct SwsContext* swsCtx = nullptr;  // 리사이징용 (필요시)

  int video_idx = -1;
  int data_idx = -1;

  MetadataParser metaParser;
};

#endif