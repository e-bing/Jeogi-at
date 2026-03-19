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

// --- MQTT 설정값 ---
const std::string ADDRESS("tcp://localhost:1883");
const std::string DEVICE_ID = "pi01";
const std::string TOPIC = "iot/" + DEVICE_ID + "/sensor/camera";

// --- 카메라 설정 ---
const int CAPTURE_W = 640;
const int CAPTURE_H = 480;
const int INFER_SIZE = 320;

// --- 추론 설정 ---
const unsigned int INFER_INTERVAL = 5;
const float SCORE_THRESHOLD = 0.45f;
const float NMS_THRESHOLD = 0.5f;

// --- 보정 설정 ---
const float CONTRAST_GAIN = 1.06f;
const float CONTRAST_BIAS = -7.0f;
const float SAT_SCALE = 0.90f;

struct LabCorrection {
  float scale, shift;
};
const LabCorrection lab_corr[3] = {
    {0.8474f, 40.8006f},  // L
    {0.9625f, 8.2722f},   // A
    {1.1875f, -25.4647f}  // B
};

std::string get_timestamp() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
  return ss.str();
}

int main() {
  unsigned int frame_idx = 0;

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
  const std::string in_pipeline = R"(
      libcamerasrc ! video/x-raw,width=640,height=480,framerate=30/1 !
      queue max-size-buffers=2 leaky=downstream !
      videoconvert ! video/x-raw,format=BGR ! appsink drop=true
)";

  const std::string out_pipeline = R"(
    appsrc !
    videoconvert !
    video/x-raw,format=I420,width=640,height=480,framerate=30/1 !
    v4l2h264enc extra-controls="controls,repeat_sequence_header=1,video_bitrate=1500000" !
    video/x-h264,profile=baseline,stream-format=byte-stream,level=(string)4 !
    h264parse config-interval=1 !
    tcpserversink host=0.0.0.0 port=5000 sync=false
)";
  cv::VideoCapture cap(in_pipeline, cv::CAP_GSTREAMER);
  cv::VideoWriter writer(out_pipeline, cv::CAP_GSTREAMER, 0, 30.0,
                         cv::Size(CAPTURE_W, CAPTURE_H));

  if (!cap.isOpened()) {
    std::cerr << "파이프라인을 열 수 없습니다!" << std::endl;
    return -1;
  }
  std::cout << "송출 준비 완료! 5000번 포트 대기 중..." << std::endl;

  // 3. 보정
  // 보정 mat 생성
  cv::Mat kernel = (cv::Mat_<float>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
  cv::Mat contrast_lut(1, 256, CV_8U);
  for (int i = 0; i < 256; i++)
    contrast_lut.at<uchar>(i) =
        cv::saturate_cast<uchar>(i * CONTRAST_GAIN + CONTRAST_BIAS);
  cv::Mat sat_lut(1, 256, CV_8U);
  for (int i = 0; i < 256; i++)
    sat_lut.at<uchar>(i) = cv::saturate_cast<uchar>(i * SAT_SCALE);

  std::vector<cv::Rect> boxes;
  std::vector<float> confidences;
  boxes.reserve(100);
  confidences.reserve(100);

  std::vector<cv::Mat> lab_ch(3);
  std::vector<cv::Mat> hsv_ch(3);
  cv::Mat lab, hsv, frame_enhanced;

  while (true) {
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) break;

    // 샤프닝
    cv::filter2D(frame, frame_enhanced, -1, kernel);

    // LAB 보정
    cv::cvtColor(frame_enhanced, lab, cv::COLOR_BGR2Lab);
    lab.convertTo(lab, CV_32F);
    cv::split(lab, lab_ch);
    for (int i = 0; i < 3; i++) {
      lab_ch[i] = lab_ch[i] * lab_corr[i].scale + lab_corr[i].shift;
      if (i == 1) lab_ch[i] -= 1.0f;
      lab_ch[i] = cv::min(cv::max(lab_ch[i], 0.f), 255.f);
    }
    cv::merge(lab_ch, lab);
    lab.convertTo(lab, CV_8U);
    cv::cvtColor(lab, frame_enhanced, cv::COLOR_Lab2BGR);

    // 색조/채도 보정
    cv::cvtColor(frame_enhanced, hsv, cv::COLOR_BGR2HSV);
    cv::split(hsv, hsv_ch);

    // 채도 0.90
    cv::LUT(hsv_ch[1], sat_lut, hsv_ch[1]);
    cv::merge(hsv_ch, hsv);
    cv::cvtColor(hsv, frame_enhanced, cv::COLOR_HSV2BGR);

    cv::LUT(frame_enhanced, contrast_lut, frame_enhanced);

    if (frame_idx % INFER_INTERVAL == 0) {
      // 4. AI 추론 (320x320 리사이징)
      ncnn::Mat in = ncnn::Mat::from_pixels_resize(
          frame_enhanced.data, ncnn::Mat::PIXEL_BGR2RGB, frame_enhanced.cols,
          frame_enhanced.rows, INFER_SIZE, INFER_SIZE);
      const float norm_vals[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};
      in.substract_mean_normalize(0, norm_vals);

      ncnn::Extractor ex = detector.create_extractor();
      ex.input("in0", in);
      ncnn::Mat out;
      ex.extract("out0", out);

      // 5. 후처리 및 JSON 데이터 생성
      boxes.clear();
      confidences.clear();
      json objects_array = json::array();
      float scale_w = (float)CAPTURE_W / INFER_SIZE;
      float scale_h = (float)CAPTURE_H / INFER_SIZE;

      for (int i = 0; i < out.w; i++) {
        float score = out.row(4)[i];
        if (score > SCORE_THRESHOLD) {
          float w = out.row(2)[i] * scale_w;
          float h = out.row(3)[i] * scale_h;
          int x = (int)((out.row(0)[i] - out.row(2)[i] / 2) * scale_w);
          int y = (int)((out.row(1)[i] - out.row(3)[i] / 2) * scale_h);
          boxes.push_back(cv::Rect(x, y, (int)w, (int)h));
          confidences.push_back(score);
        }
      }

      std::vector<int> indices;
      cv::dnn::NMSBoxes(boxes, confidences, SCORE_THRESHOLD, NMS_THRESHOLD,
                        indices);

      for (int idx : indices) {
        objects_array.push_back({{"x", boxes[idx].x / (float)CAPTURE_W},
                                 {"y", boxes[idx].y / (float)CAPTURE_H},
                                 {"w", boxes[idx].width / (float)CAPTURE_W},
                                 {"h", boxes[idx].height / (float)CAPTURE_H},
                                 {"conf", confidences[idx]}});
      }

      // 6. MQTT 전송
      json msg;
      msg["device_id"] = DEVICE_ID;
      msg["timestamp"] = get_timestamp();
      msg["sensor_type"] = "people_count";
      msg["value"] = indices.size();
      msg["unit"] = "person";
      msg["blocks"] = objects_array;

      auto pubmsg = mqtt::make_message(TOPIC, msg.dump());
      pubmsg->set_qos(0);
      client.publish(pubmsg);
    }
    frame_idx++;
    writer.write(frame_enhanced);
  }
  return 0;
}