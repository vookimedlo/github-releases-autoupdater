#include "updateinstaller.hpp"

#include <QApplication>
#include <QDesktopServices>
#include <QUrl>

bool UpdateInstaller::install(const QString& downloadedUpdateFilePath)
{
    bool result = QDesktopServices::openUrl(QUrl("file:///" + downloadedUpdateFilePath, QUrl::TolerantMode));

    QApplication::quit();

    return result;
}
