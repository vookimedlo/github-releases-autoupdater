#include "updateinstaller.hpp"

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>

#define report_error(message) {qInfo() << message; return false;}

bool UpdateInstaller::install(const QString& downloadedUpdateFilePath)
{
	if (!downloadedUpdateFilePath.endsWith(".dmg", Qt::CaseInsensitive))
		report_error("Cannot install update:" << downloadedUpdateFilePath << "is not a DMG.");

	const QString applicationBinaryName = QFileInfo(QApplication::applicationFilePath()).completeBaseName();
	if (applicationBinaryName.isEmpty())
		report_error("Failed to determine the application binary name.");

    QMessageBox msg(
            QMessageBox::Information,
            "Update ready!",
            "Application will be closed.\nPlease, update the application in the window that will be opened.",
            QMessageBox::Ok,
            qApp->activeWindow());
    msg.exec();

	QProcess hdiutil;
	hdiutil.start("hdiutil", {"attach", downloadedUpdateFilePath});
	if (!hdiutil.waitForFinished(60000))
		report_error("hdiutil either didn't run or is taking too long.");

	const QString output = hdiutil.readAllStandardOutput();
	const auto mountedVolumePathIndex = output.indexOf("/Volumes/");
	if (mountedVolumePathIndex == -1)
		report_error("hdiutil didn't succeed mounting the DMG" << downloadedUpdateFilePath);

	const QString volumePath = output.mid(mountedVolumePathIndex, -1).trimmed();
	const QString bundlePath = volumePath + '/' + applicationBinaryName + ".app";
	if (!QFileInfo::exists(bundlePath))
		report_error("No bundle found at" << bundlePath);

	//std::system(("cp -R " + bundlePath + " " + QString(QApplication::applicationDirPath() + "/../../").replace(" ", "\\ ")).toUtf8().constData());

	QApplication::quit();

	return true;
}
