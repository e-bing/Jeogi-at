#ifndef PARSER_HPP
#define PARSER_HPP

#include <iostream>
#include <string>
#include <vector>

#include "shared_data.hpp"
#include "tinyxml2.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

// 메타데이터 결과 구조체
struct MetadataResult {
  int64_t pts;
  std::vector<DetectedObject> objects;
};

class MetadataParser {
 public:
  void processPacket(AVPacket* packet);
  std::vector<MetadataResult> getCompletedResults();

 private:
  std::string xml_buffer;
  std::vector<std::pair<std::string, int64_t>> completed_streams;
  std::vector<MetadataResult> results;

  void processBuffer(int64_t pts);
  void cleanupBuffer();
  void processCompletedStreams();
  void processXmlDoc(tinyxml2::XMLDocument& doc,
                     std::vector<DetectedObject>& result);
  std::vector<DetectedObject> extractObj(tinyxml2::XMLElement* root);
};

#endif  // PARSER_HPP
