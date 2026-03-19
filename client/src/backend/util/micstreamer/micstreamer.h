#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QAudioSource>
#include <QIODevice>
#include <QByteArray>

class MicStreamer : public QObject
{
    Q_OBJECT

public:
    explicit MicStreamer(QObject *parent = nullptr);

    Q_INVOKABLE bool startStreaming();
    Q_INVOKABLE void stopStreaming();
    Q_INVOKABLE bool isStreaming() const;

private slots:
    void onAudioReadyRead();

private:
    bool ensureConnected();

private:
    QTcpSocket *m_socket = nullptr;
    QAudioSource *m_audioSource = nullptr;
    QIODevice *m_audioDevice = nullptr;
    QByteArray m_captureBuffer;
    bool m_isStreaming = false;
    quint32 m_seq = 0;

    static constexpr int FRAME_BYTES = 1920;
};
