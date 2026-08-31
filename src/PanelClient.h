#ifndef PANELCLIENT_H
#define PANELCLIENT_H

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;

// 面板 API 客户端：与服务端 PanelApiServer 通信（TCP + JSON 行协议）
// 登录成功签发 token，后续隧道命令携带 token；响应按请求序号关联
class PanelClient : public QObject
{
    Q_OBJECT

public:
    explicit PanelClient(QObject* parent = nullptr);

    bool isConnected() const;
    bool isLoggedIn() const;
    QString username() const;

    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    void login(const QString& username, const QString& password);
    void logout();
    void requestTunnels();
    void addTunnel(const QString& name, const QString& protocol, int remotePort,
                   const QString& localIp, int localPort, const QString& customDomain,
                   bool enabled, const QString& remark);
    void updateTunnel(int id, const QString& name, const QString& protocol, int remotePort,
                      const QString& localIp, int localPort, const QString& customDomain,
                      bool enabled, const QString& remark);
    void deleteTunnel(int id);
    void setTunnelEnabled(int id, bool enabled);

Q_SIGNALS:
    void connectionStateChanged(bool connected);
    void loginSucceeded(const QJsonObject& quota, const QJsonObject& serverInfo);
    void loginFailed(const QString& message);
    void loggedOut();
    void tunnelsReceived(const QJsonArray& tunnels, const QJsonObject& quota, bool frpsRunning);
    void commandSucceeded(const QString& cmd);
    void commandFailed(const QString& cmd, const QString& message);
    void logMessage(const QString& text);

private:
    void sendRequest(const QJsonObject& request);
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onConnectionTimeout();

    QTcpSocket* m_Socket = nullptr;
    QTimer* m_TimeoutTimer = nullptr;
    QByteArray m_ReceiveBuffer;
    QString m_Token;
    QString m_Username;
    bool m_WaitingLoginResponse = false;
    int m_RequestSeq = 0;
};

#endif // PANELCLIENT_H
