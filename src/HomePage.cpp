#include "HomePage.h"

#include "ui_HomePage.h"

HomePage::HomePage(QWidget* parent)
    : QWidget(parent)
    , m_Ui(new Ui::HomePage())
{
    m_Ui->setupUi(this);
}

HomePage::~HomePage()
{
    delete m_Ui;
}
