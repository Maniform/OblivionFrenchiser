#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QFuture>
#include <QFutureWatcher>
#include <QSettings>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct WemFile
{
	QString filePath;
	unsigned int id;
	QString codec;
};

struct VoiceFile
{
	QString filePath;
	QString correspondingName;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

	QSettings settings;

	const QHash<QString, QString> correspondingRaces;

	QList<WemFile> wemFiles;
	QList<VoiceFile> voiceFiles;

	QHash<QString, WemFile*> wemByCodec;
	QHash<QString, WemFile*> wemByBaseNames;
	QHash<QString, VoiceFile*> voiceFileByBaseNames;

	QStringList getFilesInFolder(const QString& folderPath, const QStringList& fileSuffixes);

    QFuture<QList<WemFile>> s1ProcessFolderFuture;
	QFutureWatcher<QList<WemFile>> s1ProcessFolderFutureWatcher;
	QList<WemFile> s1ProcessFolder(const QString& folderPath);
	QFuture<WemFile> s1ProcessFilesFuture;
	QFutureWatcher<WemFile> s1ProcessFilesFutureWatcher;
	WemFile s1ProcessFile(const QString& filePath);

	QFuture<QStringList> s2ProcessVoiceFolderFuture;
	QFutureWatcher<QStringList> s2ProcessVoiceFolderFutureWatcher;
	QStringList s2ProcessVoiceFolder(const QString& folderPath);
	QFuture<VoiceFile> s2ProcessVoiceFilesFuture;
	QFutureWatcher<VoiceFile> s2ProcessVoiceFilesFutureWatcher;
	VoiceFile s2ProcessVoiceFile(const QString& filePath);

	void replaceVoices();

private slots:
	void on_s1InputFolderPushButton_clicked();
	void on_s1ProcessPushButton_clicked();
	void s1ProcessFinished();

	void on_s2InputFolderPushButton_clicked();
	void on_s2ProcessPushButton_clicked();
	void s2ProcessVoiceFolderFinished();
	void s2ProcessVoiceFilesFinished();

	void on_s3OutputFolderPushButton_clicked();
	void on_s3ReplaceVoicesPushButton_clicked();
};
#endif // MAINWINDOW_H
