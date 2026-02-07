#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

#include "client/deviceemulator.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    DeviceEmulator client;
    client.start();

    return a.exec();
}
