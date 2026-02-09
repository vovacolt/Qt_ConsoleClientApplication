#ifndef NETWORKHELPERS_H
#define NETWORKHELPERS_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

// Constants
const int SERVER_PORT = 12345;
const QString KEY_TYPE = "type";
const QString KEY_PAYLOAD = "payload";
const QString KEY_ID = "id";

// Security & Limits
const quint32 MAX_PACKET_SIZE = 10 * 1024 * 1024; // 10 MB limit
const int MAX_PENDING_WRITE_BYTES = 10 * 1024 * 1024; // 10 MB buffer limit for client
const int CLIENT_TIMEOUT_MS = 30000;

// Types of packages
namespace PacketType
{
const QString NETWORK_METRICS = "NetworkMetrics";
const QString DEVICE_STATUS = "DeviceStatus";
const QString LOG = "Log";
const QString HANDSHAKE = "Handshake";
const QString COMMAND_START = "CmdStart";
const QString COMMAND_STOP = "CmdStop";
const QString ERROR_MSG = "Error";
}

// Helper for packing JSON into a QByteArray with a header that stores the message size
inline QByteArray packJson(const QJsonObject& json)
{
    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);

    out << (quint32)data.size();
    out.writeRawData(data.constData(), data.size());

    return block;
}

#endif // NETWORKHELPERS_H
