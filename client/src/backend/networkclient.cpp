#include "networkclient.h"
#include "recordingmanager.h"
#include "sensor_thresholds.hpp"

#include <QDebug>
#include <QFile>
#include <QSslCertificate>
#include <QSslConfiguration>

CameraImageProvider *g_cameraImageProvider = nullptr;

// ──────────────────────────────────────────────────────────────
//  헬퍼: 센서 상태 문자열 반환
// ──────────────────────────────────────────────────────────────

static QString coStatusString(double co) {
  if (co < Protocol::CO_GOOD_MAX)
    return Protocol::STATUS_GOOD;
  if (co < Protocol::CO_CAUTION_MAX)
    return Protocol::STATUS_CAUTION;
  return Protocol::STATUS_DANGER;
}

static QString co2StatusString(double co2) {
  if (co2 < Protocol::CO2_GOOD_MAX)
    return Protocol::STATUS_GOOD;
  if (co2 < Protocol::CO2_CAUTION_MAX)
    return Protocol::STATUS_CAUTION;
  return Protocol::STATUS_DANGER;
}

static QString congestionStatusString(int count) {
  if (count < Protocol::CONGESTION_EASY_MAX)
    return Protocol::CONGESTION_EASY;
  if (count < Protocol::CONGESTION_NORMAL_MAX)
    return Protocol::CONGESTION_NORMAL;
  return Protocol::CONGESTION_BUSY;
}

// ──────────────────────────────────────────────────────────────
//  생성자 / 소멸자
// ──────────────────────────────────────────────────────────────

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent), m_isConnected(false),
      m_statusMessage("Ready to connect") {
  socket = new QSslSocket(this);
  socket->setReadBufferSize(0);

  // 로컬 인증서 로드 로직은 제외합니다 (qrc에 없는 파일 에러 발생 방지)
  socket->ignoreSslErrors();
  connect(socket, &QSslSocket::encrypted, this, &NetworkClient::onEncrypted);
  connect(socket, &QSslSocket::connected, this, &NetworkClient::onConnected);
  connect(socket, &QSslSocket::disconnected, this,
          &NetworkClient::onDisconnected);
  connect(socket, &QSslSocket::readyRead, this, &NetworkClient::readData);
  connect(socket,
          QOverload<const QList<QSslError> &>::of(&QSslSocket::sslErrors), this,
          &NetworkClient::onSslErrors);
  connect(socket, &QAbstractSocket::errorOccurred, this,
          &NetworkClient::onErrorOccurred);
}

NetworkClient::~NetworkClient() {
  if (socket->state() != QAbstractSocket::UnconnectedState) {
    socket->disconnectFromHost();
  }
}

// ──────────────────────────────────────────────────────────────
//  프로퍼티 접근자
// ──────────────────────────────────────────────────────────────

bool NetworkClient::isConnected() const { return m_isConnected; }
QString NetworkClient::statusMessage() const { return m_statusMessage; }

int NetworkClient::congestionEasyMax() const {
  return Protocol::CONGESTION_EASY_MAX;
}
int NetworkClient::congestionNormalMax() const {
  return Protocol::CONGESTION_NORMAL_MAX;
}

// ──────────────────────────────────────────────────────────────
//  공개 슬롯 (QML 호출용)
// ──────────────────────────────────────────────────────────────

void NetworkClient::connectToServer(const QString &host, quint16 port) {
  // 1. 이미 연결 중이거나 연결된 상태면 중복 요청 무시
  if (socket->state() == QAbstractSocket::ConnectingState ||
      socket->state() == QAbstractSocket::ConnectedState) {
    qDebug() << "⚠️ Already connecting or connected. Ignoring request.";
    return;
  }

  // 2. 만약 에러 상태 등으로 지저분하게 남아있다면 강제 종료(abort)
  if (socket->state() != QAbstractSocket::UnconnectedState) {
    socket->abort();
  }

  // 3. server.crt를 CA로 등록하여 서버 인증서 검증
  QFile certFile(":/assets/server.crt");
  if (!certFile.open(QIODevice::ReadOnly)) {
    qDebug() << "❌ server.crt 로드 실패";
    setStatus("Error: server.crt not found");
    return;
  }
  QSslCertificate caCert(&certFile, QSsl::Pem);
  certFile.close();

  QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
  sslConfig.setCaCertificates({caCert});
  sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
  socket->setSslConfiguration(sslConfig);

  qDebug() << "🌐 Attempting to connect to" << host << ":" << port
           << "with TLS...";
  setStatus("Connecting to server...");

  socket->connectToHostEncrypted(host, port);
}

