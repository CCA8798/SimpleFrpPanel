#include "ClientTunnelPage.h"

#include "ui_ClientTunnelPage.h"

ClientTunnelPage::ClientTunnelPage(QWidget* parent)
    : QWidget(parent)
    , m_Ui(new Ui::ClientTunnelPage())
{
    m_Ui->setupUi(this);
}

ClientTunnelPage::~ClientTunnelPage()
{
    delete m_Ui;
}
