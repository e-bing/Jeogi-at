#include "recordingmanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>

RecordingManager *g_recordingManager = nullptr;

RecordingManager::RecordingManager(QObject *parent)
    : QObject(parent)
{
    QDir srcDir = QFileInfo(QString(__FILE__)).absoluteDir();
    srcDir.cdUp(); // src/
    srcDir.cdUp(); // client/
    m_rootPath = srcDir.absolutePath() + "/record";

    qDebug() << "[RecordingManager] 저장 경로:" << m_rootPath;

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &RecordingManager::shutdownAll);

    for (int i = 1; i <= 4; ++i) {
        bool ok = QDir().mkpath(camDir(i));
        qDebug() << "[RecordingManager] mkpath" << camDir(i) << "→" << (ok ? "성공" : "실패");
        m_recording[i]        = false;
        m_manualStop[i]       = false;
        m_autoStartPending[i] = false;
        m_calibrating[i]      = false;
        m_frameCounts[i]      = 0;
        m_ffmpegProcesses[i]  = nullptr;
        m_segmentRolling[i]   = false;
    }
}

QString RecordingManager::camDir(int cameraId) const {
    return m_rootPath + "/camera" + QString::number(cameraId);
}

QString RecordingManager::findFfmpeg() const {
    QStringList candidates = {"ffmpeg"};
#ifdef Q_OS_WIN
    candidates << "ffmpeg.exe"
               << "C:/ffmpeg/bin/ffmpeg.exe"
               << "C:/Program Files/ffmpeg/bin/ffmpeg.exe"
               << "C:/Program Files (x86)/ffmpeg/bin/ffmpeg.exe";
#endif
    for (const QString &candidate : candidates) {
        QProcess test;
        test.start(candidate, {"-version"});
        if (test.waitForStarted(2000)) {
            test.kill();
            test.waitForFinished(1000);
            qDebug() << "[RecordingManager] ffmpeg 발견:" << candidate;
            return candidate;
        }
    }
    return {};
}

// 캘리브레이션 완료 후 실제 fps로 ffmpeg 시작
void RecordingManager::launchFfmpeg(int cameraId, double fps) {
    QString ffmpegBin = findFfmpeg();
    if (ffmpegBin.isEmpty()) {
        emit recordingError(cameraId, "ffmpeg를 시작할 수 없습니다.");
        return;
    }

    QString sessionPath;
    {
        QMutexLocker lock(&m_mutex);
        sessionPath = m_sessionPaths.value(cameraId);
    }

    QString outputFile = sessionPath + "/recording.mp4";
    QString fpsStr     = QString::number(fps, 'f', 2);
    qDebug() << "[RecordingManager] 측정된 fps:" << fps << "→ 인코딩 시작";

    QProcess *proc = new QProcess(this);
    QStringList args = {
        "-y",
        "-r",       fpsStr,
        "-f",       "mjpeg",
        "-i",       "pipe:0",
        "-c:v",     "libx264",
        "-pix_fmt", "yuv420p",
        "-preset",  "fast",
        "-r",       fpsStr,
        outputFile
    };
    proc->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    proc->start(ffmpegBin, args);

    if (!proc->waitForStarted(3000)) {
        QString err = proc->errorString();
        delete proc;
        emit recordingError(cameraId, "ffmpeg 프로세스를 시작할 수 없습니다:\n" + err);
        return;
    }

    QMutexLocker lock(&m_mutex);
    m_ffmpegProcesses[cameraId] = proc;
}

