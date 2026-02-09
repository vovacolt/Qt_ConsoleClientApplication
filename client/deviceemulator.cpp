#include "deviceemulator.h"
#include <iostream>

#include <QRandomGenerator>
#include <QCoreApplication>
#include <QJsonParseError>

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

        m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
        m_socket->connectToHost("localhost", SERVER_PORT);
    }
}

void DeviceEmulator::onConnected()
{
    std::cout << "Connected to Server!" << std::endl;
    m_reconnectTimer->stop();
    m_buffer.clear();
}

void DeviceEmulator::onDisconnected()
{
    std::cout << "Disconnected. Retrying in 5s..." << std::endl;
    m_isStreaming = false;
    m_dataTimer->stop();
    m_reconnectTimer->start();
    m_buffer.clear();
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

    m_buffer.append(socket->readAll());

    while (true)
    {
        if (m_buffer.size() < (qsizetype)sizeof(quint32))
        {
            break;
        }

        QDataStream in(m_buffer);
        in.setVersion(QDataStream::Qt_6_5);

        quint32 blockSize;
        in >> blockSize;

        if (blockSize > MAX_PACKET_SIZE)
        {
            std::cerr << "Error: Packet too large (" << blockSize << "). Disconnecting." << std::endl;
            socket->disconnectFromHost();
            return;
        }

        // Wait for full packet
        if (m_buffer.size() < (qsizetype)(sizeof(quint32) + blockSize))
        {
            break;
        }

        // Remove header
        m_buffer.remove(0, sizeof(quint32));

        // Extract data
        QByteArray data = m_buffer.left(blockSize);
        m_buffer.remove(0, blockSize);

        // Parse JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            std::cerr << "JSON Parse Error: " << parseError.errorString().toStdString() << std::endl;
            continue;
        }

        if (doc.isObject())
        {
            processCommand(doc.object());
        }
    }
}

void DeviceEmulator::processCommand(const QJsonObject& json)
{
    QString type = json[KEY_TYPE].toString();

    if (type == PacketType::HANDSHAKE)
    {
        std::cout << "Server handshake received." << std::endl;
    }
    else if (type == PacketType::COMMAND_START)
    {
        if (!m_isStreaming)
        {
            std::cout << ">>> COMMAND START RECEIVED <<<" << std::endl;
            m_isStreaming = true;
            sendData();
        }
    }
    else if (type == PacketType::COMMAND_STOP)
    {
        if (m_isStreaming)
        {
            std::cout << ">>> COMMAND STOP RECEIVED <<<" << std::endl;
            m_isStreaming = false;
            m_dataTimer->stop();
        }
    }
}

void DeviceEmulator::sendData()
{
    if (!m_isStreaming || m_socket->state() != QAbstractSocket::ConnectedState)
    {
        return;
    }

    if (m_socket->bytesToWrite() > MAX_PENDING_WRITE_BYTES)
    {
        std::cout << "WARNING: Network congested. Skipping frame." << std::endl;
        m_dataTimer->start(100);
        return;
    }

    int roll = QRandomGenerator::global()->bounded(3);
    QJsonObject payload;

    switch (roll)
    {
    case 0: payload = generateMetrics(); break;
    case 1: payload = generateStatus(); break;
    case 2: payload = generateLog(); break;
    }

    m_socket->write(packJson(payload));

    std::cout << "Sent message: " << std::endl << payload[KEY_TYPE].toString().toStdString() << std::endl;

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
