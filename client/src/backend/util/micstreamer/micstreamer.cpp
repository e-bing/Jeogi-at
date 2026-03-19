#include "MicStreamer.h"

#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QDataStream>
#include <QDebug>

MicStreamer::MicStreamer(QObject *parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, []() {
        qDebug() << "[TCP] connected";
    });

    connect(m_socket, &QTcpSocket::disconnected, this, []() {
        qDebug() << "[TCP] disconnected";
    });

    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        qWarning() << "[TCP] error:" << m_socket->errorString();
    });
}

bool MicStreamer::isStreaming() const
{
    return m_isStreaming;
}

bool MicStreamer::ensureConnected()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    qDebug() << "[TCP] connecting to 192.168.0.50:5000 ...";

    m_socket->abort();
    m_socket->connectToHost("192.168.0.103", 5000);

    if (!m_socket->waitForConnected(3000)) {
        qWarning() << "[TCP] connect failed:" << m_socket->errorString();
        return false;
    }

    qDebug() << "[TCP] connect success";
    return true;
}

bool MicStreamer::startStreaming()
{
    if (m_isStreaming)
        return true;

    if (!ensureConnected()) {
        qWarning() << "Cannot start streaming: TCP not connected";
        return false;
    }

    QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        qWarning() << "No default audio input device";
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!device.isFormatSupported(format)) {
        qWarning() << "Audio format not supported";
        qWarning() << "Preferred format =" << device.preferredFormat();
        return false;
    }

    m_captureBuffer.clear();
    m_seq = 0;

    m_audioSource = new QAudioSource(device, format, this);
    m_audioDevice = m_audioSource->start();

    if (!m_audioDevice) {
        qWarning() << "Failed to start audio source";
        delete m_audioSource;
        m_audioSource = nullptr;
        return false;
    }

    connect(m_audioDevice, &QIODevice::readyRead,
            this, &MicStreamer::onAudioReadyRead);

    m_isStreaming = true;
    qDebug() << "Broadcast started";
    return true;
}

void MicStreamer::stopStreaming()
{
    if (!m_isStreaming)
        return;

    if (m_audioDevice) {
        disconnect(m_audioDevice, &QIODevice::readyRead,
                   this, &MicStreamer::onAudioReadyRead);
        m_audioDevice = nullptr;
    }

    if (m_audioSource) {
        m_audioSource->stop();
        m_audioSource->deleteLater();
        m_audioSource = nullptr;
    }

    m_captureBuffer.clear();
    m_isStreaming = false;

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }

    qDebug() << "Broadcast stopped";
}

void MicStreamer::onAudioReadyRead()
{
    if (!m_audioDevice || !m_socket)
        return;

    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Socket is not connected";
        return;
    }

    QByteArray data = m_audioDevice->readAll();
    if (data.isEmpty())
        return;

    m_captureBuffer.append(data);

    while (m_captureBuffer.size() >= FRAME_BYTES)
    {
        QByteArray frame = m_captureBuffer.left(FRAME_BYTES);
        m_captureBuffer.remove(0, FRAME_BYTES);

        QByteArray packet;
        QDataStream out(&packet, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        quint16 sync = 0xAA55;
        quint16 payloadSize = static_cast<quint16>(frame.size());
        quint32 seq = m_seq++;

        out << sync << payloadSize << seq;
        packet.append(frame);

        qint64 written = m_socket->write(packet);
        if (written < 0) {
            qWarning() << "socket write failed:" << m_socket->errorString();
            break;
        }
    }

    m_socket->flush();
}
