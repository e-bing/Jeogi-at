#include "../includes/parser.hpp"

#include <cstring>
#include <fstream>

using namespace tinyxml2;

std::vector<DetectedObject> MetadataParser::extractObj(XMLElement* root) {
  std::vector<DetectedObject> objItems;
  if (!root) return objItems;

  XMLElement* analytics = root->FirstChildElement("tt:VideoAnalytics");
  if (!analytics) return objItems;

  XMLElement* frame = analytics->FirstChildElement("tt:Frame");
  if (!frame) return objItems;

  for (XMLElement* obj = frame->FirstChildElement("tt:Object"); obj;
       obj = obj->NextSiblingElement("tt:Object")) {
    const char* objectId = obj->Attribute("ObjectId");
    if (!objectId) continue;

    DetectedObject objItem;
    objItem.objectId = std::stoi(objectId);
    // 초기값
    objItem.x = 0;
    objItem.y = 0;
    objItem.w = 0;
    objItem.h = 0;

    XMLElement* appearance = obj->FirstChildElement("tt:Appearance");
    if (appearance) {
      XMLElement* shape = appearance->FirstChildElement("tt:Shape");
      if (shape) {
        XMLElement* bbox = shape->FirstChildElement("tt:BoundingBox");
        if (bbox) {
          double left, top, right, bottom;
          bbox->QueryDoubleAttribute("left", &left);
          bbox->QueryDoubleAttribute("top", &top);
          bbox->QueryDoubleAttribute("right", &right);
          bbox->QueryDoubleAttribute("bottom", &bottom);

          auto to_ratio_x = [](double val) { return (float)(val / 3840.0); };

          auto to_ratio_y = [](double val) { return (float)(val / 2160.0); };

          float n_l = to_ratio_x(left);
          float n_r = to_ratio_x(right);
          float n_t = to_ratio_y(top);
          float n_b = to_ratio_y(bottom);

          objItem.x = (n_l < n_r) ? n_l : n_r;
          objItem.y = (n_t < n_b) ? n_t : n_b;
          objItem.w = std::abs(n_r - n_l);
          objItem.h = std::abs(n_b - n_t);

          // 화면 경계 이탈 방지
          // 좌표가 0보다 작으면 0으로, 1보다 크면 화면 끝에 붙임
          if (objItem.x < 0) objItem.x = 0;
          if (objItem.y < 0) objItem.y = 0;
          if (objItem.x > 1.0f) objItem.x = 1.0f;
          if (objItem.y > 1.0f) objItem.y = 1.0f;

          if (objItem.x + objItem.w > 1.0f) objItem.w = 1.0f - objItem.x;
          if (objItem.y + objItem.h > 1.0f) objItem.h = 1.0f - objItem.y;

          // 최소 크기 보장 (데이터가 너무 작아 점처럼 보이는 것 방지)
          if (objItem.w < 0.001f) objItem.w = 0.02f;
          if (objItem.h < 0.001f) objItem.h = 0.02f;
        }
      }

      XMLElement* classElem = appearance->FirstChildElement("tt:Class");
      if (classElem) {
        XMLElement* type = classElem->FirstChildElement("tt:Type");
        if (type) {
          const char* typeText = type->GetText();
          objItem.typeName = typeText ? typeText : "Unknown";

          // 필터링
          if (objItem.typeName == "Human") {
            objItems.push_back(objItem);
          }
        }
      }
    }
  }
  return objItems;
}

void MetadataParser::processXmlDoc(XMLDocument& doc,
                                   std::vector<DetectedObject>& result) {
  XMLElement* root = doc.RootElement();
  if (!root) return;

  std::vector<DetectedObject> detections = extractObj(root);
  if (!detections.empty()) {
    result.insert(result.end(), detections.begin(), detections.end());
  }
}

void MetadataParser::processPacket(AVPacket* packet) {
  if (!packet || !packet->data || packet->size <= 0) return;
  xml_buffer.append(reinterpret_cast<const char*>(packet->data), packet->size);
  processBuffer(packet->pts);
}

void MetadataParser::processBuffer(int64_t pts) {
  size_t pos = 0;
  const std::string xml_declaration =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

  while ((pos = xml_buffer.find("<tt:MetadataStream", pos)) !=
         std::string::npos) {
    size_t end_pos = xml_buffer.find("</tt:MetadataStream>", pos);
    if (end_pos == std::string::npos) break;

    size_t element_end = end_pos + std::string("</tt:MetadataStream>").length();
    std::string complete_element = xml_buffer.substr(pos, element_end - pos);

    completed_streams.emplace_back(xml_declaration + complete_element, pts);

    xml_buffer.erase(0, element_end);
    pos = 0;
  }
  processCompletedStreams();
  cleanupBuffer();
}

void MetadataParser::processCompletedStreams() {
  std::ofstream outFile("metadata_log.xml", std::ios::app);

  for (const auto& stream_pair : completed_streams) {
    if (outFile.is_open()) {
      outFile << "--- PTS: " << stream_pair.second << " ---\n";
      outFile << stream_pair.first << "\n\n";
    }

    XMLDocument doc;
    if (doc.Parse(stream_pair.first.c_str()) == XML_SUCCESS) {
      std::vector<DetectedObject> objects;
      processXmlDoc(doc, objects);
      if (!objects.empty()) {
        results.push_back({stream_pair.second, objects});
      }
    }
  }
  outFile.close();
  completed_streams.clear();
}

std::vector<MetadataResult> MetadataParser::getCompletedResults() {
  std::vector<MetadataResult> completed = std::move(results);
  results.clear();
  return completed;
}

void MetadataParser::cleanupBuffer() {
  const size_t MAX_BUFFER_SIZE = 512 * 1024;
  if (xml_buffer.size() > MAX_BUFFER_SIZE) {
    xml_buffer.clear();  // 너무 크면 그냥 비움 (안전장치)
  }
}
