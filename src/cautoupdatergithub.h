#pragma once

#include <QFile>
#include <QNetworkAccessManager>
#include <QString>

#include <vector>

class CAutoUpdaterGithub : public QObject
{
public:
	struct VersionEntry {
		QString versionString;
		QString versionChanges;
		QString versionUpdateUrl;
	};

	using ChangeLog = std::vector<VersionEntry>;

	struct UpdateStatusListener {
		// If no updates are found, the changelog is empty
		virtual void onUpdateAvailable(ChangeLog changelog) = 0;
		virtual void onUpdateDownloadProgress(float percentageDownloaded) = 0;
		virtual void onUpdateDownloadFinished() = 0;
		virtual void onUpdateError(QString errorMessage) = 0;
	};

public:
	// If the string comparison functior is not supplied, case-insensitive natural sorting is used (using QCollator)
	CAutoUpdaterGithub(const QString& githubRepositoryAddress,
					   const QString& currentVersionString);

	CAutoUpdaterGithub& operator=(const CAutoUpdaterGithub& other) = delete;

	void setUpdateStatusListener(UpdateStatusListener* listener);

	void checkForUpdates();
	void downloadAndInstallUpdate(const QString& updateUrl);

private:
	void updateCheckRequestFinished();
	void updateDownloaded();
	void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void onNewDataDownloaded();

private:
	QFile _downloadedBinaryFile;
	const QString _updatePageAddress;
	const QString _currentVersionString;
    static const QString _versionFormat;
    
	UpdateStatusListener* _listener = nullptr;

	QNetworkAccessManager _networkManager;
};

