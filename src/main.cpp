#include <QApplication>
#include <QGuiApplication>
#include <QTranslator>

#include "ElaApplication.h"

#include "Information.h"
#include "MainWindow.h"

int main(int argc, char* argv[])
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#endif
    QApplication app(argc, argv);
    eApp->init();

    // 界面语言：读 config.ini，加载内嵌翻译（SimpleFrpPanel_en.qm）。
    // 语言在设置页切换，重启后生效。
    g_GlobalInformation.loadSettings();
    if (g_GlobalInformation.language == QStringLiteral("en"))
    {
        QTranslator* translator = new QTranslator(&app);
        if (translator->load(QStringLiteral(":/translations/SimpleFrpPanel_en.qm")))
        {
            app.installTranslator(translator);
        }
    }

    MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
}
