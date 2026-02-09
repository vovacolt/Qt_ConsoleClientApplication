#ifndef DEVICEEMULATOR_H
#define DEVICEEMULATOR_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>

class DeviceEmulator : public QObject
{
    Q_OBJECT

public:
    explicit DeviceEmulator(QObject *parent = nullptr);

public slots:
    void start();

private:
    void processCommand(const QJsonObject& json);

private slots:
    void connectToServer();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError socketError);
    void onReadyRead();
    void sendData();

private:
    QTcpSocket* m_socket;
    QTimer* m_reconnectTimer;
    QTimer* m_dataTimer;

    QByteArray m_buffer;
    QJsonObject generateMetrics();
    QJsonObject generateStatus();
    QJsonObject generateLog();

    bool m_isStreaming;
};

#endif // DEVICEEMULATOR_H
