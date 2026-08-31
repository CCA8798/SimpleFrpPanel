#ifndef SERVERTUNNELPAGE_H
#define SERVERTUNNELPAGE_H

#include <QWidget>

namespace Ui {
class ServerTunnelPage;
}

// 服务端 · 隧道管理页：界面绑定 src/ServerTunnelPage.ui，
// 可在 Qt Designer 中通过"提升法"放置 Ela 控件。
class ServerTunnelPage : public QWidget
{
    Q_OBJECT

public:
    explicit ServerTunnelPage(QWidget* parent = nullptr);
    ~ServerTunnelPage() override;

private:
    Ui::ServerTunnelPage* m_Ui = nullptr;
};

#endif // SERVERTUNNELPAGE_H