void RecordingManager::startRecording(int cameraId) {
    QMutexLocker lock(&m_mutex);
    if (m_recording.value(cameraId, false)) return;
    m_manualStop[cameraId] = false;

    // ffmpeg 존재 확인 (폴더 생성 전)
    QString ffmpegBin = findFfmpeg();
    if (ffmpegBin.isEmpty()) {
        lock.unlock();
        emit recordingError(cameraId, "ffmpeg가 설치되지 않았습니다.\nPATH에 ffmpeg를 추가한 후 재시작하세요.");
        return;
    }

    QString timestamp   = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString sessionPath = camDir(cameraId) + "/" + timestamp;
    QDir().mkpath(sessionPath);

    m_sessionPaths[cameraId]    = sessionPath;
    m_frameCounts[cameraId]     = 0;
    m_startTimes[cameraId]      = QDateTime::currentDateTime();
    m_ffmpegProcesses[cameraId] = nullptr;
    m_calibrating[cameraId]     = true;   // 첫 N 프레임으로 fps 측정 후 ffmpeg 시작
    m_calibBuf[cameraId].clear();
    m_calibFirstTime[cameraId]  = QDateTime();
    m_segmentRolling[cameraId]  = false;
    m_recording[cameraId]       = true;

    lock.unlock();
    emit recordingStateChanged(cameraId, true);
}

void RecordingManager::stopRecording(int cameraId) {
    QMutexLocker lock(&m_mutex);
    if (!m_recording.value(cameraId, false)) return;
    m_manualStop[cameraId] = true;

    // 캘리브레이션 중 중단 → ffmpeg 미시작, 폴더 정리
    if (m_calibrating.value(cameraId, false)) {
        m_calibrating[cameraId] = false;
        m_calibBuf[cameraId].clear();
        m_recording[cameraId] = false;
        QString sessionPath = m_sessionPaths[cameraId];
        lock.unlock();
        QDir(sessionPath).removeRecursively();
        emit recordingStateChanged(cameraId, false);
        return;
    }

    QString sessionPath = m_sessionPaths[cameraId];
    int frameCount      = m_frameCounts[cameraId];
    QDateTime startTime = m_startTimes[cameraId];
    QProcess *proc      = m_ffmpegProcesses.value(cameraId, nullptr);

    m_recording[cameraId]       = false;
    m_ffmpegProcesses[cameraId] = nullptr;

    lock.unlock();
    emit recordingStateChanged(cameraId, false);

    if (proc) {
        proc->closeWriteChannel();
        qDebug() << "[RecordingManager] ffmpeg stdin 닫음, 인코딩 대기 중...";

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, cameraId, sessionPath, frameCount, startTime](int exitCode, QProcess::ExitStatus) {
            qDebug() << "[RecordingManager] ffmpeg 종료, exitCode:" << exitCode
                     << "총 프레임:" << frameCount << "경로:" << sessionPath;

            QDateTime endTime = QDateTime::currentDateTime();

            QJsonObject meta;
            meta["camera_id"]   = cameraId;
            meta["start_time"]  = startTime.toString(Qt::ISODate);
            meta["end_time"]    = endTime.toString(Qt::ISODate);
            meta["frame_count"] = frameCount;

            QFile metaFile(sessionPath + "/meta.json");
            if (metaFile.open(QFile::WriteOnly))
                metaFile.write(QJsonDocument(meta).toJson());

            proc->deleteLater();
            emit recordingsChanged(cameraId);
        });
    }
}

bool RecordingManager::isRecording(int cameraId) const {
    return m_recording.value(cameraId, false);
}

