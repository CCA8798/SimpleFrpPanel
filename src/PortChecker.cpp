#include "PortChecker.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QUdpSocket>

bool PortChecker::isPortFree(quint16 port, const QString& protocol)
{
    if (protocol == QLatin1String("udp"))
    {
        QUdpSocket socket;
        return socket.bind(QHostAddress::Any, port);
    }
    // 默认按 TCP 检测
    QTcpServer server;
    return server.listen(QHostAddress::Any, port);
}
