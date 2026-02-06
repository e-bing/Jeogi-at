#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFont>
#include <QDebug>
#include <QSslSocket>
#include <QSslError>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupUI();

    // OpenSSL 지원 여부 확인 및 UI에 표시
    QString sslStatus;
    if (!QSslSocket::supportsSsl()) {
        sslStatus = "❌ OpenSSL 미지원\n";
        sslStatus += "빌드: " + QSslSocket::sslLibraryBuildVersionString() + "\n";
        sslStatus += "런타임: " + QSslSocket::sslLibraryVersionString();

        QMessageBox::warning(this, "OpenSSL 확인", sslStatus);
    } else {
        sslStatus = "✅ OpenSSL 지원됨!\n";
        sslStatus += "빌드: " + QSslSocket::sslLibraryBuildVersionString() + "\n";
        sslStatus += "런타임: " + QSslSocket::sslLibraryVersionString();

        QMessageBox::information(this, "OpenSSL 확인", sslStatus);
    }

    lblStatus->setText(sslStatus);
    lblStatus->setStyleSheet("font-size: 12px; padding: 10px; background: #d4edda;");

    // TLS 소켓 생성
    socket = new QSslSocket(this);

    // 시그널 연결
    connect(socket, &QSslSocket::encrypted, this, &MainWindow::onEncrypted);
    connect(socket, &QSslSocket::connected, this, &MainWindow::onConnected);
    connect(socket, &QSslSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(socket, &QSslSocket::readyRead, this, &MainWindow::readData);
    connect(socket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
            this, &MainWindow::onSslErrors);

    qDebug() << "TLS 서버 연결 시도 중...";
    socket->connectToHostEncrypted("aboy.local", 12345);

    // 연결 상태 업데이트
    lblStatus->setText("🔌 서버 연결 시도 중...\n(TLS 암호화 예정)");
}

MainWindow::~MainWindow()
{
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->disconnectFromHost();
    }
    delete ui;
}

void MainWindow::setupUI()
{
    setWindowTitle("🚉 지하철 통합 모니터링 시스템");
    resize(900, 650);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // 상태 라벨
    lblStatus = new QLabel("서버 연결 대기 중...", this);
    lblStatus->setStyleSheet("font-size: 14px; padding: 12px; background: #e3f2fd; border-radius: 6px;");
    lblStatus->setWordWrap(true);
    layout->addWidget(lblStatus);

    // 탭 위젯
    tabWidget = new QTabWidget(this);
    tabWidget->setDocumentMode(true);
    tabWidget->setTabPosition(QTabWidget::North);

    // Tab 1: 실시간 혼잡도
    txtRealtime = new QTextEdit(this);
    txtRealtime->setReadOnly(true);
    txtRealtime->setFont(QFont("Malgun Gothic", 10));  // 한글 + 이모지 잘 보이게
    tabWidget->addTab(txtRealtime, "📊 실시간 혼잡도");

    // Tab 2: 공기질 통계
    txtAirStats = new QTextEdit(this);
    txtAirStats->setReadOnly(true);
    txtAirStats->setFont(QFont("Malgun Gothic", 10));
    tabWidget->addTab(txtAirStats, "🌫️ 공기질 통계");

    // Tab 3: 승객 흐름 통계
    txtFlowStats = new QTextEdit(this);
    txtFlowStats->setReadOnly(true);
    txtFlowStats->setFont(QFont("Malgun Gothic", 10));
    tabWidget->addTab(txtFlowStats, "👥 승객 흐름 통계");

    layout->addWidget(tabWidget);
}

void MainWindow::onEncrypted()
{
    qDebug() << "TLS 핸드셰이크 완료 → 암호화 연결 성공";
    QString certInfo = socket->peerCertificate().subjectInfo(QSslCertificate::CommonName).join(", ");
    lblStatus->setText("🔒 TLS 암호화 연결 완료\n서버: " + certInfo);
    lblStatus->setStyleSheet("font-size: 14px; padding: 12px; background: #c8e6c9; border-radius: 6px;");
}

void MainWindow::onConnected()
{
    qDebug() << "기본 연결 성공 (TLS 완료 대기 중)";
}

void MainWindow::onDisconnected()
{
    qDebug() << "서버 연결 끊김";
    lblStatus->setText("❌ 서버와 연결이 끊어졌습니다.");
    lblStatus->setStyleSheet("font-size: 14px; padding: 12px; background: #ffebee; border-radius: 6px;");
}

void MainWindow::onSslErrors(const QList<QSslError> &errors)
{
    QString errMsg = "SSL/TLS 오류 발생:\n";
    for (const QSslError &err : errors) {
        errMsg += "• " + err.errorString() + "\n";
    }
    qDebug() << errMsg;

    // 개발/테스트용: 자체서명 인증서 무시 (프로덕션에서는 제거하거나 사용자 확인 필요!)
    socket->ignoreSslErrors();

    // 필요 시 아래처럼 사용자에게 알림
    // QMessageBox::warning(this, "TLS 인증 오류", errMsg);
}

