#include "MainWindow.h"
#include "./ui_MainWindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, settings("Manicorp", "OblivionVoiceFrenchiser", this)
	, correspondingRaces{
		{ "high_elf", { "haut_elfe", "imperial" } },
		{ "wood_elf", { "haut_elfe", "imperial" } },
		{ "redguard", { "rougegarde", "imperial", "haut elfe", "nordique" } }, //je sais pas encore nordique ou haut elfe
		{ "dark_elf", { "haut_elfe", "imperial" } },
		{ "argonian", { "argonien", "haut_elfe" } },
		{ "khajiit", { "argonien", "haut_elfe" } },
		{ "nord", { "nordique", "rougegarde", "imperial", "haut elfe" } }, //je sais pas encore rougegarde ou impérial
		{ "orc", { "nordique", "rougegarde", "imperial" } }, //je sais pas encore rougegarde ou impérial
		{ "breton", { "imperial", "haut_elfe", "nordique" } },
		{ "imperial", { "imperial", "haut_elfe", "rougegarde", "nordique" } },
		{ "dark_seducer", { "vil_séducteur" } },
		{ "golden_saint", { "saint_doré" } },
		{ "sheogorath", { "shéogorath" } },
		{ "dremora", { "drémora" } }
	}
	, subFolders{ "altvoice", "beggar" }
{
	ui->setupUi(this);

	ui->s1InputFolderLineEdit->blockSignals(true);
	ui->s1InputFolderLineEdit->setText(settings.value("s1InputFolder", QDir::homePath()).toString());
	ui->s1InputFolderLineEdit->blockSignals(false);
	ui->s2InputFolderLineEdit->blockSignals(true);
	ui->s2InputFolderLineEdit->setText(settings.value("s2InputFolder", QDir::homePath()).toString());
	ui->s2InputFolderLineEdit->blockSignals(false);
	ui->s3OutputFolderLineEdit->blockSignals(true);
	ui->s3OutputFolderLineEdit->setText(settings.value("s3OutputFolder", QDir::homePath()).toString());
	ui->s3OutputFolderLineEdit->blockSignals(false);

	ui->progressBar->setVisible(false);

	connect(&s1ProcessFolderFutureWatcher, &QFutureWatcher<QList<WemFile>>::finished, this, &MainWindow::s1ProcessFinished);
	connect(&s1ProcessFilesFutureWatcher, &QFutureWatcher<WemFile>::progressValueChanged, ui->progressBar, &QProgressBar::setValue);
	connect(&s2ProcessVoiceFolderFutureWatcher, &QFutureWatcher<QStringList>::finished, this, &MainWindow::s2ProcessVoiceFolderFinished);
	connect(&s2ProcessVoiceFilesFutureWatcher, &QFutureWatcher<QString>::finished, this, &MainWindow::s2ProcessVoiceFilesFinished);
	connect(&s3ProcessVoiceFutureWatcher, &QFutureWatcher<WemFile>::progressValueChanged, ui->progressBar, &QProgressBar::setValue);
	connect(&s3ProcessVoiceFutureWatcher, &QFutureWatcher<MatchingFile>::finished, this, &MainWindow::s3ProcessVoicesFinished);
	connect(&s3ProcessReplaceVoiceFutureWatcher, &QFutureWatcher<void>::progressValueChanged, ui->progressBar, &QProgressBar::setValue);
	connect(&s3ProcessReplaceVoiceFutureWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::s3ProcessReplaceVoicesFinished);

	QDir::current().mkdir("logs");
}

MainWindow::~MainWindow()
{
	s1ProcessFolderFuture.cancel();
	s1ProcessFolderFutureWatcher.cancel();
	s1ProcessFilesFuture.cancel();
	s1ProcessFilesFutureWatcher.cancel();
	s2ProcessVoiceFolderFuture.cancel();
	s2ProcessVoiceFolderFutureWatcher.cancel();
	s2ProcessVoiceFilesFuture.cancel();
	s2ProcessVoiceFilesFutureWatcher.cancel();
	s3ProcessVoiceFuture.cancel();
	s3ProcessVoiceFutureWatcher.cancel();
	s3ProcessReplaceVoiceFuture.cancel();
	s3ProcessReplaceVoiceFutureWatcher.cancel();

	delete ui;
}

