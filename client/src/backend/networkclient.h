#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSslError>
#include <QSslSocket>
#include <QVariant>

namespace CamProtocol {
const uint32_t MAGIC_COOKIE = 0xDEADBEEF;

#pragma pack(push, 1)
struct PacketHeader {
  uint32_t magic;
  uint32_t camera_id;
  uint32_t json_size;
  uint32_t image_size;
};
#pragma pack(pop)
} // namespace CamProtocol

class NetworkClient : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
  Q_PROPERTY(
      QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
  explicit NetworkClient(QObject *parent = nullptr);
  ~NetworkClient();

  bool isConnected() const;
  QString statusMessage() const;

  Q_INVOKABLE void connectToServer(const QString &host, quint16 port);
  Q_INVOKABLE void disconnectFromServer();
  Q_INVOKABLE void sendDeviceCommand(const QString &device,
                                     const QString &action);

signals:
  void isConnectedChanged();
  void statusMessageChanged();
  void realtimeDataReceived(QVariantList data);
  void airStatsReceived(QVariantList data);
  void realtimeAirReceived(QVariantMap data);
  void flowStatsReceived(QVariantList data);
  void cameraFrameReceived(int cameraId, const QString &base64Image,
                           const QVariantMap &metadata);

private slots:
  void onEncrypted();
  void onConnected();
  void onDisconnected();
  void onSslErrors(const QList<QSslError> &errors);
  void readData();
  void onErrorOccurred(QAbstractSocket::SocketError socketError);

private:
  QSslSocket *socket;
  bool m_isConnected;
  QString m_statusMessage;

  void setStatus(const QString &message);
  void setIsConnected(bool connected);

  // Parsing helpers
  void processRealtimeData(const QJsonArray &data);
  void processRealtimeAirData(const QJsonArray &data);
  void processAirStatsData(const QJsonArray &data);
  void processFlowStatsData(const QJsonArray &data);
  void processJsonResponse(const QByteArray &line);

  QByteArray m_buffer;
};

#endif // NETWORKCLIENT_H