void RecordingManager::onFrameReceived(int cameraId, const QByteArray &jpegData) {
    QProcess *proc = nullptr;
    int frameNum   = 0;
    QString thumbPath;

    bool         shouldLaunch = false;
    double       detectedFps  = 30.0;
    QList<QByteArray> flushFrames;

    {
        QMutexLocker lock(&m_mutex);

        if (!m_recording.value(cameraId, false)) {
            // 수동 중지 아니면 자동 시작
            if (!m_manualStop.value(cameraId, false) && !m_autoStartPending.value(cameraId, false)) {
                m_autoStartPending[cameraId] = true;
                QMetaObject::invokeMethod(this, [this, cameraId]() {
                    startRecording(cameraId);
                    QMutexLocker lk(&m_mutex);
                    m_autoStartPending[cameraId] = false;
                }, Qt::QueuedConnection);
            }
            return;
        }

        // ── 캘리브레이션: 첫 N 프레임으로 실제 fps 측정 ──────────────
        if (m_calibrating.value(cameraId, false)) {
            m_calibBuf[cameraId].append(jpegData);
            int bufSize = m_calibBuf[cameraId].size();

            // 초반 버스트 제외: CALIBRATION_SKIP+1 번째 프레임부터 타이머 시작
            if (bufSize == CALIBRATION_SKIP + 1)
                m_calibFirstTime[cameraId] = QDateTime::currentDateTime();

            if (bufSize >= CALIBRATION_SKIP + CALIBRATION_FRAMES) {
                qint64 elapsedMs = m_calibFirstTime[cameraId].msecsTo(QDateTime::currentDateTime());
                if (elapsedMs > 0)
                    detectedFps = (CALIBRATION_FRAMES - 1) * 1000.0 / elapsedMs;
                detectedFps = qBound(1.0, detectedFps, 120.0);

                flushFrames = std::move(m_calibBuf[cameraId]);
                m_calibBuf[cameraId].clear();
                m_calibrating[cameraId] = false;
                shouldLaunch = true;
            } else {
                return; // 아직 캘리브레이션 중
            }
        }

        if (!shouldLaunch) {
            proc     = m_ffmpegProcesses.value(cameraId, nullptr);
            frameNum = ++m_frameCounts[cameraId];
            if (frameNum == 1)
                thumbPath = m_sessionPaths[cameraId] + "/thumb.jpg";

            // 5분 초과 → 세그먼트 롤
            if (!m_segmentRolling.value(cameraId, false) &&
                m_startTimes[cameraId].secsTo(QDateTime::currentDateTime()) >= MAX_SEGMENT_SECS) {
                m_segmentRolling[cameraId] = true;
                QMetaObject::invokeMethod(this, [this, cameraId]() {
                    rollSegment(cameraId);
                }, Qt::QueuedConnection);
            }
        }
    }

    // ── 캘리브레이션 완료 → ffmpeg 시작 + 버퍼 플러시 ────────────────
    if (shouldLaunch) {
        launchFfmpeg(cameraId, detectedFps);

        QString sessionPath;
        {
            QMutexLocker lock(&m_mutex);
            sessionPath = m_sessionPaths.value(cameraId);
            proc = m_ffmpegProcesses.value(cameraId, nullptr);
            for (const QByteArray &frame : flushFrames) {
                if (proc && proc->state() == QProcess::Running)
                    proc->write(frame);
                ++m_frameCounts[cameraId];
            }
        }
        if (!flushFrames.isEmpty()) {
            QFile thumb(sessionPath + "/thumb.jpg");
            if (thumb.open(QFile::WriteOnly))
                thumb.write(flushFrames.first());
            qDebug() << "[RecordingManager] 썸네일 저장 cam" << cameraId;
        }
        return;
    }

    // ── 일반 프레임 전송 ──────────────────────────────────────────────
    if (proc && proc->state() == QProcess::Running)
        proc->write(jpegData);

    if (!thumbPath.isEmpty()) {
        QFile thumb(thumbPath);
        if (thumb.open(QFile::WriteOnly))
            thumb.write(jpegData);
        qDebug() << "[RecordingManager] 썸네일 저장 cam" << cameraId;
    }
}

QVariantList RecordingManager::getRecordings(int cameraId) const {
    QVariantList result;
    QDir dir(camDir(cameraId));
    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);

    for (const QFileInfo &entry : entries) {
        QString videoFile = entry.absoluteFilePath() + "/recording.mp4";
        if (!QFile::exists(videoFile)) continue;

        QVariantMap session;
        session["name"]      = entry.fileName();
        session["path"]      = entry.absoluteFilePath();
        session["thumbPath"] = "file:///" + entry.absoluteFilePath() + "/thumb.jpg";
        session["videoPath"] = "file:///" + videoFile;

        QFile metaFile(entry.absoluteFilePath() + "/meta.json");
        if (metaFile.open(QFile::ReadOnly)) {
            QJsonObject meta = QJsonDocument::fromJson(metaFile.readAll()).object();
            session["startTime"]  = meta["start_time"].toString();
            session["endTime"]    = meta["end_time"].toString();
            session["frameCount"] = meta["frame_count"].toInt();

            QDateTime start = QDateTime::fromString(meta["start_time"].toString(), Qt::ISODate);
            QDateTime end   = QDateTime::fromString(meta["end_time"].toString(),   Qt::ISODate);
            int secs = start.secsTo(end);
            session["duration"] = QString("%1:%2")
                                      .arg(secs / 60, 2, 10, QChar('0'))
                                      .arg(secs % 60, 2, 10, QChar('0'));
        } else {
            session["startTime"]  = entry.fileName().replace('_', ' ');
            session["frameCount"] = 0;
            session["duration"]   = "--:--";
        }

        result.append(session);
    }
    return result;
}