QStringList MainWindow::getFilesInFolder(const QString& folderPath, const QStringList& fileSuffixes)
{
	QDirIterator it(folderPath, fileSuffixes, QDir::Files, QDirIterator::Subdirectories);
	QStringList files;
	while (it.hasNext())
	{
		QString file = it.next();
		files << file;
		if (s1ProcessFolderFuture.isCanceled())
		{
			return QStringList();
		}
	}
	return files;
}

QList<WemFile> MainWindow::s1ProcessFolder(const QString& folderPath)
{
	QStringList files = getFilesInFolder(folderPath, QStringList() << "*.txtp");
	if (s1ProcessFolderFuture.isCanceled())
	{
		return QList<WemFile>();
	}
	ui->progressBar->setMaximum(files.size());
	s1ProcessFilesFuture = QtConcurrent::mapped(files,
		[this](const QString& filePath)
		{
			return this->s1ProcessFile(filePath);
		}
	);
	s1ProcessFilesFutureWatcher.setFuture(s1ProcessFilesFuture);

	return s1ProcessFilesFuture.results();
}

WemFile MainWindow::s1ProcessFile(const QString& filePath)
{
	WemFile result;
	QFile file(filePath);
	if (file.open(QFile::ReadOnly))
	{
		QString fileContent = file.readAll();
		file.close();
		QStringList codec = fileContent.split("ulPluginID:");
		codec = codec[1].split("[");
		codec = codec[1].split("]");
		QString fileName = QFileInfo(filePath).baseName();
		QStringList wemData = fileContent.split(".wem");
		QString wemFileName = wemData[0].split("/").last();
		result.filePath = filePath;
		result.id = wemFileName.toUInt();
		result.codec = codec[0];
	}
	return result;
}

QStringList MainWindow::s2ProcessVoiceFolder(const QString& folderPath)
{
	return getFilesInFolder(folderPath, QStringList() << "*.mp3" << "*.wem");
}

VoiceFile MainWindow::s2ProcessVoiceFile(const QString& filePath)
{
	VoiceFile result;
	result.filePath = filePath;
	QString fileName = filePath.split(".esp/").last();
	fileName = fileName.split(".esm/").last();
	QStringList elements = fileName.split("/");
	fileName.replace("/", "_");
	fileName.replace(" ", "_");
	fileName = "Play_" + fileName;
	result.correspondingName = fileName.split("." + QFileInfo(fileName).suffix()).first();
	return result;
}

MatchingFile MainWindow::s3ProcessVoice(const WemFile& wemFile)
{
	MatchingFile result;
	result.wemFile = &wemFile;
	QString outputFolder = ui->s3OutputFolderLineEdit->text();

	QString currentFilePath = wemFile.filePath;
	currentFilePath.replace("v.txtp", ".txtp");
	QString baseName = QFileInfo(currentFilePath).baseName();

	if (voiceFileByBaseNames.contains(baseName))
	{
		result.found = true;
		result.voiceFile = voiceFileByBaseNames[baseName];
		return result;
	}
	else
	{
		baseName.replace("_alt01", "");
		baseName.replace("_elf_f_0300", "_1");
		for (const QString& subFolder : subFolders)
		{
			baseName.replace("_" + subFolder, "");
		}
		for (const QString& correspondingRace : correspondingRaces.keys())
		{
			if (baseName.contains("_" + correspondingRace + "_"))
			{
				for (const QString& transCorrespondingRace : correspondingRaces[correspondingRace])
				{
					QString transName = baseName;
					transName.replace("_" + correspondingRace + "_", "_" + transCorrespondingRace + "_");
					if (voiceFileByBaseNames.contains(transName))
					{
						result.found = true;
						result.voiceFile = voiceFileByBaseNames[transName];
						return result;
					}
				}
			}
		}

		//Sauvage !!
		//QStringList fileNameParts = baseName.split("_");
		//baseName = fileNameParts.last();
		//for (int i = fileNameParts.count() - 2 ; i >= 0 ; i--)
		//{
		//	if (fileNameParts[i] == "m" || fileNameParts[i] == "f")
		//	{
		//		break;
		//	}
		//	baseName = fileNameParts[i] + "_" + baseName;
		//}
		//for (const QString& voiceFileByBaseName : voiceFileByBaseNames.keys())
		//{
		//	if (voiceFileByBaseName.contains(baseName))
		//	{
		//		result.found = true;
		//		result.voiceFile = voiceFileByBaseNames[voiceFileByBaseName];
		//		return result;
		//	}
		//}
	}

	return result;
}

