#include "widget.h"

#include <QHostInfo>
#include <QNetworkInterface>
#include <QGridLayout>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , hostCombo(new QComboBox)
    , portLineEdit(new QLineEdit)
    , getFortuneButton(new QPushButton("Get Fortune"))
    , setFortuneButton(new QPushButton("Set Fortune"))
    , tcpSocket(new QTcpSocket(this))
{
    qDebug() << "Constructor is called";
    hostCombo->setEditable(true);
    // find out name of this machine
    QString name = QHostInfo::localHostName();
    if (!name.isEmpty()) {
        hostCombo->addItem(name);
        QString domain = QHostInfo::localDomainName();
        if (!domain.isEmpty())
            hostCombo->addItem(name + QChar('.') + domain);
    }
    if (name != QLatin1String("localhost"))
        hostCombo->addItem(QString("localhost"));
    // find out IP addresses of this machine
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    // add non-localhost addresses
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (!ipAddressesList.at(i).isLoopback())
            hostCombo->addItem(ipAddressesList.at(i).toString());
    }
    // add localhost addresses
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (ipAddressesList.at(i).isLoopback())
            hostCombo->addItem(ipAddressesList.at(i).toString());
    }

    portLineEdit->setValidator(new QIntValidator(1, 65535, this));

    auto hostLabel = new QLabel("Server name:");
    hostLabel->setBuddy(hostCombo);
    auto portLabel = new QLabel("Server port:");
    portLabel->setBuddy(portLineEdit);

    fortuneLineEdit = new QLineEdit("This examples requires that you run the "
                                "Fortune Server example as well.");

    getFortuneButton->setDefault(true);
    getFortuneButton->setEnabled(false);

    auto quitButton = new QPushButton("Quit");

    in.setDevice(tcpSocket);
    in.setVersion(QDataStream::Qt_4_0);

    connect(hostCombo, &QComboBox::editTextChanged,
            this, &Widget::enableFortuneButtons);
    connect(portLineEdit, &QLineEdit::textChanged,
            this, &Widget::enableFortuneButtons);
    connect(fortuneLineEdit, &QLineEdit::textChanged,
            this, &Widget::enableFortuneButtons);
    connect(getFortuneButton, &QAbstractButton::clicked,
            this, &Widget::getFortune);
    connect(setFortuneButton, &QAbstractButton::clicked,
            this, &Widget::setFortune);
    connect(quitButton, &QAbstractButton::clicked, this, &QWidget::close);

    connect(tcpSocket, &QAbstractSocket::connected, this, &Widget::requestNewFortune);
    connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &Widget::displayError);

    QGridLayout *mainLayout = new QGridLayout(this);
    mainLayout->addWidget(hostLabel, 0, 0);
    mainLayout->addWidget(hostCombo, 0, 1);
    mainLayout->addWidget(portLabel, 1, 0);
    mainLayout->addWidget(portLineEdit, 1, 1);
    mainLayout->addWidget(fortuneLineEdit, 2, 0, 1, 2);
    mainLayout->addWidget(getFortuneButton, 3, 1, 1, 1);
    mainLayout->addWidget(setFortuneButton, 4, 1, 1, 1);
    mainLayout->addWidget(quitButton, 3, 0, 1, 1);

    portLineEdit->setFocus();

    enableFortuneButtons();
}

Widget::~Widget()
{

}

void Widget::getFortune()
{
    getFortuneFlag = true;
    openConnection();
}

void Widget::setFortune()
{
    setFortuneFlag = true;
    openConnection();
}

void Widget::openConnection()
{
    qDebug() << "Open connection is called";
    getFortuneButton->setEnabled(false);
    setFortuneButton->setEnabled(false);
    tcpSocket->abort();
    tcpSocket->connectToHost(hostCombo->currentText(),
                             portLineEdit->text().toInt());
}

void Widget::requestNewFortune()
{
    qDebug() << "New fortune is requested";
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_10);

    if (getFortuneFlag) {
        getFortuneFlag = false;
        out << READ_FORTUNE_MARKER;
        connect(tcpSocket, &QAbstractSocket::readyRead,
                this, &Widget::readFortune);
    } else if (setFortuneFlag) {
        setFortuneFlag = false;
        out << WRITE_FORTUNE_MARKER;
        out << fortuneLineEdit->text();
    } else {
        qDebug() << "No action is required";
    }


    tcpSocket->write(block);
    tcpSocket->flush();
}

void Widget::readFortune()
{
    qDebug() << "Read fortune is called";
    in.startTransaction();

    QString nextFortune;
    in >> nextFortune;

    if (!in.commitTransaction())
        return;

    if (nextFortune == currentFortune) {
        getFortuneFlag = true;
        QTimer::singleShot(0, this, &Widget::openConnection);
        return;
    }

    currentFortune = nextFortune;
    fortuneLineEdit->setText(currentFortune);
    getFortuneButton->setEnabled(true);
    setFortuneButton->setEnabled(true);
    disconnect(tcpSocket, &QAbstractSocket::readyRead,
               this, &Widget::readFortune);
}

void Widget::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        break;
    case QAbstractSocket::HostNotFoundError:
        QMessageBox::information(this, tr("Fortune Client"),
                                 tr("The host was not found. Please check the "
                                    "host name and port settings."));
        break;
    case QAbstractSocket::ConnectionRefusedError:
        QMessageBox::information(this, tr("Fortune Client"),
                                 tr("The connection was refused by the peer. "
                                    "Make sure the fortune server is running, "
                                    "and check that the host name and port "
                                    "settings are correct."));
        break;
    default:
        QMessageBox::information(this, tr("Fortune Client"),
                                 tr("The following error occurred: %1.")
                                 .arg(tcpSocket->errorString()));
    }

    getFortuneButton->setEnabled(true);
    setFortuneButton->setEnabled(true);
}

void Widget::enableFortuneButtons()
{
    getFortuneButton->setEnabled(!hostCombo->currentText().isEmpty() &&
                                 !portLineEdit->text().isEmpty() &&
                                 !fortuneLineEdit->text().isEmpty());
    setFortuneButton->setEnabled(!hostCombo->currentText().isEmpty() &&
                                 !portLineEdit->text().isEmpty() &&
                                 !fortuneLineEdit->text().isEmpty());

}