void NetworkClient::disconnectFromServer() { socket->disconnectFromHost(); }

void NetworkClient::sendDeviceCommand(const QString &device,
                                      const QString &action) {
  if (socket->state() != QAbstractSocket::ConnectedState) {
    setStatus("Cannot send command: not connected");
    return;
  }

  QJsonObject data;
  data[Protocol::FIELD_DEVICE] = device;
  data[Protocol::FIELD_ACTION] = action;

  QJsonObject msg;
  msg[Protocol::FIELD_TYPE] = Protocol::MSG_DEVICE_COMMAND;
  msg[Protocol::FIELD_DATA] = data;

  QByteArray bytes = QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n";
  socket->write(bytes);
  socket->flush();
  setStatus(QString("Sent command: %1 -> %2").arg(device, action));
}

// ──────────────────────────────────────────────────────────────
//  소켓 이벤트 핸들러 (private slots)
// ──────────────────────────────────────────────────────────────

void NetworkClient::onEncrypted() {
  qDebug() << "✅ TLS Handshake complete!";
  QString certInfo = socket->peerCertificate()
                         .subjectInfo(QSslCertificate::CommonName)
                         .join(", ");
  qDebug() << "🔒 Peer Certificate:" << certInfo;
  setStatus("Connection Established: " + certInfo);
  setIsConnected(true);
}

void NetworkClient::onConnected() {
  qDebug() << "📡 Socket state: Connected. Initiating TLS Handshake...";
}

void NetworkClient::onDisconnected() {
  qDebug() << "Socket disconnected, last error:" << socket->errorString()
           << "state:" << socket->state();
  setStatus("❌ Disconnected from server");
  setIsConnected(false);
}

void NetworkClient::onSslErrors(const QList<QSslError> &errors) {
  QString errMsg;
  for (const QSslError &err : errors) {
    errMsg += err.errorString() + "; ";
  }
  qDebug() << "❌ SSL Error:" << errMsg;
  setStatus("SSL Error: " + errMsg);
  socket->abort();
}

void NetworkClient::onErrorOccurred(QAbstractSocket::SocketError socketError) {
  // Q_UNUSED(socketError)
  qDebug() << "❌ Socket Error Details:" << socket->errorString()
           << "Code:" << socketError;
  setStatus("Error: " + socket->errorString());
}

#include <QtEndian>

void NetworkClient::readData() {
  QByteArray newData = socket->readAll();
  m_buffer.append(newData);

  // 패킷 헤더 시작 패턴 (\xDE\xAD\xBE\xEF in Big-Endian)
  static const QByteArray magicPattern = QByteArray::fromHex("DEADBEEF");
  const int headerSize = sizeof(CamProtocol::PacketHeader);

  while (!m_buffer.isEmpty()) {
    // 1. 매직 쿠키 위치 찾기
    if (m_buffer.size() < 4)
      break;
    int magicPos = m_buffer.indexOf(magicPattern);

    if (magicPos == -1) {
      // 매직 쿠키가 없음 - 버퍼 전체가 쓰레기, 비움
      m_buffer.clear();
      break;
    }

    if (magicPos > 0) {
      // 매직 쿠키가 앞에 없음 - 앞부분 쓰레기 제거
      m_buffer.remove(0, magicPos);
      continue;
    }

    // 3. 최소한 헤더만큼은 데이터가 있어야 함
    if (m_buffer.size() < headerSize)
      break;

    // 4. 헤더 읽기
    CamProtocol::PacketHeader header;
    memcpy(&header, m_buffer.constData(), headerSize);

    uint32_t magic = qFromBigEndian<uint32_t>(header.magic);
    uint32_t cam_id = qFromBigEndian<uint32_t>(header.camera_id);
    uint32_t json_size = qFromBigEndian<uint32_t>(header.json_size);
    uint32_t image_size = qFromBigEndian<uint32_t>(header.image_size);
    uint32_t total_size = headerSize + json_size + image_size;

    // 4. 매직 쿠키 재검증 (혹시 모를 오파싱 방어)
    if (magic != CamProtocol::MAGIC_COOKIE) {
      m_buffer.remove(0, 1);
      continue;
    }

    // 5. 비정상 패킷 사이즈 방어
    if (json_size > 1000000 || image_size > 10000000) {
      qDebug() << "❌ Abnormal packet size! json:" << json_size
               << "img:" << image_size;
      m_buffer.remove(0, 4); // 이 매직 쿠키를 건너뛰고 다음 탐색
      continue;
    }

    if (m_buffer.size() < (int)total_size)
      break;
    QByteArray json_data = m_buffer.mid(headerSize, json_size);

    if (cam_id == 0) {
      // JSON 전용 패킷
      processJsonResponse(json_data);
    } else {
      // 카메라 패킷
      QByteArray img_data = m_buffer.mid(headerSize + json_size, image_size);
      if (g_cameraImageProvider) {
        g_cameraImageProvider->updateImage(cam_id, img_data);
      }
      if (g_recordingManager) {
        g_recordingManager->onFrameReceived(cam_id, img_data);
      }

      QVariantMap metadata;
      if (!json_data.isEmpty())
        metadata = QJsonDocument::fromJson(json_data).object().toVariantMap();

      emit cameraFrameReceived(
          cam_id, QString::number(QDateTime::currentMSecsSinceEpoch()),
          metadata);
    }

    // 8. 처리 완료된 패킷만큼 버퍼에서 제거
    m_buffer.remove(0, total_size);

    if (m_buffer.size() > 10 * 1024 * 1024) {
      qDebug() << "⚠️ BUFFER OVERFLOW! Clearing 10MB...";
      m_buffer.clear();
    }
    // 9. 다음 패킷이 연달아 와있을 수 있으므로 계속 루프
  }

  // 버퍼 오버플로우 방지 (매우 큰 비정상 데이터 대비)
  if (m_buffer.size() > 10 * 1024 * 1024) {
    int nextMagic = m_buffer.indexOf(magicPattern, 4); // 현재 위치 이후
    if (nextMagic > 0)
      m_buffer.remove(0, nextMagic);
    else
      m_buffer.clear();
  }
}

