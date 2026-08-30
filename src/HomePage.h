#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>

namespace Ui {
class HomePage;
}

// 中央页面：普通 QWidget，界面绑定 src/HomePage.ui。
// 在 Qt Designer 中编辑 HomePage.ui，通过"提升法"将控件提升为 Ela 组件
// （如 ElaPushButton，头文件 ElaPushButton.h），编译时即可链接真实类。
class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget* parent = nullptr);
    ~HomePage() override;

private:
    Ui::HomePage* m_Ui = nullptr;
};

#endif // HOMEPAGE_H
