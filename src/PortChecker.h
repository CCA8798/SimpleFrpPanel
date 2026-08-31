#ifndef PORTCHECKER_H
#define PORTCHECKER_H

#include <QString>

// 端口占用检测工具：通过"尝试绑定"探测本机端口是否空闲
// （tcp 用 QTcpServer，udp 用 QUdpSocket；绑定失败即认为被其他程序占用）
class PortChecker
{
public:
    static bool isPortFree(quint16 port, const QString& protocol);
};

#endif // PORTCHECKER_H
