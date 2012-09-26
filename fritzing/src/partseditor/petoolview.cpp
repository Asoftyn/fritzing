/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2011 Fachhochschule Potsdam - http://fh-potsdam.de

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

#include "peutils.h"
#include "petoolview.h"
#include "pegraphicsitem.h"
#include "../utils/textutils.h"
#include "../utils/graphicsutils.h"
#include "../debugdialog.h"

#include <QHBoxLayout>
#include <QTextStream>
#include <QSplitter>
#include <QPushButton>
#include <QLineEdit>
#include <QFile>

//////////////////////////////////////

PEDoubleSpinBox::PEDoubleSpinBox(QWidget * parent) : QDoubleSpinBox(parent)
{
}

void PEDoubleSpinBox::stepBy(int steps)
{
    double amount;
    emit getSpinAmount(amount);
    setSingleStep(amount);
    QDoubleSpinBox::stepBy(steps);
}

//////////////////////////////////////

PEToolView::PEToolView(QWidget * parent) : QWidget(parent) 
{
    this->setObjectName("PEToolView");

    QFile styleSheet(":/resources/styles/newpartseditor.qss");
    if (!styleSheet.open(QIODevice::ReadOnly)) {
        DebugDialog::debug("Unable to open :/resources/styles/newpartseditor.qss");
    } else {
    	this->setStyleSheet(styleSheet.readAll());
    }

    m_pegi = NULL;

    QVBoxLayout * mainLayout = new QVBoxLayout;

    QSplitter * splitter = new QSplitter(Qt::Vertical);
    mainLayout->addWidget(splitter);

    QFrame * connectorsFrame = new QFrame;
    QVBoxLayout * connectorsLayout = new QVBoxLayout;

	QFrame * hFrame = new QFrame;
	QHBoxLayout * hFrameLayout = new QHBoxLayout;

    QLabel * label = new QLabel(tr("Connector List"));
	hFrameLayout->addWidget(label);

	hFrameLayout->addSpacing(PEUtils::Spacing);

    m_busModeBox = new QCheckBox(tr("Set Internal Connections"));
    m_busModeBox->setChecked(false);
    m_busModeBox->setToolTip(tr("Set this checkbox to edit internal connections by drawing wires"));
    connect(m_busModeBox, SIGNAL(clicked(bool)), this, SLOT(busModeChangedSlot(bool)));
    hFrameLayout->addWidget(m_busModeBox);

	hFrameLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));
	hFrame->setLayout(hFrameLayout);
    connectorsLayout->addWidget(hFrame);

    m_connectorListWidget = new QListWidget();
	connect(m_connectorListWidget, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this, SLOT(switchConnector(QListWidgetItem *, QListWidgetItem *)));

    connectorsLayout->addWidget(m_connectorListWidget);

    connectorsFrame->setLayout(connectorsLayout);
    splitter->addWidget(connectorsFrame);

    m_connectorInfoGroupBox = new QGroupBox;
    m_connectorInfoLayout = new QVBoxLayout;

	m_modeFrame = new QFrame;
	QHBoxLayout * modeLayout = new QHBoxLayout;

	m_pickModeButton = new QPushButton(tr("Select connector graphic"));
    m_pickModeButton->setToolTip(tr("Using the mouse pointer and mouse wheel, navigate to the SVG region you want to assign to the current connector, then mouse down to select it."));
	connect(m_pickModeButton, SIGNAL(clicked()), this, SLOT(pickModeChangedSlot()), Qt::DirectConnection);
    modeLayout->addWidget(m_pickModeButton);

	modeLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));
	m_modeFrame->setLayout(modeLayout);
    m_connectorInfoLayout->addWidget(m_modeFrame);             

    m_connectorInfoWidget = new QFrame;             // a placeholder for PEUtils::connectorForm
    m_connectorInfoLayout->addWidget(m_connectorInfoWidget);             

	m_terminalPointGroupBox = new QGroupBox("Terminal point");
	m_terminalPointGroupBox->setToolTip(tr("Controls for setting the terminal point for a connector. The terminal point is where a wire will attach to the connector. You can also drag the crosshair of the current connector"));
    QVBoxLayout * anchorGroupLayout = new QVBoxLayout;

    QFrame * posRadioFrame = new QFrame;
    QHBoxLayout * posRadioLayout = new QHBoxLayout;

    QList<QString> positionNames;
    positionNames << "Center"  << "W" << "N" << "S" << "E";
    QList<QString> trPositionNames;
    trPositionNames << tr("Center") << tr("W") << tr("N") << tr("S") << tr("E");
    QList<QString> trLongNames;
    trLongNames << tr("center") << tr("west") << tr("north") << tr("south") << tr("east");
    for (int i = 0; i < positionNames.count(); i++) {
        QPushButton * button = new QPushButton(trPositionNames.at(i));
        button->setProperty("how", positionNames.at(i));
		button->setToolTip(tr("Sets the connector's terminal point to %1.").arg(trLongNames.at(i)));
        connect(button, SIGNAL(clicked()), this, SLOT(buttonChangeTerminalPoint()));
        posRadioLayout->addWidget(button);
        m_buttons.append(button);
    }

    posRadioLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));

    posRadioFrame->setLayout(posRadioLayout);
    anchorGroupLayout->addWidget(posRadioFrame);

    QFrame * posNumberFrame = new QFrame;
    QHBoxLayout * posNumberLayout = new QHBoxLayout;

    label = new QLabel("x");
    posNumberLayout->addWidget(label);

    m_terminalPointX = new PEDoubleSpinBox;
    m_terminalPointX->setDecimals(4);
	m_terminalPointX->setToolTip(tr("Modifies the x-coordinate of the terminal point"));
    posNumberLayout->addWidget(m_terminalPointX);
    connect(m_terminalPointX, SIGNAL(getSpinAmount(double &)), this, SLOT(getSpinAmountSlot(double &)), Qt::DirectConnection);
    connect(m_terminalPointX, SIGNAL(valueChanged(double)), this, SLOT(terminalPointEntry()));
    connect(m_terminalPointX, SIGNAL(valueChanged(const QString &)), this, SLOT(terminalPointEntry()));

    posNumberLayout->addSpacing(PEUtils::Spacing);

    label = new QLabel("y");
    posNumberLayout->addWidget(label);

    m_terminalPointY = new PEDoubleSpinBox;
    m_terminalPointY->setDecimals(4);
	m_terminalPointY->setToolTip(tr("Modifies the y-coordinate of the terminal point"));
    posNumberLayout->addWidget(m_terminalPointY);
    connect(m_terminalPointY, SIGNAL(getSpinAmount(double &)), this, SLOT(getSpinAmountSlot(double &)), Qt::DirectConnection);
    connect(m_terminalPointY, SIGNAL(valueChanged(double)), this, SLOT(terminalPointEntry()));
    connect(m_terminalPointY, SIGNAL(valueChanged(const QString &)), this, SLOT(terminalPointEntry()));

    posNumberLayout->addSpacing(PEUtils::Spacing);

    m_units = new QLabel();
    posNumberLayout->addWidget(m_units);
	posNumberLayout->addSpacing(PEUtils::Spacing);

	m_terminalPointDragState = new QLabel(tr("Dragging disabled"));
    posNumberLayout->addWidget(m_terminalPointDragState);

    posNumberLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));

    posNumberFrame->setLayout(posNumberLayout);
    anchorGroupLayout->addWidget(posNumberFrame);

    m_terminalPointGroupBox->setLayout(anchorGroupLayout);
    m_connectorInfoLayout->addWidget(m_terminalPointGroupBox);

	m_connectorInfoLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_connectorInfoGroupBox->setLayout(m_connectorInfoLayout);

	splitter->addWidget(m_connectorInfoGroupBox);

	//this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	//this->setWidget(splitter);

    this->setLayout(mainLayout);

    m_connectorListWidget->resize(m_connectorListWidget->width(), 0);

    enableConnectorChanges(false, false);
}

