#ifndef RECORDINGMANAGER_H
#define RECORDINGMANAGER_H

#include <QDateTime>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QVariant>

class RecordingManager : public QObject {
    Q_OBJECT

public:
    explicit RecordingManager(QObject *parent = nullptr);

    Q_INVOKABLE void startRecording(int cameraId);
    Q_INVOKABLE void stopRecording(int cameraId);
    Q_INVOKABLE bool isRecording(int cameraId) const;
    Q_INVOKABLE QVariantList getRecordings(int cameraId) const;
    Q_INVOKABLE void deleteRecording(const QString &sessionPath);

public slots:
    void onFrameReceived(int cameraId, const QByteArray &jpegData);
    void shutdownAll();

signals:
    void recordingStateChanged(int cameraId, bool recording);
    void recordingsChanged(int cameraId);
    void recordingError(int cameraId, const QString &message);

private:
    QString camDir(int cameraId) const;
    QString findFfmpeg() const;
    void    launchFfmpeg(int cameraId, double fps);

    // 첫 N 프레임으로 카메라 fps 자동 측정 후 ffmpeg 시작
    static constexpr int CALIBRATION_SKIP   = 5;   // 초반 버스트 제외
    static constexpr int CALIBRATION_FRAMES = 60;  // 측정할 프레임 수

    QString m_rootPath;
    QMap<int, bool>             m_recording;
    QMap<int, bool>             m_manualStop;
    QMap<int, bool>             m_autoStartPending;
    QMap<int, bool>             m_calibrating;
    QMap<int, QList<QByteArray>> m_calibBuf;
    QMap<int, QDateTime>        m_calibFirstTime;
    QMap<int, QString>          m_sessionPaths;
    QMap<int, int>              m_frameCounts;
    QMap<int, QDateTime>        m_startTimes;
    QMap<int, QProcess*>        m_ffmpegProcesses;
    mutable QMutex m_mutex;
};

extern RecordingManager *g_recordingManager;

#endif // RECORDINGMANAGER_H
