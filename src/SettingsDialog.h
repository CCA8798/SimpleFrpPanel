#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QWidget>

// 应用设置对话框（右上角齿轮按钮 / 以后可扩展更多设置项）。
// 设置值直接读写 g_GlobalInformation 并持久化到 config.ini。
void showSettingsDialog(QWidget* parent);

#endif // SETTINGSDIALOG_H