PEToolView::~PEToolView() 
{
}

void PEToolView::highlightElement(PEGraphicsItem * pegi) {
    m_pegi = pegi;
    if (pegi == NULL) {
        enableConnectorChanges(false, false);
        return;
    }

    enableConnectorChanges(pegi->showingMarquee(), true);
}

void PEToolView::enableConnectorChanges(bool enableTerminalPoint, bool enablePick)
{
	m_terminalPointGroupBox->setEnabled(enablePick);
	if (m_connectorInfoWidget) {
		m_connectorInfoWidget->setEnabled(enablePick);
	}
	m_pickModeButton->setEnabled(enablePick);

	if (enableTerminalPoint) {
		m_terminalPointDragState->setText(tr("<font color='black'>Dragging enabled</font>"));
		m_terminalPointDragState->setEnabled(true);
	}
	else {
		m_terminalPointDragState->setText(tr("<font color='gray'>Dragging disabled</font>"));
		m_terminalPointDragState->setEnabled(false);
	}
}

void PEToolView::initConnectors(QList<QDomElement> & connectorList) {
    m_connectorListWidget->blockSignals(true);

    m_connectorListWidget->clear();  // deletes QListWidgetItems
    m_connectorList = connectorList;

    int ix = 0;
    foreach (QDomElement connector, connectorList) {
		QListWidgetItem *item = new QListWidgetItem;
		item->setData(Qt::DisplayRole, connector.attribute("name"));
		item->setData(Qt::UserRole, ix++);
		m_connectorListWidget->addItem(item);
    }

    if (m_connectorListWidget->count() > 0) {
        m_connectorListWidget->setCurrentRow(0);
        switchConnector(m_connectorListWidget->currentItem(), NULL);
    }

    m_connectorListWidget->blockSignals(false);

}

