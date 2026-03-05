#include "networkclient.h"

#include <QDebug>
#include <QSslConfiguration>
#include <QFile>
#include <QSslCertificate>

CameraImageProvider* g_cameraImageProvider = nullptr;

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent), m_isConnected(false),
      m_statusMessage("Ready to connect") {
  socket = new QSslSocket(this);
  socket->setReadBufferSize(0);

    // 1. 리소스에서 인증서 파일 읽기
    QFile certFile(":/assets/server.crt"); // 경로를 본인의 qrc 경로에 맞게 수정
    if (certFile.open(QIODevice::ReadOnly)) {
        QSslCertificate cert(&certFile, QSsl::Pem);

        QSslConfiguration sslConfig = socket->sslConfiguration();

        // 2. 이 인증서를 신뢰할 수 있는 CA 목록에 추가
        QList<QSslCertificate> caCerts = sslConfig.caCertificates();
        caCerts.append(cert);
        sslConfig.setCaCertificates(caCerts);

        // 3. 신뢰하는 대상만 연결 허용 (VerifyPeer로 변경)
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
        socket->setSslConfiguration(sslConfig);

        qDebug() << "🔒 Local certificate loaded and trusted.";
    } else {
        qDebug() << "❌ Failed to load certificate file!";
    }

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

bool NetworkClient::isConnected() const { return m_isConnected; }

QString NetworkClient::statusMessage() const { return m_statusMessage; }

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

    // 3. SSL 에러를 무시하도록 미리 설정 (영상 패킷 수신 시 끊김 방지)
    // 이 코드가 있어야 대용량 데이터 전송 시 SSL 경고로 인해 끊기는 것을 막습니다.
    socket->ignoreSslErrors();

    qDebug() << "🌐 Attempting to connect to" << host << ":" << port << "with TLS...";
    setStatus("Connecting to sensitive server...");

    socket->connectToHostEncrypted(host, port);
    socket->ignoreSslErrors();
}

void NetworkClient::disconnectFromServer() { socket->disconnectFromHost(); }

void NetworkClient::onEncrypted() {
  qDebug() << "✅ TLS Handshake complete!";
  QString certInfo = socket->peerCertificate()
                         .subjectInfo(QSslCertificate::CommonName)
                         .join(", ");
  qDebug() << "🔒 Peer Certificate:" << certInfo;
  setStatus("🔒 Encrypted Connection Established: " + certInfo);
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
  QString errMsg = "SSL Errors: ";
  for (const QSslError &err : errors) {
    errMsg += err.errorString() + "; ";
  }
  qDebug() << errMsg;

  // For development/demo purposes ONLY: Ignore self-signed cert errors
  // In production, you should handle this properly!
  socket->ignoreSslErrors(errors);
  setStatus("⚠️ TLS Error ignored (Self-signed?)");
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
      if (m_buffer.size() < 4) break;
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
      if (m_buffer.size() < headerSize) break;

      // 4. 헤더 읽기
      CamProtocol::PacketHeader header;
      memcpy(&header, m_buffer.constData(), headerSize);

      uint32_t magic      = qFromBigEndian<uint32_t>(header.magic);
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
          qDebug() << "❌ Abnormal packet size! json:" << json_size << "img:" << image_size;
          m_buffer.remove(0, 4); // 이 매직 쿠키를 건너뛰고 다음 탐색
          continue;
      }

      if (m_buffer.size() < (int)total_size) break;
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

          QVariantMap metadata;
          if (!json_data.isEmpty())
              metadata = QJsonDocument::fromJson(json_data).object().toVariantMap();

          emit cameraFrameReceived(cam_id,
                                   QString::number(QDateTime::currentMSecsSinceEpoch()),
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
      if (nextMagic > 0) m_buffer.remove(0, nextMagic);
      else m_buffer.clear();
  }
}

void NetworkClient::processJsonResponse(const QByteArray &line) {
  QJsonDocument jsonDoc = QJsonDocument::fromJson(line);
  if (jsonDoc.isNull() || !jsonDoc.isObject()) {
    qDebug() << "Invalid JSON received:" << line;
    return;
  }

  QJsonObject jsonObj = jsonDoc.object();
  QString type = jsonObj["type"].toString();
  QJsonValue dataVal = jsonObj["data"];

  if (type == "realtime") {
    processRealtimeData(dataVal.toArray());
  } else if (type == "realtime_air") {
    // Robust handling: handle both Array and Single Object
    if (dataVal.isArray()) {
      processRealtimeAirData(dataVal.toArray());
    } else if (dataVal.isObject()) {
      QJsonArray arr;
      arr.append(dataVal);
      processRealtimeAirData(arr);
    } else {
      // Try parsing root object if "data" is missing but type matches
      processRealtimeAirData(QJsonArray{jsonObj});
    }
  } else if (type == "air_stats") {
    processAirStatsData(dataVal.toArray());
  } else if (type == "flow_stats") {
    processFlowStatsData(dataVal.toArray());
  } else if (type == "system_monitor") {
    processSystemMonitorData(jsonObj);
  } else if (type == "temp_humi") {
    emit tempHumiReceived(jsonObj.toVariantMap());
  }
}

void NetworkClient::processRealtimeData(const QJsonArray &data) {
  QJsonArray modifiedData;
  for (const QJsonValue &val : data) {
    QJsonObject obj = val.toObject();
    int count = obj["count"].toInt();
    QString status;
    if (count < 50) {
      status = "여유";
    } else if (count < 100) {
      status = "보통";
    } else {
      status = "혼잡";
    }
    obj["status"] = status;
    modifiedData.append(obj);
  }
  emit realtimeDataReceived(modifiedData.toVariantList());
}

