#include "ServerTunnelPage.h"

#include "ui_ServerTunnelPage.h"

ServerTunnelPage::ServerTunnelPage(QWidget* parent)
    : QWidget(parent)
    , m_Ui(new Ui::ServerTunnelPage())
{
    m_Ui->setupUi(this);
}

ServerTunnelPage::~ServerTunnelPage()
{
    delete m_Ui;
}
