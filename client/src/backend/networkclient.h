#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QByteArray>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QSize>
#include <QSslError>
#include <QSslSocket>
#include <QString>
#include <QVariant>
#include <cstdint>

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
#include "../../protocol/message_types.hpp"
#include "../../protocol/sensor_thresholds.hpp"

class NetworkClient : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
  Q_PROPERTY(
      QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
  Q_PROPERTY(int congestionEasyMax READ congestionEasyMax CONSTANT)
  Q_PROPERTY(int congestionNormalMax READ congestionNormalMax CONSTANT)

  // Device IDs
  Q_PROPERTY(QString DEVICE_MOTOR READ deviceMotor CONSTANT)
  Q_PROPERTY(QString DEVICE_SPEAKER READ deviceSpeaker CONSTANT)
  Q_PROPERTY(QString DEVICE_LIGHTING READ deviceLighting CONSTANT)
  Q_PROPERTY(QString DEVICE_DIGITAL_DISPLAY READ deviceDigitalDisplay CONSTANT)
  Q_PROPERTY(QString DEVICE_MODE_CONTROL READ deviceModeControl CONSTANT)

  // Action Values
  Q_PROPERTY(QString ACTION_ON READ actionOn CONSTANT)
  Q_PROPERTY(QString ACTION_OFF READ actionOff CONSTANT)
  Q_PROPERTY(QString ACTION_AUTO READ actionAuto CONSTANT)
  Q_PROPERTY(QString ACTION_MANUAL READ actionManual CONSTANT)

  // System Monitor Fields
  Q_PROPERTY(QString FIELD_CPU_USAGE READ fieldCpuUsage CONSTANT)
  Q_PROPERTY(QString FIELD_CPU_TEMP READ fieldCpuTemp CONSTANT)
  Q_PROPERTY(QString FIELD_DISK_USAGE READ fieldDiskUsage CONSTANT)
  Q_PROPERTY(QString FIELD_SERVER READ fieldServer CONSTANT)
  Q_PROPERTY(QString FIELD_FIRMWARE READ fieldFirmware CONSTANT)
  Q_PROPERTY(QString FIELD_CONNECTED READ fieldConnected CONSTANT)

public:
  explicit NetworkClient(QObject *parent = nullptr);
  ~NetworkClient();

  bool isConnected() const;
  QString statusMessage() const;
  int congestionEasyMax() const;
  int congestionNormalMax() const;

  // Getters for protocol constants
  QString deviceMotor() const { return Protocol::DEVICE_MOTOR; }
  QString deviceSpeaker() const { return Protocol::DEVICE_SPEAKER; }
  QString deviceLighting() const { return Protocol::DEVICE_LIGHTING; }
  QString deviceDigitalDisplay() const {
    return Protocol::DEVICE_DIGITAL_DISPLAY;
  }
  QString deviceModeControl() const { return Protocol::DEVICE_MODE_CONTROL; }

  QString actionOn() const { return Protocol::ACTION_ON; }
  QString actionOff() const { return Protocol::ACTION_OFF; }
  QString actionAuto() const { return Protocol::ACTION_AUTO; }
  QString actionManual() const { return Protocol::ACTION_MANUAL; }

  // System Monitor Fields Getters
  QString fieldCpuUsage() const { return Protocol::FIELD_CPU_USAGE; }
  QString fieldCpuTemp() const { return Protocol::FIELD_CPU_TEMP; }
  QString fieldDiskUsage() const { return Protocol::FIELD_DISK_USAGE; }
  QString fieldServer() const { return Protocol::FIELD_SERVER; }
  QString fieldFirmware() const { return Protocol::FIELD_FIRMWARE; }
  QString fieldConnected() const { return Protocol::FIELD_CONNECTED; }

  Q_INVOKABLE void connectToServer(const QString &host, quint16 port);
  Q_INVOKABLE void disconnectFromServer();
  Q_INVOKABLE void sendDeviceCommand(const QString &device,
                                     const QString &action);

signals:
  void isConnectedChanged();
  void statusMessageChanged();
  void airStatsReceived(QVariantList data);
  void realtimeAirReceived(QVariantMap data);
  void flowStatsReceived(QVariantList data);
  void zoneCongestionReceived(QVariantList zones, int totalCount);
  void cameraFrameReceived(int cameraId, const QString &timestamp,
                           const QVariantMap &metadata);
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

    QImage requestImage(
        const QString& id, QSize* size, const QSize&) override
    {
        int camId = id.split("?").first().toInt();
        QImage result;
        {
            QMutexLocker locker(&m_mutex);
            QImage& img = m_images[camId];
            if (!img.isNull())
            {
                m_prev_images[camId] = img;
                result = img.copy();
            }
            else
            {
                result = m_prev_images.value(camId).copy();
            }
        }
        if (size && !result.isNull()) *size = result.size();
        return result;
    }

    void updateImage(int cameraId, const QByteArray& jpegData) {
        QImage img;
        img.loadFromData(jpegData, "JPEG");
        {
            QMutexLocker locker(&m_mutex);
            m_images[cameraId] = std::move(img);
        }
    }

private:
  QMap<int, QImage> m_images;
  QMap<int, QImage> m_prev_images;
  QMutex m_mutex;
};

extern CameraImageProvider *g_cameraImageProvider;

#endif // NETWORKCLIENT_H
