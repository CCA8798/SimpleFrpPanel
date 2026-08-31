#include "ServerUserPage.h"

#include "ui_ServerUserPage.h"

ServerUserPage::ServerUserPage(QWidget* parent)
    : QWidget(parent)
    , m_Ui(new Ui::ServerUserPage())
{
    m_Ui->setupUi(this);
}

ServerUserPage::~ServerUserPage()
{
    delete m_Ui;
}