void NetworkClient::processRealtimeAirData(const QJsonArray &data) {
  if (data.isEmpty())
    return;

  // Find the most recent record by recorded_at across all stations
  QJsonObject latestObj;
  QString latestTime = "";

  for (const QJsonValue &val : data) {
    QJsonObject obj = val.toObject();
    // Support multiple field names for timestamp
    QString recordedAt = obj.contains("recorded_at")
                             ? obj["recorded_at"].toString()
                             : obj["timestamp"].toString();

    if (latestTime.isEmpty() ||
        (recordedAt > latestTime && !recordedAt.isEmpty())) {
      latestTime = recordedAt;
      latestObj = obj;
    }
  }

  if (latestObj.isEmpty())
    latestObj = data.at(0).toObject();

  // Support both "co2_ppm" (new) and "toxic_gas_level" (old/backup)
  double co2 = 0.0;
  if (latestObj.contains("co2_ppm")) {
    co2 = latestObj["co2_ppm"].toDouble();
  } else if (latestObj.contains("toxic_gas_level")) {
    co2 = latestObj["toxic_gas_level"].toDouble();
  } else if (latestObj.contains("co2")) {
    co2 = latestObj["co2"].toDouble();
  }

  double co = latestObj["co_level"].toDouble();

  // Ensuring both co2_ppm and co_level are updated in the map for UI
  // consistency
  latestObj["co2_ppm"] = co2;
  latestObj["co_level"] = co;

  // LOG: Print detailed debug information to identify value discrepancies
  qDebug() << "-----------------------------------------";
  qDebug() << "📡 [NETWORK_DATA] Realtime Air Update";
  qDebug() << "📍 Station: " << latestObj["station"].toString();
  qDebug() << "🕙 Recorded At: " << latestTime;
  qDebug() << "🌫️ CO2: " << co2 << " (raw_ppm)";
  qDebug() << "🌫️ CO:  " << co << " (raw_level)";
  qDebug() << "-----------------------------------------";

  // Ensure both field names are present in the map for UI compatibility
  latestObj["co2_ppm"] = co2;
  latestObj["co_level"] = co;

  // Calculate status strings for the UI
  QString coStatus;
  if (co < 9.0)
    coStatus = "🟢 양호";
  else if (co < 25.0)
    coStatus = "🟡 주의";
  else
    coStatus = "🔴 위험";

  QString gasStatus;
  if (co2 < 400.0)
    gasStatus = "🟢 양호";
  else if (co2 < 600.0)
    gasStatus = "🟡 주의";
  else
    gasStatus = "🔴 위험";

  latestObj["co_status"] = coStatus;
  latestObj["gas_status"] = gasStatus;

  emit realtimeAirReceived(latestObj.toVariantMap());
}

void NetworkClient::processAirStatsData(const QJsonArray &data) {
  QJsonArray modifiedData;
  for (const QJsonValue &val : data) {
    QJsonObject obj = val.toObject();
    double co = obj["co"].toDouble();
    double co2 = obj["co2"].toDouble(); // Server changed "gas" to "co2"

    // CO status: <9.0 (Good), <25.0 (Moderate), >=25.0 (Poor)
    QString coStatus;
    if (co < 9.0) {
      coStatus = "🟢 양호";
    } else if (co < 25.0) {
      coStatus = "🟡 주의";
    } else {
      coStatus = "🔴 위험";
    }

    // CO2 (Gas) status: <400 (Good), <600 (Moderate), >=600 (Poor)
    QString gasStatus;
    if (co2 < 400.0) {
      gasStatus = "🟢 양호";
    } else if (co2 < 600.0) {
      gasStatus = "🟡 주의";
    } else {
      gasStatus = "🔴 위험";
    }

    obj["co_status"] = coStatus;
    obj["gas_status"] = gasStatus;
    obj["gas"] =
        co2; // Map server "co2" back to QML "gas" for backward compatibility
    modifiedData.append(obj);
  }
  emit airStatsReceived(modifiedData.toVariantList());
}

void NetworkClient::processFlowStatsData(const QJsonArray &data) {
  QJsonArray modifiedData;
  for (const QJsonValue &val : data) {
    QJsonObject obj = val.toObject();
    int count = obj["avg_count"].toInt();
    QString status;
    if (count < 50) {
      status = "여유";
    } else if (count < 100) {
      status = "보통";
    } else {
      status = "혼잡";
    }
    obj["status"] = status;
    modifiedData.append(obj);
  }
  emit flowStatsReceived(modifiedData.toVariantList());
}

void NetworkClient::processSystemMonitorData(const QJsonObject &obj) {
  qDebug() << "🖥️ [NETWORK_DATA] System Monitor Update Received";
  emit systemMonitorReceived(obj.toVariantMap());
}

void NetworkClient::sendDeviceCommand(const QString &device,
                                      const QString &action) {
  if (socket->state() != QAbstractSocket::ConnectedState) {
    setStatus("Cannot send command: not connected");
    return;
  }

  QJsonObject obj;
  obj["type"] = "device_command";
  QJsonObject data;
  data["device"] = device;
  data["action"] = action;
  obj["data"] = data;

  QJsonDocument doc(obj);
  QByteArray bytes = doc.toJson(QJsonDocument::Compact) + "\n";
  socket->write(bytes);
  socket->flush();
  setStatus(QString("Sent command: %1 -> %2").arg(device, action));
}

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
