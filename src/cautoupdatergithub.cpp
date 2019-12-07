#include "cautoupdatergithub.h"
#include "updateinstaller.hpp"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>


#include <assert.h>

#if !defined UPDATE_FILE_EXTENSION
    #if defined _WIN32
    #define UPDATE_FILE_EXTENSION QLatin1String(".exe")
    #elif defined __APPLE__
    #define UPDATE_FILE_EXTENSION QLatin1String(".dmg")
    #else
    #define UPDATE_FILE_EXTENSION QLatin1String(".AppImage")
    #endif
#endif

#if !defined OS_JSON
    #if defined _WIN32
    #define OS_JSON "Windows"
    #elif defined __APPLE__
    #define OS_JSON "MacOS"
    #else
    #define OS_JSON ""
    #endif
#endif

const QString CAutoUpdaterGithub::_versionFormat("yyyy.MM.dd");

CAutoUpdaterGithub::CAutoUpdaterGithub(const QString& githubRepositoryAddress, const QString& currentVersionString) :
	_updatePageAddress(githubRepositoryAddress),
	_currentVersionString(currentVersionString)
{
	assert(githubRepositoryAddress.contains("https://raw.githubusercontent.com/"));
	assert(!currentVersionString.isEmpty());
        qDebug() << QSslSocket::supportsSsl() << QSslSocket::sslLibraryBuildVersionString() << QSslSocket::sslLibraryVersionString();
}

void CAutoUpdaterGithub::setUpdateStatusListener(UpdateStatusListener* listener)
{
	_listener = listener;
}

void CAutoUpdaterGithub::checkForUpdates()
{
	QNetworkReply * reply = _networkManager.get(QNetworkRequest(QUrl(_updatePageAddress)));
	if (!reply)
	{
		if (_listener)
			_listener->onUpdateError("Network request rejected.");
		return;
	}

	connect(reply, &QNetworkReply::finished, this, &CAutoUpdaterGithub::updateCheckRequestFinished, Qt::UniqueConnection);
}

void CAutoUpdaterGithub::downloadAndInstallUpdate(const QString& updateUrl)
{
	assert(!_downloadedBinaryFile.isOpen());

	_downloadedBinaryFile.setFileName(QDir::tempPath() + '/' + QCoreApplication::applicationName() + UPDATE_FILE_EXTENSION);
	if (!_downloadedBinaryFile.open(QFile::WriteOnly))
	{
		if (_listener)
			_listener->onUpdateError("Failed to open temporary file " + _downloadedBinaryFile.fileName());
		return;
	}

	QNetworkRequest request((QUrl(updateUrl)));
	request.setSslConfiguration(QSslConfiguration::defaultConfiguration()); // HTTPS
	request.setMaximumRedirectsAllowed(5);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	QNetworkReply * reply = _networkManager.get(request);
	if (!reply)
	{
		if (_listener)
			_listener->onUpdateError("Network request rejected.");
		return;
	}

	connect(reply, &QNetworkReply::readyRead, this, &CAutoUpdaterGithub::onNewDataDownloaded);
	connect(reply, &QNetworkReply::downloadProgress, this, &CAutoUpdaterGithub::onDownloadProgress);
	connect(reply, &QNetworkReply::finished, this, &CAutoUpdaterGithub::updateDownloaded, Qt::UniqueConnection);
}

void CAutoUpdaterGithub::updateCheckRequestFinished()
{
	auto reply = qobject_cast<QNetworkReply *>(sender());
	if (!reply)
		return;

	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		if (_listener)
			_listener->onUpdateError(reply->errorString());

		return;
	}

	if (reply->bytesAvailable() <= 0)
	{
		if (_listener)
			_listener->onUpdateError("No data downloaded.");
		return;
	}

    ChangeLog changelog;
    const auto data = QString(reply->readAll());
    const auto json = QJsonDocument::fromJson(data.toUtf8()).object();
    if (json.contains(OS_JSON) && json[OS_JSON].isObject()) {
        const auto osJson = json[OS_JSON].toObject();
        if (osJson.contains("version") && osJson["version"].isString() &&
        osJson.contains("url") && osJson["url"].isString()) {

            bool hasChangelog = osJson.contains("changelog") && osJson["changelog"].isString();
            auto version = osJson["version"].toString();
            auto versionDate = QDate::fromString(version, _versionFormat);
            auto currentVersionDate = QDate::fromString(_currentVersionString, _versionFormat);

            if (currentVersionDate < versionDate) {
               VersionEntry entry {
                   osJson["version"].toString(),
                   hasChangelog ? "<br />" + osJson["changelog"].toString().replace("\n", "<br />")
                                                                   : "",
                   osJson["url"].toString()
                };

                changelog.push_back(entry);
            }
        }
    }

	if (_listener)
		_listener->onUpdateAvailable(changelog);
}

void CAutoUpdaterGithub::updateDownloaded()
{
	_downloadedBinaryFile.close();

	auto reply = qobject_cast<QNetworkReply *>(sender());
	if (!reply)
		return;

	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		if (_listener)
			_listener->onUpdateError(reply->errorString());

		return;
	}

	if (_listener) {
	    _listener->onUpdateDownloadProgress(100);
            _listener->onUpdateDownloadFinished();
        }

	if (!UpdateInstaller::install(_downloadedBinaryFile.fileName()) && _listener)
		_listener->onUpdateError("Failed to launch the downloaded update.");
}

void CAutoUpdaterGithub::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
	if (_listener)
		_listener->onUpdateDownloadProgress(bytesReceived < bytesTotal ? bytesReceived * 100 / (float)bytesTotal : 100.0f);
}

void CAutoUpdaterGithub::onNewDataDownloaded()
{
	auto reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply)
		return;

	_downloadedBinaryFile.write(reply->readAll());
}
