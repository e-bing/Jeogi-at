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
    socket->disconnectFromHost();
  }

  setStatus("Connecting to sensitive server...");
  socket->connectToHostEncrypted(host, port);
}

void NetworkClient::disconnectFromServer() { socket->disconnectFromHost(); }

void NetworkClient::onEncrypted() {
  qDebug() << "TLS Handshake complete";
  QString certInfo = socket->peerCertificate()
                         .subjectInfo(QSslCertificate::CommonName)
                         .join(", ");
  setStatus("🔒 Encrypted Connection Established: " + certInfo);
  setIsConnected(true);
}

void NetworkClient::onConnected() {
  qDebug() << "Socket connected (waiting for encrypted)";
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

void NetworkClient::readData() {
  while (socket->canReadLine()) {
    QByteArray line = socket->readLine().trimmed();
    if (line.isEmpty())
      continue;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(line);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
      qDebug() << "Invalid JSON received:" << line;
      continue;
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
    }
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
