#include "SettingsDialog.h"

#include "ElaComboBox.h"
#include "ElaContentDialog.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaToggleSwitch.h"
#include "Information.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QObject>
#include <QTimer>
#include <QVBoxLayout>

// 本文件为自由函数对话框，字符串上下文统一用 "SimpleFrpPanel"（与 .ts 一致）。
// 源字符串保留中文：默认（zh_CN 未加载翻译）显示中文；英文经 SimpleFrpPanel_en.qm 提供。


void showSettingsDialog(QWidget* parent)
{
    ElaContentDialog* dialog = new ElaContentDialog(parent);
    dialog->setWindowTitle(QCoreApplication::translate("SimpleFrpPanel", "设置"));
    dialog->setLeftButtonText(QString());
    dialog->setMiddleButtonText(QString());
    dialog->setRightButtonText(QCoreApplication::translate("SimpleFrpPanel", "完成"));

    // 给足空间：ElaContentDialog 默认只有 400px 宽，文本长了会挤压换行
    dialog->resize(540, 430);

    QWidget* centralWidget = new QWidget(dialog);
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(24, 22, 24, 12);
    centralLayout->setSpacing(4);

    // 首尾等量 stretch：让整组内容在对话框中央区域垂直居中
    centralLayout->addStretch(1);

    // 标题
    ElaText* titleText = new ElaText(QCoreApplication::translate("SimpleFrpPanel", "设置"), centralWidget);
    titleText->setTextPixelSize(17);
    titleText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    centralLayout->addWidget(titleText);
    centralLayout->addSpacing(14);

    // —— 「关闭窗口时弹出询问」开关行（单行：文字 + 弹性 + 开关） ——
    QWidget* rowWidget = new QWidget(centralWidget);
    QHBoxLayout* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(12);
    ElaText* rowTitle = new ElaText(QCoreApplication::translate("SimpleFrpPanel", "关闭窗口时弹出询问"), rowWidget);
    rowTitle->setTextPixelSize(14);
    rowTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    rowLayout->addWidget(rowTitle);
    rowLayout->addStretch();

    ElaToggleSwitch* confirmSwitch = new ElaToggleSwitch(rowWidget);
    // 注意：ElaToggleSwitch 构造时 setFixedSize(44, 22)，旋钮端点按该宽度计算；
    // 不要再改其宽度（setMinimumWidth 等会让轨道变宽而旋钮不重算，视觉上停在中间）
    confirmSwitch->setIsToggled(g_GlobalInformation.confirmOnClose);
    rowLayout->addWidget(confirmSwitch);
    centralLayout->addWidget(rowWidget);
    centralLayout->addSpacing(8);

    // 说明文字：单行 + Fixed 高度，避免换行文本裁剪
    ElaText* rowHintLine1 = new ElaText(
        QCoreApplication::translate("SimpleFrpPanel", "开启后，每次点关闭都会询问「后台运行 / 直接退出」。"), centralWidget);
    ElaText* rowHintLine2 = new ElaText(
        QCoreApplication::translate("SimpleFrpPanel", "弹窗中取消勾选“下次继续弹出”，将停止询问并按所选动作执行。"), centralWidget);
    const QList<ElaText*> hintLabels = {rowHintLine1, rowHintLine2};
    for (ElaText* hint : hintLabels)
    {
        hint->setTextPixelSize(12);
        hint->setWordWrap(false);
        hint->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        hint->setFixedHeight(20);
        centralLayout->addWidget(hint);
    }
    centralLayout->addSpacing(6);

    ElaText* actionHint = new ElaText(centralWidget);
    actionHint->setTextPixelSize(12);
    actionHint->setWordWrap(false);
    actionHint->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    actionHint->setFixedHeight(20);
    centralLayout->addWidget(actionHint);
    centralLayout->addSpacing(12);

    // —— 界面语言选择行 ——
    QWidget* langRow = new QWidget(centralWidget);
    QHBoxLayout* langLayout = new QHBoxLayout(langRow);
    langLayout->setContentsMargins(0, 0, 0, 0);
    langLayout->setSpacing(12);
    ElaText* langTitle = new ElaText(QCoreApplication::translate("SimpleFrpPanel", "界面语言"), langRow);
    langTitle->setTextPixelSize(14);
    langTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    langLayout->addWidget(langTitle);
    langLayout->addStretch();

    ElaComboBox* langCombo = new ElaComboBox(langRow);
    langCombo->addItem(QStringLiteral("简体中文")); // 语言自名不翻译
    langCombo->addItem(QStringLiteral("English"));
    const int currentIndex = (g_GlobalInformation.language == QStringLiteral("en")) ? 1 : 0;
    langCombo->setCurrentIndex(currentIndex);
    langCombo->setFixedSize(150, 30);
    langLayout->addWidget(langCombo);
    centralLayout->addWidget(langRow);

    ElaText* langHint = new ElaText(
        QCoreApplication::translate("SimpleFrpPanel", "切换后需重启程序生效。"), centralWidget);
    langHint->setTextPixelSize(12);
    langHint->setWordWrap(false);
    langHint->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    langHint->setFixedHeight(20);
    centralLayout->addWidget(langHint);
    centralLayout->addSpacing(12);

    // —— 更新代理行 ——
    QWidget* proxyRow = new QWidget(centralWidget);
    QHBoxLayout* proxyLayout = new QHBoxLayout(proxyRow);
    proxyLayout->setContentsMargins(0, 0, 0, 0);
    proxyLayout->setSpacing(12);
    ElaText* proxyTitle = new ElaText(
        QCoreApplication::translate("SimpleFrpPanel", "frp 更新代理"), proxyRow);
    proxyTitle->setTextPixelSize(14);
    proxyTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    proxyLayout->addWidget(proxyTitle);
    proxyLayout->addStretch();

    ElaLineEdit* proxyEdit = new ElaLineEdit(proxyRow);
    proxyEdit->setPlaceholderText(QStringLiteral("127.0.0.1:7897")); // 占位示例不翻译
    proxyEdit->setText(g_GlobalInformation.updateProxy);
    proxyEdit->setFixedSize(170, 30);
    proxyLayout->addWidget(proxyEdit);
    centralLayout->addWidget(proxyRow);

    ElaText* proxyHint = new ElaText(
        QCoreApplication::translate("SimpleFrpPanel",
                                    "更新 frp 时使用的 HTTP 代理（主机:端口），留空则使用系统代理。"),
        centralWidget);
    proxyHint->setTextPixelSize(12);
    proxyHint->setWordWrap(false);
    proxyHint->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    proxyHint->setFixedHeight(20);
    centralLayout->addWidget(proxyHint);

    centralLayout->addStretch();

    dialog->setCentralWidget(centralWidget);

    // 对话框窗口以父窗口中心为准显式居中：
    // 库内 ElaContentDialog 以蒙层几何计算位置，个别情况下会偏移，这里双保险对齐
    const auto centerOnParent = [dialog]() {
        if (QWidget* parent = dialog->parentWidget())
        {
            const QPoint globalCenter = parent->mapToGlobal(parent->rect().center());
            dialog->move(globalCenter.x() - dialog->width() / 2,
                         globalCenter.y() - dialog->height() / 2);
        }
    };
    centerOnParent();

    const auto updateActionHint = [actionHint]() {
        const GlobalInformation& g = g_GlobalInformation;
        const QString actionText = (g.closeAction == QStringLiteral("quit"))
                                       ? QCoreApplication::translate("SimpleFrpPanel", "直接退出")
                                       : QCoreApplication::translate("SimpleFrpPanel", "后台运行");
        actionHint->setText(g.confirmOnClose
                                ? QCoreApplication::translate("SimpleFrpPanel", "当前：每次关闭窗口都会弹出询问")
                                : QString(QCoreApplication::translate("SimpleFrpPanel", "当前：关闭窗口不再询问，将直接%1"
                                                      "（打开上方开关可恢复询问）"))
                                      .arg(actionText));
    };
    updateActionHint();

    // 开关即改即存，并同步底部提示
    QObject::connect(confirmSwitch, &ElaToggleSwitch::toggled, dialog,
                     [updateActionHint](bool checked) {
                         g_GlobalInformation.confirmOnClose = checked;
                         g_GlobalInformation.saveSettings();
                         updateActionHint();
                     });

    // 语言选择即存（重启后生效）
    QObject::connect(langCombo, QOverload<int>::of(&ElaComboBox::currentIndexChanged), dialog,
                     [](int index) {
                         g_GlobalInformation.language = (index == 1) ? QStringLiteral("en")
                                                                     : QStringLiteral("zh_CN");
                         g_GlobalInformation.saveSettings();
                     });

    // 更新代理即改即存
    QObject::connect(proxyEdit, &ElaLineEdit::editingFinished, dialog, [proxyEdit]() {
        g_GlobalInformation.updateProxy = proxyEdit->text().trimmed();
        g_GlobalInformation.saveSettings();
    });

    // 空文本按钮隐藏（ElaContentDialog 内部按钮）
    const QList<ElaPushButton*> buttons = dialog->findChildren<ElaPushButton*>();
    for (ElaPushButton* button : buttons)
    {
        if (button->text().isEmpty())
        {
            button->setVisible(false);
        }
    }

    // 开关的"置位动画"在对话框弹出前预先走完：打开即见旋钮停在正确端点
    {
        QEventLoop pumpLoop;
        QTimer::singleShot(350, &pumpLoop, &QEventLoop::quit);
        pumpLoop.exec();
    }

    // 显示后再对齐一次（库内 moveToCenter 在 show 时执行，这里兜底确保以父窗口中心为准）
    QTimer::singleShot(0, dialog, centerOnParent);

    dialog->exec();
    dialog->deleteLater();
}


