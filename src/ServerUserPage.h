#ifndef SERVERUSERPAGE_H
#define SERVERUSERPAGE_H

#include <QWidget>

#include <functional>

class DatabaseManager;
class QStandardItemModel;

namespace Ui {
class ServerUserPage;
}

// 服务端 · 用户管理页：
// - 上部：数据库文件管理（下拉选择 / 新建 / 删除 / 刷新）+ 当前库设置（公网 IP、端口），
//   固定高度、紧凑布局，不随窗口拉伸
// - 下部：当前库的用户列表（查询 / 新增 / 修改 / 删除），随窗口尺寸自动扩展
class ServerUserPage : public QWidget
{
    Q_OBJECT

public:
    explicit ServerUserPage(QWidget* parent = nullptr);
    ~ServerUserPage() override;

protected:
    // 每次切换到本面板时刷新一次
    void showEvent(QShowEvent* event) override;

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
    void onPollRefresh();

private:
    void loadSettingToUi();
    void refreshUserTable();
    void updateControlsEnabled(bool isDatabaseOpen);
    int selectedUserId() const;
    QString stateSignature() const;
    void showConfirmDialog(const QString& title, const QString& content,
                           const QString& confirmText, std::function<void()> onConfirm);

    Ui::ServerUserPage* m_Ui = nullptr;
    DatabaseManager* m_DatabaseManager = nullptr;
    QStandardItemModel* m_UserModel = nullptr;
    QTimer* m_PollTimer = nullptr;
    QString m_LastSignature;
};

#endif // SERVERUSERPAGE_H