void PEToolView::switchConnector(QListWidgetItem * current, QListWidgetItem * previous) {
    Q_UNUSED(previous);

    if (m_connectorInfoWidget) {
        delete m_connectorInfoWidget;
        m_connectorInfoWidget = NULL;
    }

    if (current == NULL) return;

    int index = current->data(Qt::UserRole).toInt();
    QDomElement element = m_connectorList.at(index);

    int pos = 99999;
    for (int ix = 0; ix < m_connectorInfoLayout->count(); ix++) {
        QLayoutItem * item = m_connectorInfoLayout->itemAt(ix);
        if (item->widget() == m_modeFrame) {
            pos = ix + 1;
            break;
        }
    }

    m_connectorInfoWidget = PEUtils::makeConnectorForm(element, index, this, false);
    m_connectorInfoLayout->insertWidget(pos, m_connectorInfoWidget);
    m_connectorInfoGroupBox->setTitle(tr("Connector %1").arg(element.attribute("name")));
    m_units->setText(QString("(%1)").arg(PEUtils::Units));

    emit switchedConnector(element);
}

bool PEToolView::busMode() {
    return m_busModeBox->isChecked();
}

void PEToolView::busModeChangedSlot(bool state)
{
	if (state) enableConnectorChanges(false, false);
	else enableConnectorChanges(m_pegi != NULL && m_pegi->showingMarquee(), m_pegi != NULL);

    emit busModeChanged(state);
}

void PEToolView::nameEntry() {
	changeConnector();
}

void PEToolView::typeEntry() {
	changeConnector();
}

void PEToolView::descriptionEntry() {
	changeConnector();
}

void PEToolView::changeConnector() {
	QListWidgetItem * item = m_connectorListWidget->currentItem();
    if (item == NULL) return;

    int index = item->data(Qt::UserRole).toInt();

    ConnectorMetadata cmd;
    if (!PEUtils::fillInMetadata(index, this, cmd)) return;

    emit connectorMetadataChanged(&cmd);
}

void PEToolView::setCurrentConnector(const QDomElement & newConnector) {
	for (int ix = 0; ix < m_connectorListWidget->count(); ix++) {
		int index = m_connectorListWidget->item(ix)->data(Qt::UserRole).toInt();
		QDomElement connector = m_connectorList.at(index);
		if (connector.attribute("id") == newConnector.attribute("id")) {
			m_connectorListWidget->setCurrentRow(index);
			return;
		}
	}
}

QDomElement PEToolView::currentConnector() {
    QListWidgetItem * item = m_connectorListWidget->currentItem();
    if (item == NULL) return QDomElement();

    int index = item->data(Qt::UserRole).toInt();
    return m_connectorList.at(index);
}

void PEToolView::setTerminalPointCoords(QPointF p) {
    m_terminalPointX->blockSignals(true);
    m_terminalPointY->blockSignals(true);
    m_terminalPointX->setValue(PEUtils::convertUnits(p.x()));
    m_terminalPointY->setValue(PEUtils::convertUnits(p.y()));
    m_terminalPointX->blockSignals(false);
    m_terminalPointY->blockSignals(false);
}

void PEToolView::setTerminalPointLimits(QSizeF sz) {
    m_terminalPointX->setRange(0, sz.width());
    m_terminalPointY->setRange(0, sz.height());
}

void PEToolView::buttonChangeTerminalPoint() {
    QString how = sender()->property("how").toString();
    emit terminalPointChanged(how);
}

void PEToolView::terminalPointEntry()
{
    if (sender() == m_terminalPointX) {
        emit terminalPointChanged("x", PEUtils::unconvertUnits(m_terminalPointX->value()));
    }
    else if (sender() == m_terminalPointY) {
       emit terminalPointChanged("y", PEUtils::unconvertUnits(m_terminalPointY->value()));
    }
}

void PEToolView::getSpinAmountSlot(double & d) {
    emit getSpinAmount(d);
}


void PEToolView::removeConnector() {
    QListWidgetItem * item = m_connectorListWidget->currentItem();
    if (item == NULL) return;

    int index = item->data(Qt::UserRole).toInt();
    QDomElement element = m_connectorList.at(index);
    emit removedConnector(element);

}

void PEToolView::setChildrenVisible(bool vis)
{
	foreach (QWidget * widget, findChildren<QWidget *>()) {
		widget->setVisible(vis);
	}
}

void PEToolView::pickModeChangedSlot() {
	emit pickModeChanged(true);
}