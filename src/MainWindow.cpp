#include "MainWindow.h"

#include "HomePage.h"

MainWindow::MainWindow(QWidget* parent)
    : ElaWindow(parent)
{
    setWindowTitle(QStringLiteral("SimpleFrpPanel"));
    resize(1000, 700);
    moveToCenter();
    setUserInfoCardVisible(false);

    m_HomePage = new HomePage(this);
    addPageNode(QStringLiteral("首页"), m_HomePage);
}

MainWindow::~MainWindow()
{
}
