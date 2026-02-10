#include <mqtt/async_client.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>
#include <vector>

#include "net.h"

using json = nlohmann::json;

// --- 설정값 ---
const std::string ADDRESS("tcp://localhost:1883");
const std::string DEVICE_ID = "pi01";
const std::string TOPIC = "iot/" + DEVICE_ID + "/sensor/camera";

// RTSP 송출용 GStreamer 파이프라인 (H.264 하드웨어 가속)
const std::string rtsp_pipeline = R"(
    appsrc ! 
    videoconvert ! 
    video/x-raw,format=I420,width=640,height=480,framerate=15/1 ! 
    x264enc tune=zerolatency bitrate=800 speed-preset=ultrafast key-int-max=15 ! 
    video/x-h264,profile=baseline,stream-format=byte-stream ! 
    h264parse config-interval=1 ! 
    tcpserversink host=0.0.0.0 port=5000 sync=false
)";

std::string get_timestamp() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
  return ss.str();
}

int main() {
  int frame_idx = 0;

  // 1. MQTT 클라이언트 초기화
  mqtt::async_client client(ADDRESS, DEVICE_ID);
  mqtt::connect_options connOpts;
  connOpts.set_clean_session(true);
  try {
    client.connect(connOpts)->wait();
  } catch (...) {
    std::cerr << "MQTT 연결 실패!" << std::endl;
    return -1;
  }

  // 2. NCNN 모델 로드 (YOLOv26/v8 Nano)
  ncnn::Net detector;
  detector.opt.num_threads = 4;
  detector.opt.use_fp16_packed = true;
  detector.opt.use_fp16_storage = true;
  detector.opt.use_fp16_arithmetic = true;
  detector.load_param("../models/model.ncnn.param");
  detector.load_model("../models/model.ncnn.bin");

  // 3. 카메라(Capture) 및 RTSP(Writer) 설정
  std::string in_pipeline =
      "libcamerasrc ! video/x-raw, width=640, height=480, framerate=30/1 ! "
      "videoconvert ! video/x-raw, format=BGR ! appsink drop=true";
  cv::VideoCapture cap(in_pipeline, cv::CAP_GSTREAMER);
  cv::VideoWriter writer(rtsp_pipeline, cv::CAP_GSTREAMER, 0, 15.0,
                         cv::Size(640, 480));

  if (!cap.isOpened()) {
    std::cerr << "카메라를 열 수 없습니다!" << std::endl;
    return -1;
  }
  if (!writer.isOpened()) {
    std::cerr
        << "GStreamer Writer(송출부)를 열 수 없습니다! 파이프라인을 확인하세요."
        << std::endl;
    return -1;
  }
  std::cout << "송출 준비 완료! 5000번 포트 대기 중..." << std::endl;

  while (true) {
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) break;

    if (frame_idx % 5 == 0) {
      // 4. AI 추론 (320x320 리사이징)
      ncnn::Mat in =
          ncnn::Mat::from_pixels_resize(frame.data, ncnn::Mat::PIXEL_BGR2RGB,
                                        frame.cols, frame.rows, 320, 320);
      const float norm_vals[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};
      in.substract_mean_normalize(0, norm_vals);

      ncnn::Extractor ex = detector.create_extractor();
      ex.input("in0", in);
      ncnn::Mat out;
      ex.extract("out0", out);

      // 5. 후처리 및 JSON 데이터 생성
      std::vector<cv::Rect> boxes;
      std::vector<float> confidences;
      json objects_array = json::array();
      float scale_w = 640.f / 320.f;
      float scale_h = 480.f / 320.f;

      for (int i = 0; i < out.w; i++) {
        float score = out.row(4)[i];
        if (score > 0.45f) {
          float w = out.row(2)[i] * scale_w;
          float h = out.row(3)[i] * scale_h;
          int x = (int)((out.row(0)[i] - out.row(2)[i] / 2) * scale_w);
          int y = (int)((out.row(1)[i] - out.row(3)[i] / 2) * scale_h);
          boxes.push_back(cv::Rect(x, y, (int)w, (int)h));
          confidences.push_back(score);
        }
      }

      std::vector<int> indices;
      cv::dnn::NMSBoxes(boxes, confidences, 0.45f, 0.5f, indices);

      for (int idx : indices) {
        objects_array.push_back({{"x", boxes[idx].x},
                                 {"y", boxes[idx].y},
                                 {"w", boxes[idx].width},
                                 {"h", boxes[idx].height},
                                 {"conf", confidences[idx]}});
      }

      // 6. MQTT 전송 (가이드 포맷 준수)
      json msg;
      msg["device_id"] = DEVICE_ID;
      msg["timestamp"] = get_timestamp();
      msg["sensor_type"] = "people_count";
      msg["value"] = indices.size();
      msg["unit"] = "person";
      msg["blocks"] = objects_array;  // 서버 시각화용 좌표 데이터 추가

      client.publish(TOPIC, msg.dump());
    }
    frame_idx++;

    // 7. RTSP 영상 송출 (원본 프레임 전송)
    writer.write(frame);
  }
  return 0;
}