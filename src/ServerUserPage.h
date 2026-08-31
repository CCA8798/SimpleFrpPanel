#ifndef SERVERUSERPAGE_H
#define SERVERUSERPAGE_H

#include <QWidget>

namespace Ui {
class ServerUserPage;
}

// 服务端 · 用户管理页：界面绑定 src/ServerUserPage.ui，
// 可在 Qt Designer 中通过"提升法"放置 Ela 控件。
class ServerUserPage : public QWidget
{
    Q_OBJECT

public:
    explicit ServerUserPage(QWidget* parent = nullptr);
    ~ServerUserPage() override;

private:
    Ui::ServerUserPage* m_Ui = nullptr;
};

#endif // SERVERUSERPAGE_H