// 5분 세그먼트 종료 후 자동 재시작 (m_manualStop 건드리지 않음)
void RecordingManager::rollSegment(int cameraId) {
    QMutexLocker lock(&m_mutex);
    if (!m_recording.value(cameraId, false)) return;

    QString   sessionPath = m_sessionPaths[cameraId];
    int       frameCount  = m_frameCounts[cameraId];
    QDateTime startTime   = m_startTimes[cameraId];
    QProcess *proc        = m_ffmpegProcesses.value(cameraId, nullptr);

    m_recording[cameraId]       = false;
    m_ffmpegProcesses[cameraId] = nullptr;

    lock.unlock();
    emit recordingStateChanged(cameraId, false);

    if (proc) {
        proc->closeWriteChannel();
        qDebug() << "[RecordingManager] 세그먼트 롤 cam" << cameraId;

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, cameraId, sessionPath, frameCount, startTime](int, QProcess::ExitStatus) {
            QDateTime endTime = QDateTime::currentDateTime();

            QJsonObject meta;
            meta["camera_id"]   = cameraId;
            meta["start_time"]  = startTime.toString(Qt::ISODate);
            meta["end_time"]    = endTime.toString(Qt::ISODate);
            meta["frame_count"] = frameCount;

            QFile metaFile(sessionPath + "/meta.json");
            if (metaFile.open(QFile::WriteOnly))
                metaFile.write(QJsonDocument(meta).toJson());

            proc->deleteLater();
            emit recordingsChanged(cameraId);
        });
    }
    // m_manualStop = false 이므로 다음 프레임 도착 시 자동으로 새 세그먼트 시작
}

void RecordingManager::shutdownAll() {
    for (int i = 1; i <= 4; ++i) {
        QProcess  *proc = nullptr;
        QString    sessionPath;
        int        frameCount = 0;
        QDateTime  startTime;

        {
            QMutexLocker lock(&m_mutex);
            if (!m_recording.value(i, false)) continue;

            // 캘리브레이션 중이면 폴더 정리
            if (m_calibrating.value(i, false)) {
                m_calibrating[i] = false;
                m_calibBuf[i].clear();
                m_recording[i]   = false;
                sessionPath = m_sessionPaths[i];
                lock.unlock();
                QDir(sessionPath).removeRecursively();
                continue;
            }

            proc       = m_ffmpegProcesses.value(i, nullptr);
            sessionPath = m_sessionPaths[i];
            frameCount = m_frameCounts[i];
            startTime  = m_startTimes[i];
            m_recording[i]        = false;
            m_ffmpegProcesses[i]  = nullptr;
        }

        if (proc) {
            proc->closeWriteChannel();
            qDebug() << "[RecordingManager] 종료 대기 cam" << i;
            proc->waitForFinished(15000);

            QDateTime endTime = QDateTime::currentDateTime();
            QJsonObject meta;
            meta["camera_id"]   = i;
            meta["start_time"]  = startTime.toString(Qt::ISODate);
            meta["end_time"]    = endTime.toString(Qt::ISODate);
            meta["frame_count"] = frameCount;

            QFile metaFile(sessionPath + "/meta.json");
            if (metaFile.open(QFile::WriteOnly))
                metaFile.write(QJsonDocument(meta).toJson());

            delete proc;
        }
    }
}

void RecordingManager::deleteRecording(const QString &sessionPath) {
    QDir(sessionPath).removeRecursively();
    for (int i = 1; i <= 4; ++i) {
        if (sessionPath.startsWith(camDir(i))) {
            emit recordingsChanged(i);
            break;
        }
    }
}
