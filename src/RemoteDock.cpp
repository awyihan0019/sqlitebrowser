#include <QSslCertificate>
#include <QInputDialog>
#include <QFileInfo>

#include "RemoteDock.h"
#include "ui_RemoteDock.h"
#include "Settings.h"
#include "RemoteDatabase.h"
#include "RemoteModel.h"
#include "MainWindow.h"

RemoteDock::RemoteDock(MainWindow* parent)
    : QDialog(parent),
      ui(new Ui::RemoteDock),
      mainWindow(parent),
      remoteDatabase(parent->getRemote()),
      remoteModel(new RemoteModel(this, parent->getRemote()))
{
    ui->setupUi(this);

    // Set up model
    ui->treeStructure->setModel(remoteModel);

    // Initial setup
    reloadSettings();
}

RemoteDock::~RemoteDock()
{
    delete ui;
}

void RemoteDock::reloadSettings()
{
    // Load list of client certs
    ui->comboUser->clear();
    QStringList client_certs = Settings::getValue("remote", "client_certificates").toStringList();
    foreach(const QString& file, client_certs)
    {
        auto certs = QSslCertificate::fromPath(file);
        foreach(const QSslCertificate& cert, certs)
            ui->comboUser->addItem(cert.subjectInfo(QSslCertificate::CommonName).at(0), file);
    }
}

void RemoteDock::setNewIdentity()
{
    // Get identity
    QString identity = ui->comboUser->currentText();
    if(identity.isEmpty())
        return;

    // Get certificate file name
    QString cert = ui->comboUser->itemData(ui->comboUser->findText(identity), Qt::UserRole).toString();
    if(cert.isEmpty())
        return;

    // Open root directory. Get host name from client cert
    QString host = remoteDatabase.getInfoFromClientCert(cert, RemoteDatabase::CertInfoServer);
    remoteModel->setNewRootDir(QString("https://%1:5550/").arg(host), cert);

    // Enable buttons if necessary
    enableButtons();
}

void RemoteDock::fetchDatabase(const QModelIndex& idx)
{
    if(!idx.isValid())
        return;

    // Get item
    const RemoteModelItem* item = remoteModel->modelIndexToItem(idx);

    // Only open database file
    if(item->value(RemoteModelColumnType).toString() == "database")
        remoteDatabase.fetch(item->value(RemoteModelColumnUrl).toString(), RemoteDatabase::RequestTypeDatabase, remoteModel->currentClientCertificate());
}

void RemoteDock::enableButtons()
{
    bool db_opened = mainWindow->getDb().isOpen();
    bool logged_in = !remoteModel->currentClientCertificate().isEmpty();

    ui->buttonPushDatabase->setEnabled(db_opened && logged_in);
}

void RemoteDock::pushDatabase()
{
    // Ask for file name to save under. The default suggestion is the local file name. If it is a remote file (like when it initially was fetched using DB4S),
    // the extra bit of information at the end of the name gets removed first.
    QString name = QFileInfo(mainWindow->getDb().currentFile()).fileName();
    name = name.remove(QRegExp("_[0-9]+.remotedb$"));
    name = QInputDialog::getText(this, qApp->applicationName(),
                                 tr("Please enter the database name to push to."),
                                 QLineEdit::Normal,
                                 name);
    if(name.isEmpty())
        return;

    // Build push URL
    QString url = QString("https://%1:5550/").arg(remoteDatabase.getInfoFromClientCert(remoteModel->currentClientCertificate(), RemoteDatabase::CertInfoServer));
    url.append(remoteDatabase.getInfoFromClientCert(remoteModel->currentClientCertificate(), RemoteDatabase::CertInfoUser));
    url.append("/");
    url.append(name);

    // Push database
    remoteDatabase.push(mainWindow->getDb().currentFile(), url, remoteModel->currentClientCertificate());
}
