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
		{ "wood_elf", "haut_elfe" },
		{ "redguard", "rougegarde" },
		{ "dark_elf", "haut_elfe" },
		{ "argonian", "argonien" },
		{ "khakiit", "argonien" },
		{ "nord", "nordique" },
		{ "orc", "nordique" },
		{ "breton", "imperial" },
		{ "imperial", "imperial" },
		{ "dark_seducer", "vil_séducteur" },
		{ "golden_saint", "saint_doré" },
		{ "sheogorath", "shéogorath" },
		{ "dremora", "drémora" }
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

	ui->s1ListWidget->setVisible(false);
	ui->s2ListWidget->setVisible(false);
	ui->progressBar->setVisible(false);

	connect(&s1ProcessFolderFutureWatcher, &QFutureWatcher<QList<WemFile>>::finished, this, &MainWindow::s1ProcessFinished);
	connect(&s1ProcessFilesFutureWatcher, &QFutureWatcher<WemFile>::progressValueChanged, ui->progressBar, &QProgressBar::setValue);
	connect(&s2ProcessVoiceFolderFutureWatcher, &QFutureWatcher<QStringList>::finished, this, &MainWindow::s2ProcessVoiceFolderFinished);
	connect(&s2ProcessVoiceFilesFutureWatcher, &QFutureWatcher<QString>::finished, this, &MainWindow::s2ProcessVoiceFilesFinished);
}

MainWindow::~MainWindow()
{
	s1ProcessFolderFuture.cancel();
	s1ProcessFolderFutureWatcher.cancel();
	s1ProcessFilesFuture.cancel();
	s1ProcessFilesFutureWatcher.cancel();

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

void MainWindow::replaceVoices()
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
	ui->progressBar->setRange(0, 0);
	//ui->progressBar->setVisible(true);

	QFile outputFile("output.txt");
	outputFile.open(QIODevice::WriteOnly | QIODevice::Text);
	QTextStream ts(&outputFile);

	for (const WemFile& wemFile : wemFiles)
	{
		QString baseName = QFileInfo(wemFile.filePath).baseName();
		if (voiceFileByBaseNames.contains(baseName))
		{
			VoiceFile* voiceFile = voiceFileByBaseNames[baseName];
			QString outputFilePath = outputFolder + "/" + QString::number(wemFile.id) + ".wem";
			QFile::copy(voiceFile->filePath, outputFilePath);
		}
		else
		{
			for (const QString& correspondingRace : correspondingRaces.keys())
			{
				baseName.replace(correspondingRace, correspondingRaces[correspondingRace]);
			}
			for (const QString& subFolder : subFolders)
			{
				baseName.replace("_" + subFolder, "");
			}

			if (voiceFileByBaseNames.contains(baseName))
			{
				VoiceFile* voiceFile = voiceFileByBaseNames[baseName];
				QString outputFilePath = outputFolder + "/" + QString::number(wemFile.id) + ".wem";
				QFile::copy(voiceFile->filePath, outputFilePath);
			}
			else
			{
				ui->s3MissingListWidget->addItem(QFileInfo(wemFile.filePath).baseName());
				ts << QFileInfo(wemFile.filePath).baseName() << Qt::endl;
			}
		}
	}
	outputFile.close();

	ui->statusbar->showMessage(tr("Fichiers manquant : ") + QString::number(ui->s3MissingListWidget->count()));

	//s3ReplaceVoicesFuture = QtConcurrent::run(&MainWindow::s3ReplaceVoicesInFolder, this, outputFolder);
	//s3ReplaceVoicesFutureWatcher.setFuture(s3ReplaceVoicesFuture);
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
	ui->s1ListWidget->clear();

	ui->progressBar->setRange(0, 0);
	ui->progressBar->setVisible(true);

	s1ProcessFolderFuture = QtConcurrent::run(&MainWindow::s1ProcessFolder, this, ui->s1InputFolderLineEdit->text());
	s1ProcessFolderFutureWatcher.setFuture(s1ProcessFolderFuture);
}

void MainWindow::s1ProcessFinished()
{
	ui->s1GroupBox->setEnabled(true);
	ui->s1ListWidget->setVisible(true);
	ui->progressBar->setVisible(false);
	ui->s2GroupBox->setEnabled(true);

	wemFiles = s1ProcessFolderFuture.result();

	QStringList items;
	for (WemFile& wemFile : wemFiles)
	{
		QString baseName = QFileInfo(wemFile.filePath).baseName();
		wemByBaseNames[baseName] = &wemFile;
		items << baseName + " : " + QString::number(wemFile.id);
	}
	ui->s1ListWidget->addItems(items);
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
	ui->s2ListWidget->clear();
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
	ui->s2ListWidget->addItems(voiceFileList);
	ui->s2ListWidget->setVisible(true);

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
	//ui->s3ReplaceVoicesGroupBox->setEnabled(false);
	ui->progressBar->setRange(0, 0);
	ui->progressBar->setVisible(true);

	replaceVoices();
}
