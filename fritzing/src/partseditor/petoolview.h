/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2010 Fachhochschule Potsdam - http://fh-potsdam.de

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

#ifndef PETOOLVIEW_H
#define PETOOLVIEW_H

#include <QFrame>
#include <QRadioButton>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QTreeWidget>
#include <QDomDocument>
#include <QDoubleSpinBox>

#include "../viewlayer.h"

class PEDoubleSpinBox : public QDoubleSpinBox
{
Q_OBJECT
public:
    PEDoubleSpinBox(QWidget * parent = 0);

    void stepBy (int steps);

signals:
    void getSpinAmount(double &);
};

class PEToolView : public QWidget
{
Q_OBJECT
public:
	PEToolView(QWidget * parent = NULL);
	~PEToolView();

    void highlightElement(class PEGraphicsItem *);
    void initConnectors(QList<QDomElement> * connectorList);
	bool busMode();
    int currentConnectorIndex();
	void setCurrentConnector(const QDomElement &);
    void setTerminalPointCoords(QPointF);
    void setTerminalPointLimits(QSizeF);
	void setChildrenVisible(bool vis);
    void enableConnectorChanges(bool enableTerminalPoint, bool enablePick);
	void showAssignedConnectors(const QDomDocument * svgDoc, ViewLayer::ViewIdentifier);

signals:
    void switchedConnector(int);
    void removedConnector(const QDomElement &);
    void pickModeChanged(bool);
    void busModeChanged(bool);
    void terminalPointChanged(const QString & how);
    void terminalPointChanged(const QString & coord, double value);
    void getSpinAmount(double &);
    void connectorMetadataChanged(struct ConnectorMetadata *);

protected slots:
    void switchConnector(QTreeWidgetItem * current, QTreeWidgetItem * previous);
    void pickModeChangedSlot();
    void busModeChangedSlot(bool);
    void descriptionEntry();
    void typeEntry();
    void nameEntry();
    void buttonChangeTerminalPoint();
    void terminalPointEntry();
    void getSpinAmountSlot(double &);
    void removeConnector();

protected:
	void changeConnector();
	void hideConnectorListStuff();

protected:
    QTreeWidget * m_connectorListWidget;
    QList<QPushButton *> m_buttons;
    class PEGraphicsItem * m_pegi;
    QList<QDomElement> * m_connectorList;
    QGroupBox * m_connectorInfoGroupBox;
    QBoxLayout * m_connectorInfoLayout;
    QWidget * m_connectorInfoWidget;
	QCheckBox * m_busModeBox;
    QDoubleSpinBox * m_terminalPointX;
    QDoubleSpinBox * m_terminalPointY;
    QLabel * m_units;
	QGroupBox * m_terminalPointGroupBox;
	QLabel * m_terminalPointDragState;
};

#endif