void MainWindow::replaceVoice(const MatchingFile& matchingFile, const QString& outputFolder)
{
	QFile::copy(matchingFile.voiceFile->filePath, outputFolder + "/" + QString::number(matchingFile.wemFile->id) + ".wem");
}

void MainWindow::on_s1InputFolderPushButton_clicked()
{
	QString dir = QFileDialog::getExistingDirectory(this, tr("Ouvrir le dossier"),
		settings.value("s1InputFolder", QDir::homePath()).toString(),
		QFileDialog::ShowDirsOnly
		| QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty())
	{
		settings.setValue("s1InputFolder", dir);
		ui->s1InputFolderLineEdit->setText(dir);
	}
}

void MainWindow::on_s1ProcessPushButton_clicked()
{
	QString inputFolder = ui->s1InputFolderLineEdit->text();
	if (inputFolder.isEmpty())
	{
		QMessageBox::warning(this, tr("Erreur"), tr("Veuillez sélectionner un dossier d'entrée."));
		return;
	}
	QDir dir(inputFolder);
	if (!dir.exists())
	{
		QMessageBox::warning(this, tr("Erreur"), tr("Le dossier d'entrée n'existe pas."));
		return;
	}

	settings.setValue("s1InputFolder", inputFolder);
	ui->s1GroupBox->setEnabled(false);

	ui->progressBar->setRange(0, 0);
	ui->progressBar->setVisible(true);

	s1ProcessFolderFuture = QtConcurrent::run(&MainWindow::s1ProcessFolder, this, ui->s1InputFolderLineEdit->text());
	s1ProcessFolderFutureWatcher.setFuture(s1ProcessFolderFuture);
}

void MainWindow::s1ProcessFinished()
{
	ui->s1GroupBox->setEnabled(true);
	ui->progressBar->setVisible(false);
	ui->s2GroupBox->setEnabled(true);

	wemFiles = s1ProcessFolderFuture.result();

	for (WemFile& wemFile : wemFiles)
	{
		QString baseName = QFileInfo(wemFile.filePath).baseName();
		wemByBaseNames[baseName] = &wemFile;
	}
}

void MainWindow::on_s2InputFolderPushButton_clicked()
{
	QString dir = QFileDialog::getExistingDirectory(this, tr("Ouvrir le dossier"),
		settings.value("s2InputFolder", QDir::homePath()).toString(),
		QFileDialog::ShowDirsOnly
		| QFileDialog::DontResolveSymlinks);
	if (!dir.isEmpty())
	{
		settings.setValue("s2InputFolder", dir);
		ui->s2InputFolderLineEdit->setText(dir);
	}
}

void MainWindow::on_s2ProcessPushButton_clicked()
{
	QString inputFolder = ui->s2InputFolderLineEdit->text();
	if (inputFolder.isEmpty())
	{
		QMessageBox::warning(this, tr("Erreur"), tr("Veuillez sélectionner un dossier d'entrée."));
		return;
	}
	QDir dir(inputFolder);
	if (!dir.exists())
	{
		QMessageBox::warning(this, tr("Erreur"), tr("Le dossier d'entrée n'existe pas."));
		return;
	}

	settings.setValue("s2InputFolder", inputFolder);
	ui->s2GroupBox->setEnabled(false);
	ui->progressBar->setRange(0, 0);
	ui->progressBar->setVisible(true);
	voiceFiles.clear();
	s2ProcessVoiceFolderFuture = QtConcurrent::run(&MainWindow::s2ProcessVoiceFolder, this, ui->s2InputFolderLineEdit->text());
	s2ProcessVoiceFolderFutureWatcher.setFuture(s2ProcessVoiceFolderFuture);
}

void MainWindow::s2ProcessVoiceFolderFinished()
{
	QStringList voiceFilePaths = s2ProcessVoiceFolderFuture.result();
	ui->progressBar->setMaximum(voiceFilePaths.size());

	s2ProcessVoiceFilesFuture = QtConcurrent::mapped(voiceFilePaths,
		[this](const QString& filePath)
		{
			return this->s2ProcessVoiceFile(filePath);
		}
	);
	s2ProcessVoiceFilesFutureWatcher.setFuture(s2ProcessVoiceFilesFuture);

	QFile frenchFilesLog("logs/frenchFiles.log");
	if (frenchFilesLog.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QTextStream out(&frenchFilesLog);
		for (const QString& filePath : voiceFilePaths)
		{
			QString shortFilePath = filePath;
			shortFilePath.replace(ui->s2InputFolderLineEdit->text() + "/", "");
			out << QFileInfo(filePath).fileName() << "\t\t\t" << shortFilePath << Qt::endl;
		}
		frenchFilesLog.close();
	}
}

