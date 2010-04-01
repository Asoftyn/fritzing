/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2009 Fachhochschule Potsdam - http://fh-potsdam.de

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

class SymbolPaletteItem : public PaletteItem 
{
	Q_OBJECT

public:
	SymbolPaletteItem(ModelPart *, ViewIdentifierClass::ViewIdentifier, const ViewGeometry & viewGeometry, long id, QMenu * itemMenu, bool doLabel = true);
	~SymbolPaletteItem();

	ConnectorItem* newConnectorItem(class Connector *connector);
	void busConnectorItems(class Bus * bus, QList<ConnectorItem *> & items);
	qreal voltage();
	void setProp(const QString & prop, const QString & value);
	void setVoltage(qreal);
	bool collectExtraInfoHtml(const QString & family, const QString & prop, const QString & value, bool swappingEnabled, QString & returnProp, QString & returnValue);
	QString getProperty(const QString & key);
	ConnectorItem * connector0();
	ConnectorItem * connector1();
	QString retrieveSvg(ViewLayer::ViewLayerID, QHash<QString, class SvgFileSplitter *> & svgHash, bool blackOnly, qreal dpi);
	QObject * createPlugin(QWidget * parent, const QString &classid, const QUrl &url, const QStringList &paramNames, const QStringList &paramValues);
	bool isPlural();

public:
	static qreal DefaultVoltage;

public slots:
	void voltageEntry(const QString & text);

protected:
	void removeMeFromBus(qreal voltage);
	qreal useVoltage(ConnectorItem * connectorItem);
	QString makeSvg();
	QVariant itemChange(GraphicsItemChange change, const QVariant &value);
	QString replaceTextElement(QString svg);

protected:
	qreal m_voltage;
	QPointer<ConnectorItem> m_connector0;
	QPointer<ConnectorItem> m_connector1;
	bool m_voltageReference;
	class FSvgRenderer * m_renderer;

};

#endif