void NetworkClient::processJsonResponse(const QByteArray &line) {
  QJsonDocument jsonDoc = QJsonDocument::fromJson(line);
  if (jsonDoc.isNull() || !jsonDoc.isObject()) {
    qDebug() << "Invalid JSON received:" << line;
    return;
  }

  const QJsonObject jsonObj = jsonDoc.object();
  const QString type = jsonObj[Protocol::FIELD_TYPE].toString();
  const QJsonValue dataVal = jsonObj[Protocol::FIELD_DATA];

  if (type == Protocol::MSG_REALTIME_AIR) {
    // 서버가 배열 또는 단일 객체로 보낼 수 있으므로 양쪽 모두 처리
    if (dataVal.isArray()) {
      processRealtimeAirData(dataVal.toArray());
    } else if (dataVal.isObject()) {
      processRealtimeAirData(QJsonArray{dataVal});
    } else {
      processRealtimeAirData(QJsonArray{jsonObj});
    }

  } else if (type == Protocol::MSG_AIR_STATS) {
    processAirStatsData(dataVal.toArray());

  } else if (type == Protocol::MSG_FLOW_STATS) {
    processFlowStatsData(dataVal.toArray());

  } else if (type == "temp_humi_stats") {
    processTempHumiStatsData(dataVal.toArray());

  } else if (type == Protocol::MSG_SYSTEM_MONITOR) {
    processSystemMonitorData(jsonObj);

  } else if (type == Protocol::MSG_TEMP_HUMI) {
    emit tempHumiReceived(jsonObj.toVariantMap());

  } else if (type == Protocol::MSG_ZONE_CONGESTION) {
    QJsonArray zonesArr = jsonObj[Protocol::FIELD_ZONES].toArray();
    int totalCount = jsonObj[Protocol::FIELD_TOTAL_COUNT].toInt();
    QJsonArray zoneCountsArr = jsonObj[Protocol::FIELD_ZONE_COUNTS].toArray();
    emit zoneCongestionReceived(zonesArr.toVariantList(), totalCount, zoneCountsArr.toVariantList());
  }
}

// ──────────────────────────────────────────────────────────────
//  데이터 파싱 헬퍼 (private)
// ──────────────────────────────────────────────────────────────

