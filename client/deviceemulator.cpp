#include "deviceemulator.h"
#include <iostream>

#include <QRandomGenerator>
#include <QCoreApplication>

#include "../common/networkhelpers.h"

DeviceEmulator::DeviceEmulator(QObject *parent)
    : QObject(parent), m_isStreaming(false)
{
    m_socket = new QTcpSocket(this);
    m_reconnectTimer = new QTimer(this);
    m_dataTimer = new QTimer(this);

    // Reconnection timer (5 sec)
    m_reconnectTimer->setInterval(5000);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DeviceEmulator::connectToServer);

    // Data sending timer
    connect(m_dataTimer, &QTimer::timeout, this, &DeviceEmulator::sendData);

    connect(m_socket, &QTcpSocket::connected, this, &DeviceEmulator::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &DeviceEmulator::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &DeviceEmulator::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &DeviceEmulator::onError);
}

void DeviceEmulator::start()
{
    connectToServer();
}

void DeviceEmulator::connectToServer()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState)
    {
        std::cout << "Connecting to localhost..." << std::endl;
        m_socket->connectToHost("localhost", SERVER_PORT);
    }
}

void DeviceEmulator::onConnected()
{
    std::cout << "Connected to Server!" << std::endl;
    m_reconnectTimer->stop();
}

void DeviceEmulator::onDisconnected()
{
    std::cout << "Disconnected. Retrying in 5s..." << std::endl;
    m_isStreaming = false;
    m_dataTimer->stop();
    m_reconnectTimer->start();
}

void DeviceEmulator::onError(QAbstractSocket::SocketError socketError)
{
    std::cout << "Socket Error [" << socketError << "]: " << m_socket->errorString().toStdString() << std::endl;

    // Reset the streaming flag and data sending timer on any error
    m_isStreaming = false;
    m_dataTimer->stop();

    if (m_socket->state() == QAbstractSocket::UnconnectedState && !m_reconnectTimer->isActive())
    {
        std::cout << "Connection failed. Retrying in 5s..." << std::endl;
        m_reconnectTimer->start();
    }
}

void DeviceEmulator::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());

    if (!socket)
    {
        return;
    }

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_5);

    while (true)
    {
        if (socket->bytesAvailable() < sizeof(quint32))
        {
            break;
        }

        // Read the packet size, but don't move the position until we're sure the data arrived
        QByteArray header = socket->peek(sizeof(quint32));
        QDataStream headerStream(header);
        headerStream.setVersion(QDataStream::Qt_6_5);
        quint32 blockSize;
        headerStream >> blockSize;

        // Waiting for more data
        if (socket->bytesAvailable() < (quint32)sizeof(quint32) + blockSize)
        {
            break;
        }
        // Data is ready

        // Skipping the size header
        quint32 dummy;
        in >> dummy;

        QByteArray data;
        data.resize(blockSize);
        in.readRawData(data.data(), blockSize);

        QJsonDocument doc = QJsonDocument::fromJson(data.data());

        if (doc.isObject())
        {
            processCommand(doc.object());
        }
    }
}

void DeviceEmulator::processCommand(const QJsonObject& json)
{
    QString type = json[KEY_TYPE].toString();
    std::cout << "Received: " << type.toStdString() << std::endl;

    if (type == PacketType::HANDSHAKE)
    {
        std::cout << "Server confirmed connection." << std::endl;
    }
    else if (type == PacketType::COMMAND_START)
    {
        m_isStreaming = true;
        sendData();
    }
    else if (type == PacketType::COMMAND_STOP)
    {
        m_isStreaming = false;
        m_dataTimer->stop();
    }
}

void DeviceEmulator::sendData()
{
    if (!m_isStreaming || m_socket->state() != QAbstractSocket::ConnectedState)
    {
        return;
    }

    // Selecting a data type
    int roll = QRandomGenerator::global()->bounded(3);
    QJsonObject payload;

    switch (roll)
    {
    case 0: payload = generateMetrics(); break;
    case 1: payload = generateStatus(); break;
    case 2: payload = generateLog(); break;
    }

    m_socket->write(packJson(payload));

    // Logs the message being sent
    QJsonDocument doc(payload);
    QByteArray byteArray = doc.toJson();
    QString jsonString = QString(byteArray);
    std::cout << "Sent message: " << std::endl << jsonString.toStdString() << std::endl;

    // Random delay 10ms - 100ms
    int delay = QRandomGenerator::global()->bounded(10, 101);
    m_dataTimer->start(delay);
}

QJsonObject DeviceEmulator::generateMetrics()
{
    QJsonObject json;

    json[KEY_TYPE] = PacketType::NETWORK_METRICS;
    json["bandwidth"] = QRandomGenerator::global()->generateDouble() * 1000.0;
    json["latency"] = QRandomGenerator::global()->generateDouble() * 50.0;
    json["packet_loss"] = QRandomGenerator::global()->generateDouble() * 0.05;

    return json;
}

QJsonObject DeviceEmulator::generateStatus()
{
    QJsonObject json;

    json[KEY_TYPE] = PacketType::DEVICE_STATUS;
    json["uptime"] = (int)QRandomGenerator::global()->bounded(1000, 50000);
    json["cpu_usage"] = QRandomGenerator::global()->bounded(0, 101);
    json["memory_usage"] = QRandomGenerator::global()->bounded(10, 90);

    return json;
}

QJsonObject DeviceEmulator::generateLog()
{
    QJsonObject json;
    json[KEY_TYPE] = PacketType::LOG;

    QString msg;
    int len = QRandomGenerator::global()->bounded(0, 3);

    if(len == 0)
    {
        msg = "Short log info"; // < 50
        json["severity"] = "INFO";
    }
    else if(len == 1)
    {
        msg = "Medium log info message with some details about system state..."; // 50-200
        json["severity"] = "WARNING";
    }
    else
    {
        msg = QString("Long log error details...").repeated(10); // 200+
        json["severity"] = "ERROR";
    }

    json["message"] = msg;

    return json;
}
