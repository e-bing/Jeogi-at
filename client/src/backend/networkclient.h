#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QSslError>
#include <QSslSocket>
#include <QVariant>
#include <QQuickImageProvider>

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

// 공유 프로토콜 정의
#include "message_types.hpp"
#include "sensor_thresholds.hpp"

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
  void cameraFrameReceived(int cameraId, const QString &timestamp, const QVariantMap &metadata);
  void systemMonitorReceived(QVariantMap data);
  void tempHumiReceived(QVariantMap data);

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
  void processSystemMonitorData(const QJsonObject &obj);
  void processJsonResponse(const QByteArray &line);

  QByteArray m_buffer;
};

class CameraImageProvider : public QQuickImageProvider {
public:
    CameraImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString& id, QSize* size, const QSize&) override {
        QMutexLocker locker(&m_mutex);
        QImage img = m_images.value(id.split("?").first().toInt());
        if (size) *size = img.size();
        return img;
    }

    void updateImage(int cameraId, const QByteArray& jpegData) {
        QImage img;
        img.loadFromData(jpegData, "JPEG");
        QMutexLocker locker(&m_mutex);
        m_images[cameraId] = img;
    }

private:
    QMap<int, QImage> m_images;
    QMutex m_mutex;
};

extern CameraImageProvider* g_cameraImageProvider;

#endif // NETWORKCLIENT_H
