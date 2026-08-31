#ifndef SERVERUSERPAGE_H
#define SERVERUSERPAGE_H

#include <QWidget>

class DatabaseManager;
class QStandardItemModel;

namespace Ui {
class ServerUserPage;
}

// 服务端 · 用户管理页：
// - 上 30%：数据库文件管理（下拉选择 / 新建 / 删除 / 刷新）+ 当前库设置（公网 IP、端口）
// - 下 70%：当前库的用户列表（查询 / 新增 / 修改 / 删除）
class ServerUserPage : public QWidget
{
    Q_OBJECT

public:
    explicit ServerUserPage(QWidget* parent = nullptr);
    ~ServerUserPage() override;

private slots:
    void onRefreshDbComboBox();
    void onCreateDatabase();
    void onDeleteDatabase();
    void onCurrentDbChanged();
    void onSaveSetting();
    void onSearchUsers();
    void onAddUser();
    void onEditUser();
    void onDeleteUser();

private:
    void loadSettingToUi();
    void refreshUserTable();
    void updateControlsEnabled(bool isDatabaseOpen);
    int selectedUserId() const;

    Ui::ServerUserPage* m_Ui = nullptr;
    DatabaseManager* m_DatabaseManager = nullptr;
    QStandardItemModel* m_UserModel = nullptr;
};

#endif // SERVERUSERPAGE_H
