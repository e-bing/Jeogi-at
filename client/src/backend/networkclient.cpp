#include "networkclient.h"

#include <QDebug>

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

// ──────────────────────────────────────────────────────────────
//  공개 슬롯 (QML 호출용)
// ──────────────────────────────────────────────────────────────

void NetworkClient::connectToServer(const QString &host, quint16 port) {
  if (socket->state() != QAbstractSocket::UnconnectedState) {
    socket->disconnectFromHost();
  }
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

  // 개발/데모용: 자체 서명 인증서 에러 무시
  // 운영 환경에서는 반드시 인증서를 제대로 처리해야 합니다.
  socket->ignoreSslErrors();
  setStatus("⚠️ TLS Error ignored (Self-signed?)");
}

void NetworkClient::onErrorOccurred(QAbstractSocket::SocketError socketError) {
  Q_UNUSED(socketError)
  setStatus("Error: " + socket->errorString());
}

// ──────────────────────────────────────────────────────────────
//  데이터 수신 및 JSON 라우팅
// ──────────────────────────────────────────────────────────────

void NetworkClient::readData() {
  m_buffer += socket->readAll();

  while (true) {
    int nlPos = m_buffer.indexOf('\n');
    if (nlPos < 0)
      break;

    QByteArray line = m_buffer.left(nlPos).trimmed();
    m_buffer.remove(0, nlPos + 1);

    if (line.isEmpty())
      continue;

    // 유효한 JSON은 반드시 '{'로 시작 — 카메라 바이너리 패킷 무시
    if (!line.startsWith('{')) {
      qDebug() << "[readData] 바이너리 패킷 무시 (size:" << line.size() << ")";
      continue;
    }

    processJsonResponse(line);
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

  if (type == Protocol::MSG_REALTIME) {
    processRealtimeData(dataVal.toArray());

  } else if (type == Protocol::MSG_REALTIME_AIR) {
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

  } else if (type == Protocol::MSG_SYSTEM_MONITOR) {
    processSystemMonitorData(jsonObj);

  } else if (type == Protocol::MSG_TEMP_HUMI) {
    emit tempHumiReceived(jsonObj.toVariantMap());
  }
  // MSG_ZONE_CONGESTION 은 현재 Qt UI에서 미사용 — 필요 시 핸들러 추가
}

// ──────────────────────────────────────────────────────────────
//  데이터 파싱 헬퍼 (private)
// ──────────────────────────────────────────────────────────────

void NetworkClient::processRealtimeData(const QJsonArray &data) {
  QJsonArray result;
  for (const QJsonValue &val : data) {
    QJsonObject obj = val.toObject();
    int count = obj[Protocol::FIELD_COUNT].toInt();
    obj[Protocol::FIELD_STATUS] = congestionStatusString(count);
    result.append(obj);
  }
  emit realtimeDataReceived(result.toVariantList());
}

void NetworkClient::processRealtimeAirData(const QJsonArray &data) {
  if (data.isEmpty())
    return;

  // 가장 최신 recorded_at을 가진 레코드 선택
  QJsonObject latestObj;
  QString latestTime;

  for (const QJsonValue &val : data) {
    QJsonObject obj = val.toObject();
    // 서버가 recorded_at 또는 timestamp 필드를 사용하는 경우 모두 지원
    QString ts = obj.contains(Protocol::FIELD_RECORDED_AT)
                     ? obj[Protocol::FIELD_RECORDED_AT].toString()
                     : obj["timestamp"].toString();

    if (latestTime.isEmpty() || (!ts.isEmpty() && ts > latestTime)) {
      latestTime = ts;
      latestObj = obj;
    }
  }

  if (latestObj.isEmpty())
    latestObj = data.at(0).toObject();

  // CO2: co2_ppm → toxic_gas_level → co2 순서로 폴백
  double co2 = 0.0;
  if (latestObj.contains(Protocol::FIELD_CO2_PPM))
    co2 = latestObj[Protocol::FIELD_CO2_PPM].toDouble();
  else if (latestObj.contains("toxic_gas_level"))
    co2 = latestObj["toxic_gas_level"].toDouble();
  else if (latestObj.contains(Protocol::FIELD_CO2))
    co2 = latestObj[Protocol::FIELD_CO2].toDouble();

  double co = latestObj[Protocol::FIELD_CO_LEVEL].toDouble();

  // UI 일관성을 위해 두 필드 모두 확실히 설정
  latestObj[Protocol::FIELD_CO2_PPM] = co2;
  latestObj[Protocol::FIELD_CO_LEVEL] = co;

  qDebug() << "-----------------------------------------";
  qDebug() << "📡 [NETWORK_DATA] Realtime Air Update";
  qDebug() << "📍 Station:" << latestObj[Protocol::FIELD_STATION].toString();
  qDebug() << "🕙 Recorded At:" << latestTime;
  qDebug() << "🌫️ CO2:" << co2 << "(raw_ppm)";
  qDebug() << "🌫️ CO:" << co << "(raw_level)";
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