void MainWindow::readData()
{
    while (socket->canReadLine()) {
        QByteArray line = socket->readLine().trimmed();
        if (line.isEmpty()) continue;

        qDebug() << "수신 데이터:" << line;

        QJsonDocument jsonDoc = QJsonDocument::fromJson(line);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            qDebug() << "잘못된 JSON 형식";
            continue;
        }

        QJsonObject jsonObj = jsonDoc.object();
        QString type = jsonObj["type"].toString();

        if (type == "realtime") {
            updateRealtimeView(jsonObj["data"].toArray());
        }
        else if (type == "air_stats") {
            updateAirStatsView(jsonObj["data"].toArray());
        }
        else if (type == "flow_stats") {
            updateFlowStatsView(jsonObj["data"].toArray());
        }
    }
}

void MainWindow::updateRealtimeView(const QJsonArray& data)
{
    QString html = "<h2>🚉 실시간 혼잡도 모니터링</h2><hr>";
    html += "<table border='1' cellpadding='8' cellspacing='0' style='border-collapse:collapse; width:100%; font-family:Malgun Gothic;'>";
    html += "<tr style='background:#4CAF50; color:white;'><th>역</th><th>구역</th><th>인원</th><th>상태</th></tr>";

    for (const QJsonValue& val : data) {
        QJsonObject obj = val.toObject();
        QString station = obj["station"].toString();
        QString platform = obj["platform"].toString();
        int count = obj["count"].toInt();
        QString status = obj["status"].toString();  // 서버에서 "🟢 정상" 형태로 올 수 있음

        QString bgColor = "#ffffff";
        if (status.contains("주의") || status.contains("🟡")) bgColor = "#fff9c4";
        if (status.contains("위험") || status.contains("🔴")) bgColor = "#ffcdd2";

        html += QString("<tr style='background:%1;'><td>%2</td><td>%3</td><td>%4명</td><td>%5</td></tr>")
                    .arg(bgColor, station.toHtmlEscaped(), platform.toHtmlEscaped(),
                         QString::number(count), status.toHtmlEscaped());
    }

    html += "</table>";
    txtRealtime->setHtml(html);
}

void MainWindow::updateAirStatsView(const QJsonArray& data)
{
    QString days[] = {"", "일요일", "월요일", "화요일", "수요일", "목요일", "금요일", "토요일"};

    QMap<int, QList<QJsonObject>> grouped;
    for (const QJsonValue& val : data) {
        QJsonObject obj = val.toObject();
        int day = obj["day"].toInt();
        grouped[day].append(obj);
    }

    QString html = "<h2>🌫️ 공기질 상세 리포트 (CAM-01)</h2><hr>";

    for (int d = 1; d <= 7; ++d) {
        if (!grouped.contains(d)) continue;

        html += QString("<h3>▶ %1요일</h3>").arg(days[d]);
        html += "<table border='1' cellpadding='8' cellspacing='0' style='border-collapse:collapse; width:100%; font-family:Malgun Gothic;'>";
        html += "<tr style='background:#FF9800; color:white;'><th>시간</th><th>CO 수치 (ppm)</th><th>유해가스 (%)</th><th>상태</th></tr>";

        for (const QJsonObject& obj : grouped[d]) {
            int hour = obj["hour"].toInt();
            double co = obj["co"].toDouble();
            double gas = obj["gas"].toDouble();

            // 서버에서 보낸 이모지 사용 (없으면 기본 계산)
            QString coEmoji = obj.contains("co_emoji") ? obj["co_emoji"].toString() : (co > 10 ? "😷" : "👍");
            QString gasEmoji = obj.contains("gas_emoji") ? obj["gas_emoji"].toString() : (gas > 0.5 ? "☣️" : "👍");

            html += QString("<tr><td>%1시</td><td>%2</td><td>%3</td><td>%4 %5</td></tr>")
                        .arg(hour)
                        .arg(co, 0, 'f', 2)
                        .arg(gas, 0, 'f', 2)
                        .arg(coEmoji, gasEmoji);
        }

        html += "</table><br>";
    }

    txtAirStats->setHtml(html);
}

void MainWindow::updateFlowStatsView(const QJsonArray& data)
{
    QString days[] = {"", "일요일", "월요일", "화요일", "수요일", "목요일", "금요일", "토요일"};

    QMap<int, QList<QJsonObject>> grouped;
    for (const QJsonValue& val : data) {
        QJsonObject obj = val.toObject();
        int day = obj["day"].toInt();
        grouped[day].append(obj);
    }

    QString html = "<h2>👥 승객 흐름 리포트 (CAM-01)</h2><hr>";

    for (int d = 1; d <= 7; ++d) {
        if (!grouped.contains(d)) continue;

        html += QString("<h3>▶ %1요일 분석</h3>").arg(days[d]);
        html += "<table border='1' cellpadding='8' cellspacing='0' style='border-collapse:collapse; width:100%; font-family:Malgun Gothic;'>";
        html += "<tr style='background:#2196F3; color:white;'><th>시간</th><th>평균 인원</th><th>상태</th></tr>";

        for (const QJsonObject& obj : grouped[d]) {
            int hour = obj["hour"].toInt();
            int avg = obj["avg_count"].toInt();
            QString status = obj["status"].toString();  // "🚨 매우 혼잡" 등

            html += QString("<tr><td>%1시</td><td>%2명</td><td>%3</td></tr>")
                        .arg(hour)
                        .arg(avg)
                        .arg(status.toHtmlEscaped());
        }

        html += "</table><br>";
    }

    txtFlowStats->setHtml(html);
}
