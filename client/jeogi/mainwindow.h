#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSslSocket>       // QTcpSocket → QSslSocket으로 변경
#include <QLabel>
#include <QTextEdit>
#include <QTabWidget>
#include <QJsonArray>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onEncrypted();           // 새로 추가
    void onSslErrors(const QList<QSslError> &errors);
    void onConnected();
    void onDisconnected();
    void readData();

private:
    Ui::MainWindow *ui;
    QSslSocket *socket;           // QTcpSocket → QSslSocket

    // UI 위젯들 (기존과 동일)
    QTabWidget *tabWidget;
    QTextEdit *txtRealtime;
    QTextEdit *txtAirStats;
    QTextEdit *txtFlowStats;
    QLabel *lblStatus;

    void setupUI();
    void updateRealtimeView(const QJsonArray& data);
    void updateAirStatsView(const QJsonArray& data);
    void updateFlowStatsView(const QJsonArray& data);
};

#endif // MAINWINDOW_H
