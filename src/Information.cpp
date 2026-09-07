//
// Created by Administrator on 2026/9/7.
//
#include "Information.h"

#include <QCoreApplication>
#include <QSettings>

namespace {
const QString kSettingsSection = QStringLiteral("general");
} // namespace

GlobalInformation g_GlobalInformation;

void GlobalInformation::loadSettings()
{
    QSettings settings(QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini"),
                       QSettings::IniFormat);
    settings.beginGroup(kSettingsSection);
    confirmOnClose = settings.value(QStringLiteral("confirmOnClose"), true).toBool();
    const QString action =
        settings.value(QStringLiteral("closeAction"), QStringLiteral("tray")).toString();
    closeAction = (action == QStringLiteral("quit")) ? QStringLiteral("quit")
                                                     : QStringLiteral("tray");
    language = settings.value(QStringLiteral("language"), QStringLiteral("zh_CN")).toString();
    updateProxy = settings.value(QStringLiteral("updateProxy")).toString();
    settings.endGroup();
}

void GlobalInformation::saveSettings() const
{
    QSettings settings(QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini"),
                       QSettings::IniFormat);
    settings.beginGroup(kSettingsSection);
    settings.setValue(QStringLiteral("confirmOnClose"), confirmOnClose);
    settings.setValue(QStringLiteral("closeAction"), closeAction);
    settings.setValue(QStringLiteral("language"), language);
    settings.setValue(QStringLiteral("updateProxy"), updateProxy);
    settings.endGroup();
    settings.sync();
}
