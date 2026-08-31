#ifndef CLIENTTUNNELPAGE_H
#define CLIENTTUNNELPAGE_H

#include <QWidget>

namespace Ui {
class ClientTunnelPage;
}

// 客户端 · 隧道管理页：界面绑定 src/ClientTunnelPage.ui，
// 可在 Qt Designer 中通过"提升法"放置 Ela 控件。
class ClientTunnelPage : public QWidget
{
    Q_OBJECT

public:
    explicit ClientTunnelPage(QWidget* parent = nullptr);
    ~ClientTunnelPage() override;

private:
    Ui::ClientTunnelPage* m_Ui = nullptr;
};

#endif // CLIENTTUNNELPAGE_H
