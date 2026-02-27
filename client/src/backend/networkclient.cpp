#include "networkclient.h"

#include <QDebug>

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent), m_isConnected(false),
      m_statusMessage("Ready to connect") {
  socket = new QSslSocket(this);

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
  if (socket->state() != QAbstractSocket::UnconnectedState) {
    qDebug() << "🔄 Current socket state:" << socket->state()
             << ". Disconnecting first...";
    socket->disconnectFromHost();
  }

  qDebug() << "🌐 Attempting to connect to" << host << ":" << port
           << "with TLS...";
  setStatus("Connecting to sensitive server...");
  socket->connectToHostEncrypted(host, port);
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
  qDebug() << "Socket disconnected";
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
  socket->ignoreSslErrors();
  setStatus("⚠️ TLS Error ignored (Self-signed?)");
}

void NetworkClient::onErrorOccurred(QAbstractSocket::SocketError socketError) {
  Q_UNUSED(socketError)
  setStatus("Error: " + socket->errorString());
}

#include <QtEndian>

void NetworkClient::readData() {
  QByteArray newData = socket->readAll();
  m_buffer.append(newData);

  // 패킷 헤더 시작 패턴 (\xDE\xAD\xBE\xEF in Big-Endian)
  static const QByteArray magicPattern = QByteArray::fromHex("DEADBEEF");

  while (m_buffer.size() >= 4) {
    int magicPos = m_buffer.indexOf(magicPattern);

    if (magicPos != -1) {
      // 1. 매직 쿠키를 찾음. 그 이전 데이터에 JSON 줄바꿈이 있는지 확인
      if (magicPos > 0) {
        QByteArray precedingData = m_buffer.left(magicPos);
        int lastNewline = precedingData.lastIndexOf('\n');
        if (lastNewline != -1) {
          QByteArray jsonData = precedingData.left(lastNewline + 1);
          for (const QByteArray &line : jsonData.split('\n')) {
            if (!line.trimmed().isEmpty())
              processJsonResponse(line.trimmed());
          }
        }
        m_buffer.remove(
            0, magicPos); // 매직 쿠키 이전의 쓰레기 데이터나 처리된 JSON 제거
      }

      // 이제 m_buffer는 매직 쿠키로 시작함
      if (m_buffer.size() <
          static_cast<int>(sizeof(CamProtocol::PacketHeader))) {
        break; // 헤더가 다 올 때까지 대기
      }

      CamProtocol::PacketHeader header;
      memcpy(&header, m_buffer.constData(), sizeof(header));
      uint32_t cam_id = qFromBigEndian<uint32_t>(header.camera_id);
      uint32_t json_size = qFromBigEndian<uint32_t>(header.json_size);
      uint32_t image_size = qFromBigEndian<uint32_t>(header.image_size);
      int total_size = sizeof(header) + json_size + image_size;

      if (m_buffer.size() >= total_size) {
        int offset = sizeof(header);
        QByteArray json_data = m_buffer.mid(offset, json_size);
        offset += json_size;
        QByteArray img_data = m_buffer.mid(offset, image_size);

        QVariantMap metadata;
        if (!json_data.isEmpty()) {
          QJsonDocument metaDoc = QJsonDocument::fromJson(json_data);
          if (!metaDoc.isNull() && metaDoc.isObject()) {
            metadata = metaDoc.object().toVariantMap();
          }
        }

        emit cameraFrameReceived(cam_id, img_data.toBase64(), metadata);
        m_buffer.remove(0, total_size);
        continue; // 다음 패킷 처리
      } else {
        break; // 전체 페이로드가 올 때까지 대기
      }
    } else {
      // 2. 매직 쿠키를 못 찾음. 버퍼 내에 단순 JSON이 있는지 확인
      int lastNewline = m_buffer.lastIndexOf('\n');
      if (lastNewline != -1) {
        QByteArray jsonData = m_buffer.left(lastNewline + 1);
        for (const QByteArray &line : jsonData.split('\n')) {
          if (!line.trimmed().isEmpty())
            processJsonResponse(line.trimmed());
        }
        m_buffer.remove(0, lastNewline + 1);
      }

      // 쿠키도 없고 줄바꿈도 없는데 버퍼만 크다면(쓰레기 데이터) 정리
      if (m_buffer.size() > 1024 * 1024) {
        qDebug() << "⚠️ Buffer overflow protection triggered. Clearing...";
        m_buffer.clear();
      }
      break;
    }
  }
}

void NetworkClient::processJsonResponse(const QByteArray &line) {
  QJsonDocument jsonDoc = QJsonDocument::fromJson(line);
  if (jsonDoc.isNull() || !jsonDoc.isObject()) {
    return;
  }

  QJsonObject jsonObj = jsonDoc.object();
  QString type = jsonObj["type"].toString();
  QJsonValue dataVal = jsonObj["data"];

  if (type == "realtime") {
    processRealtimeData(dataVal.toArray());
  } else if (type == "realtime_air") {
    if (dataVal.isArray()) {
      processRealtimeAirData(dataVal.toArray());
    } else if (dataVal.isObject()) {
      QJsonArray arr;
      arr.append(dataVal);
      processRealtimeAirData(arr);
    } else {
      processRealtimeAirData(QJsonArray{jsonObj});
    }
  } else if (type == "air_stats") {
    processAirStatsData(dataVal.toArray());
  } else if (type == "flow_stats") {
    processFlowStatsData(dataVal.toArray());
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
