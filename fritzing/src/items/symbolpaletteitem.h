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


#ifndef SYMBOLPALETTEITEM_H
#define SYMBOLPALETTEITEM_H

#include "paletteitem.h"

/*
#include <QTime>

class FocusBugLineEdit : public QLineEdit {
    Q_OBJECT

public:
	FocusBugLineEdit(QWidget * parent = NULL);
	~FocusBugLineEdit();

signals:
	void safeEditingFinished();

protected slots:
	void editingFinishedSlot();
	
protected:
	QTime m_lastEditingFinishedEmit; 					

};
*/

class SymbolPaletteItem : public PaletteItem 
{
	Q_OBJECT

public:
	SymbolPaletteItem(ModelPart *, ViewLayer::ViewIdentifier, const ViewGeometry & viewGeometry, long id, QMenu * itemMenu, bool doLabel);
	~SymbolPaletteItem();

	ConnectorItem* newConnectorItem(class Connector *connector);
	void busConnectorItems(class Bus * bus, QList<ConnectorItem *> & items);
	double voltage();
	void setProp(const QString & prop, const QString & value);
	void setVoltage(double);
	bool collectExtraInfo(QWidget * parent, const QString & family, const QString & prop, const QString & value, bool swappingEnabled, QString & returnProp, QString & returnValue, QWidget * & returnWidget);
	QString getProperty(const QString & key);
	ConnectorItem * connector0();
	ConnectorItem * connector1();
	QString retrieveSvg(ViewLayer::ViewLayerID, QHash<QString, QString> & svgHash, bool blackOnly, double dpi);
	PluralType isPlural();
	void addedToScene(bool temporary);
	bool hasPartNumberProperty();
	bool isNetLabel();
	bool hasPartLabel();
    QString getLabel();

public:
	static double DefaultVoltage;

public slots:
	void voltageEntry(const QString & text);
	void labelEntry();

protected:
	void removeMeFromBus(double voltage);
	double useVoltage(ConnectorItem * connectorItem);
	QString makeSvg();
	QString makeNetLabelSvg();
	QString replaceTextElement(QString svg);
    ViewLayer::ViewIdentifier useViewIdentifierForPixmap(ViewLayer::ViewIdentifier, bool swappingEnabled);
    void setLabel(const QString &);
    QString getDirection();
    QString retrieveNetLabelSvg(ViewLayer::ViewLayerID, QHash<QString, QString> & svgHash, bool blackOnly, double dpi);

protected:
	double m_voltage;
	QPointer<ConnectorItem> m_connector0;
	QPointer<ConnectorItem> m_connector1;
	bool m_voltageReference;
	bool m_isNetLabel;
};

#endif
