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

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QMessageBox>
#include <QMutexLocker>

#include "peconnectorsview.h"
#include "peutils.h"
#include "hashpopulatewidget.h"
#include "../debugdialog.h"

//////////////////////////////////////

PEConnectorsView::PEConnectorsView(QWidget * parent) : QWidget(parent) 
{
	m_connectorCount = 0;

    QFile styleSheet(":/resources/styles/newpartseditor.qss");
    if (!styleSheet.open(QIODevice::ReadOnly)) {
        DebugDialog::debug("Unable to open :/resources/styles/newpartseditor.qss");
    } else {
    	this->setStyleSheet(styleSheet.readAll());
    }

	QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setSizeConstraint( QLayout::SetMinAndMaxSize );
	
    QLabel *explanation = new QLabel(tr("This is where you edit the connector metadata for the part"));
    mainLayout->addWidget(explanation);

    QFrame * numberFrame = new QFrame();
    QHBoxLayout * numberLayout = new QHBoxLayout();

    QLabel * label = new QLabel(tr("number of connectors:"));
    numberLayout->addWidget(label);

    m_numberEdit = new QLineEdit();
    QValidator *validator = new QIntValidator(1, 999, this);
    m_numberEdit->setValidator(validator);
    numberLayout->addWidget(m_numberEdit);
    connect(m_numberEdit, SIGNAL(editingFinished()), this, SLOT(connectorCountEntry()));

    numberLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));
    numberFrame->setLayout(numberLayout);
    mainLayout->addWidget(numberFrame);

	m_scrollArea = new QScrollArea;
	m_scrollArea->setWidgetResizable(true);
	m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	m_scrollFrame = new QFrame;
	m_scrollArea->setWidget(m_scrollFrame);

	mainLayout->addWidget(m_scrollArea);
	this->setLayout(mainLayout);

}

PEConnectorsView::~PEConnectorsView() {

}

void PEConnectorsView::initConnectors(QList<QDomElement> & connectorList) 
{
    if (m_scrollFrame) {
        m_scrollArea->setWidget(NULL);
        delete m_scrollFrame;
        m_scrollFrame = NULL;
    }

    m_connectorCount = connectorList.size();
    m_numberEdit->setText(QString::number(m_connectorCount));

	m_scrollFrame = new QFrame(this);
	m_scrollFrame->setObjectName("NewPartsEditorConnectors");
	QVBoxLayout *scrollLayout = new QVBoxLayout();

    int ix = 0;
    foreach (QDomElement connector, connectorList) {
        QWidget * widget = PEUtils::makeConnectorForm(connector, ix++, this, true);
        scrollLayout->addWidget(widget);
    }

    scrollLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));
    m_scrollFrame->setLayout(scrollLayout);

    m_scrollArea->setWidget(m_scrollFrame);
}

void PEConnectorsView::nameEntry() {
	QLineEdit * lineEdit = qobject_cast<QLineEdit *>(sender());
	if (lineEdit != NULL && lineEdit->isModified()) {
		changeConnector();
	}
}

void PEConnectorsView::typeEntry() {
    changeConnector();
}

void PEConnectorsView::descriptionEntry() {
	QLineEdit * lineEdit = qobject_cast<QLineEdit *>(sender());
	if (lineEdit != NULL && lineEdit->isModified()) {
		changeConnector();
	}
}

void PEConnectorsView::connectorCountEntry() {
    if (!m_mutex.tryLock(1)) return;            // need the mutex because multiple editingFinished() signals can be triggered more-or-less at once
   
    QLineEdit * lineEdit = qobject_cast<QLineEdit *>(sender());
    if (lineEdit != NULL && lineEdit->isModified()) {
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
    if (!PEUtils::fillInMetadata(senderIndex, m_scrollFrame, cmd)) return;

    QList<ConnectorMetadata *> cmdList;
    cmdList.append(&cmd);
    emit removedConnectors(cmdList);
}


void PEConnectorsView::changeConnector() {
    bool ok;
    int senderIndex = sender()->property("index").toInt(&ok);
    if (!ok) return;

    ConnectorMetadata cmd;
    if (!PEUtils::fillInMetadata(senderIndex, m_scrollFrame, cmd)) return;

    emit connectorMetadataChanged(&cmd);
}

