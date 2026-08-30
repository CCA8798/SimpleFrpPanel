#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ElaWindow.h"

class HomePage;

// 主窗口：使用 Ela 的 ElaWindow（无边框 + 导航栏 + 自绘标题栏）。
// 中央页面使用普通 QWidget（HomePage），其界面由 src/HomePage.ui 描述，
// 可在 Qt Designer 中通过"提升法"放置 Ela 控件。
class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    HomePage* m_HomePage = nullptr;
};

#endif // MAINWINDOW_H