void MainWindow::s2ProcessVoiceFilesFinished()
{
	ui->s2GroupBox->setEnabled(true);
	ui->progressBar->setVisible(false);

	voiceFiles = s2ProcessVoiceFilesFuture.results();

	QStringList voiceFileList;
	for (VoiceFile& voiceFile : voiceFiles)
	{
		voiceFileList << voiceFile.correspondingName;
		voiceFileByBaseNames[voiceFile.correspondingName] = &voiceFile;
	}

	ui->s3ReplaceVoicesGroupBox->setEnabled(true);

	ui->statusbar->showMessage(tr("Fichiers trouvés : ") + QString::number(voiceFiles.size()));
}

void MainWindow::on_s3OutputFolderPushButton_clicked()
{
	QString dir = QFileDialog::getExistingDirectory(this, tr("Ouvrir le dossier"),
		settings.value("s3OutputFolder", QDir::homePath()).toString(),
		QFileDialog::ShowDirsOnly
		| QFileDialog::DontResolveSymlinks);
	if (!dir.isEmpty())
	{
		settings.setValue("s3OutputFolder", dir);
		ui->s3OutputFolderLineEdit->setText(dir);
	}
}

void MainWindow::on_s3ReplaceVoicesPushButton_clicked()
{
	QString outputFolder = ui->s3OutputFolderLineEdit->text();
	if (outputFolder.isEmpty())
	{
		QMessageBox::warning(this, tr("Erreur"), tr("Veuillez sélectionner un dossier de sortie."));
		return;
	}
	QDir dir(outputFolder);
	if (!dir.exists())
	{
		QMessageBox::warning(this, tr("Erreur"), tr("Le dossier de sortie n'existe pas."));
		return;
	}

	settings.setValue("s3OutputFolder", outputFolder);
	ui->s3ReplaceVoicesGroupBox->setEnabled(false);
	ui->progressBar->setMaximum(wemFiles.size());
	ui->progressBar->setVisible(true);

	matchingFiles.clear();

	s3ProcessVoiceFuture = QtConcurrent::mapped(wemFiles,
		[this](const WemFile& wemFile)
		{
			return this->s3ProcessVoice(wemFile);
		}
	);
	s3ProcessVoiceFutureWatcher.setFuture(s3ProcessVoiceFuture);
}

void MainWindow::s3ProcessVoicesFinished()
{
	matchingFiles = s3ProcessVoiceFuture.results();
	ui->progressBar->setMaximum(matchingFiles.size());

	s3ProcessReplaceVoiceFuture = QtConcurrent::map(matchingFiles,
		[this](const MatchingFile& matchingFile)
		{
			if (matchingFile.found)
			{
				this->replaceVoice(matchingFile, ui->s3OutputFolderLineEdit->text());
			}
		}
	);
	s3ProcessReplaceVoiceFutureWatcher.setFuture(s3ProcessReplaceVoiceFuture);

	QFile foundFilesLog("logs/foundFiles.log");
	if (foundFilesLog.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QTextStream out(&foundFilesLog);
		for (const MatchingFile& matchingFile : matchingFiles)
		{
			if (matchingFile.found)
			{
				out << QFileInfo(matchingFile.wemFile->filePath).fileName() << "\t" << QFileInfo(matchingFile.voiceFile->filePath).fileName() << "\t" << matchingFile.wemFile->id << Qt::endl;
			}
		}
		foundFilesLog.close();
	}

	QFile missingFilesLog("logs/missingFiles.log");
	if (missingFilesLog.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QTextStream out(&missingFilesLog);
		for (const MatchingFile& matchingFile : matchingFiles)
		{
			if (!matchingFile.found)
			{
				out << QFileInfo(matchingFile.wemFile->filePath).fileName() << "\t[" << matchingFile.wemFile->id << "]" << Qt::endl;
			}
		}
		missingFilesLog.close();
	}
}

void MainWindow::s3ProcessReplaceVoicesFinished()
{
	ui->s3ReplaceVoicesGroupBox->setEnabled(true);
	ui->progressBar->setVisible(false);

	QMessageBox::information(this, tr("Terminé"), tr("Les voix ont été remplacées avec succès."));
}