void NetworkClient::processRealtimeAirData(const QJsonArray &data) {
  if (data.isEmpty())
    return;

  // 가장 최신 recorded_at을 가진 레코드 선택 (비교 로직 제거, 첫 번째 항목 사용)
  QJsonObject latestObj = data.at(0).toObject();
  QString latestTime = latestObj.contains(Protocol::FIELD_RECORDED_AT)
                           ? latestObj[Protocol::FIELD_RECORDED_AT].toString()
                           : latestObj["timestamp"].toString();

  // CO2: co2_ppm → toxic_gas_level → co2 순서로 폴백
  double co2 = 0.0;
  if (latestObj.contains(Protocol::FIELD_CO2_PPM))
    co2 = latestObj[Protocol::FIELD_CO2_PPM].toDouble();
  else if (latestObj.contains("toxic_gas_level"))
    co2 = latestObj["toxic_gas_level"].toDouble();
  else if (latestObj.contains(Protocol::FIELD_CO2))
    co2 = latestObj[Protocol::FIELD_CO2].toDouble();

  double co = latestObj[Protocol::FIELD_CO_LEVEL].toDouble();

  // 온습도 필드 매핑 및 신호 발생 (Dashboard.qml 호환성)
  if (latestObj.contains(Protocol::FIELD_TEMP) ||
      latestObj.contains(Protocol::FIELD_HUMI)) {
    QVariantMap thMap;
    double temp = latestObj[Protocol::FIELD_TEMP].toDouble();
    double humi = latestObj[Protocol::FIELD_HUMI].toDouble();
    thMap["temperature"] = temp;
    thMap["humidity"] = humi;
    emit tempHumiReceived(thMap);
  }

  // UI 일관성을 위해 필드 설정
  latestObj[Protocol::FIELD_CO2_PPM] = co2;
  latestObj[Protocol::FIELD_CO_LEVEL] = co;

  qDebug() << "-----------------------------------------";
  qDebug() << "📡 [NETWORK_DATA] Realtime Air Update";
  qDebug() << "📍 Station:" << latestObj[Protocol::FIELD_STATION].toString();
  qDebug() << "🕙 Recorded At:" << latestTime;
  qDebug() << "🌫️ CO2:" << co2 << "(raw_ppm)";
  qDebug() << "🌫️ CO:" << co << "(raw_level)";
  qDebug() << "🌡️ Temp:" << latestObj[Protocol::FIELD_TEMP].toDouble() << "°C";
  qDebug() << "💧 Humi:" << latestObj[Protocol::FIELD_HUMI].toDouble() << "%";
  qDebug() << "-----------------------------------------";

  latestObj[Protocol::FIELD_CO_STATUS] = coStatusString(co);
  latestObj[Protocol::FIELD_GAS_STATUS] = co2StatusString(co2);

  emit realtimeAirReceived(latestObj.toVariantMap());
}

void NetworkClient::processAirStatsData(const QJsonArray &data) {
  QJsonArray result;
  for (const QJsonValue &val : data) {
    QJsonObject obj = val.toObject();
    double co = obj[Protocol::FIELD_CO].toDouble();
    double co2 = obj[Protocol::FIELD_CO2].toDouble();

    obj[Protocol::FIELD_CO_STATUS] = coStatusString(co);
    obj[Protocol::FIELD_GAS_STATUS] = co2StatusString(co2);

    // QML 하위호환: "gas" 필드로도 co2 값 접근 가능하도록 유지
    obj[Protocol::FIELD_GAS] = co2;

    result.append(obj);
  }
  emit airStatsReceived(result.toVariantList());
}

void NetworkClient::processFlowStatsData(const QJsonArray &data) {
  QJsonArray result;
  for (const QJsonValue &val : data) {
    QJsonObject obj = val.toObject();
    int count = obj[Protocol::FIELD_AVG_COUNT].toInt();
    obj[Protocol::FIELD_STATUS] = congestionStatusString(count);
    result.append(obj);
  }
  emit flowStatsReceived(result.toVariantList());
}

void NetworkClient::processTempHumiStatsData(const QJsonArray &data) {
  QJsonArray result;
  for (const QJsonValue &val : data) {
    result.append(val.toObject());
  }
  emit tempHumiStatsReceived(result.toVariantList());
}

void NetworkClient::processSystemMonitorData(const QJsonObject &obj) {
  qDebug() << "🖥️ [NETWORK_DATA] System Monitor Update Received";
  emit systemMonitorReceived(obj.toVariantMap());
}

// ──────────────────────────────────────────────────────────────
//  내부 상태 세터
// ──────────────────────────────────────────────────────────────

void NetworkClient::setStatus(const QString &message) {
  if (m_statusMessage != message) {
    m_statusMessage = message;
    emit statusMessageChanged();
  }
}

void NetworkClient::setIsConnected(bool connected) {
  if (m_isConnected != connected) {
    m_isConnected = connected;
    emit isConnectedChanged();
  }
}
