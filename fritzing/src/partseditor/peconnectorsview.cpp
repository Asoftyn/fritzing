/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2012 Fachhochschule Potsdam - http://fh-potsdam.de

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fritzing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fritzing.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************

$Revision$:
$Author$:
$Date$

********************************************************************/


/**************************************

TODO:

    would be nice to have a change all radios function

**************************************/


#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
#include <QMessageBox>
#include <QMutexLocker>

#include "peconnectorsview.h"
#include "peutils.h"
#include "hashpopulatewidget.h"
#include "../debugdialog.h"

//////////////////////////////////////

PEConnectorsView::PEConnectorsView(QWidget * parent) : QScrollArea(parent) 
{
    m_mainFrame = NULL;
	this->setWidgetResizable(true);
	this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QFile styleSheet(":/resources/styles/newpartseditor.qss");
    if (!styleSheet.open(QIODevice::ReadOnly)) {
        DebugDialog::debug("Unable to open :/resources/styles/newpartseditor.qss");
    } else {
    	this->setStyleSheet(styleSheet.readAll());
    }
}

PEConnectorsView::~PEConnectorsView() {

}

void PEConnectorsView::initConnectors(QList<QDomElement> & connectorList) 
{
    if (m_mainFrame) {
        this->setWidget(NULL);
        delete m_mainFrame;
        m_mainFrame = NULL;
    }

    m_connectorCount = connectorList.size();

	m_mainFrame = new QFrame(this);
	m_mainFrame->setObjectName("NewPartsEditorConnectors");
	QVBoxLayout *mainLayout = new QVBoxLayout(m_mainFrame);
    mainLayout->setSizeConstraint( QLayout::SetMinAndMaxSize );

    QLabel *explanation = new QLabel(tr("This is where you edit the connector metadata for the part"));
    mainLayout->addWidget(explanation);

    QFrame * numberFrame = new QFrame();
    QHBoxLayout * numberLayout = new QHBoxLayout();

    QLabel * label = new QLabel(tr("number of connectors:"));
    numberLayout->addWidget(label);

    QLineEdit * numberEdit = new QLineEdit();
    numberEdit->setText(QString::number(m_connectorCount));
    QValidator *validator = new QIntValidator(1, 999, this);
    numberEdit->setValidator(validator);
    numberLayout->addWidget(numberEdit);
    connect(numberEdit, SIGNAL(editingFinished()), this, SLOT(connectorCountEntry()));

    numberLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));
    numberFrame->setLayout(numberLayout);
    mainLayout->addWidget(numberFrame);

    int ix = 0;
    foreach (QDomElement connector, connectorList) {
        QWidget * widget = PEUtils::makeConnectorForm(connector, ix++, this, true);
        mainLayout->addWidget(widget);
    }

    mainLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));
    m_mainFrame->setLayout(mainLayout);

    this->setWidget(m_mainFrame);
}

void PEConnectorsView::nameEntry() {
    changeConnector();
}

void PEConnectorsView::typeEntry() {
    changeConnector();
}

void PEConnectorsView::descriptionEntry() {
    changeConnector();
}

void PEConnectorsView::connectorCountEntry() {
    if (!m_mutex.tryLock(1)) return;            // need the mutex because multiple editingFinished() signals can be triggered more-or-less at once
   
    QLineEdit * lineEdit = qobject_cast<QLineEdit *>(sender());
    if (lineEdit != NULL) {
        int newCount = lineEdit->text().toInt();
        if (newCount != m_connectorCount) {
            m_connectorCount = newCount;
            emit connectorCountChanged(newCount);
        }
    }

    m_mutex.unlock();
}

void PEConnectorsView::removeConnector() {
    bool ok;
    int senderIndex = sender()->property("index").toInt(&ok);
    if (!ok) return;

    ConnectorMetadata cmd;
    if (!fillInMetadata(senderIndex, cmd)) return;

    QList<ConnectorMetadata *> cmdList;
    cmdList.append(&cmd);
    emit removedConnectors(cmdList);
}


void PEConnectorsView::changeConnector() {
    bool ok;
    int senderIndex = sender()->property("index").toInt(&ok);
    if (!ok) return;

    ConnectorMetadata cmd;
    if (!fillInMetadata(senderIndex, cmd)) return;

    emit connectorMetadataChanged(&cmd);
}

bool PEConnectorsView::fillInMetadata(int senderIndex, ConnectorMetadata & cmd)
{
    bool result = false;
    QList<QWidget *> widgets = m_mainFrame->findChildren<QWidget *>();
    foreach (QWidget * widget, widgets) {
        bool ok;
        int index = widget->property("index").toInt(&ok);
        if (!ok) continue;

        if (index != senderIndex) continue;

        QString type = widget->property("type").toString();
        if (type == "name") {
            QLineEdit * lineEdit = qobject_cast<QLineEdit *>(widget);
            if (lineEdit == NULL) continue;

            cmd.connectorName = lineEdit->text();
            cmd.connectorID = widget->property("id").toString();
            result = true;
        }
        else if (type == "radio") {
            QRadioButton * radioButton = qobject_cast<QRadioButton *>(widget);
            if (radioButton == NULL) continue;
            if (!radioButton->isChecked()) continue;

            cmd.connectorType = (Connector::ConnectorType) radioButton->property("value").toInt();
        }
        else if (type == "description") {
            QLineEdit * lineEdit = qobject_cast<QLineEdit *>(widget);
            if (lineEdit == NULL) continue;

            cmd.connectorDescription = lineEdit->text();
        }

    }
    return result;   
}
