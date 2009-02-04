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

$Revision: 2248 $:
$Author: merunga $:
$Date: 2009-01-22 19:47:17 +0100 (Thu, 22 Jan 2009) $

********************************************************************/


#ifndef PARTSEDITORCONNECTORSPALETTEITEM_H_
#define PARTSEDITORCONNECTORSPALETTEITEM_H_

#include "partseditorpaletteitem.h"

class PartsEditorConnectorsView;

class PartsEditorConnectorsPaletteItem : public PartsEditorPaletteItem {
	Q_OBJECT
	public:
		PartsEditorConnectorsPaletteItem(PartsEditorConnectorsView *owner, ModelPart *modelPart, ItemBase::ViewIdentifier viewIdentifier, StringPair *path, QString layer);
		PartsEditorConnectorsPaletteItem(PartsEditorConnectorsView *owner, ModelPart *modelPart, ItemBase::ViewIdentifier viewIdentifier);

	public slots:
		void highlightConnectors(const QString &connId);

	protected:
		void highlightConnsAux(PaletteItemBase* item, const QString &connId);
		ConnectorItem* newConnectorItem(Connector *connector);
		LayerKinPaletteItem * newLayerKinPaletteItem(
			PaletteItemBase * chief, ModelPart * modelPart, ItemBase::ViewIdentifier viewIdentifier,
			const ViewGeometry & viewGeometry, long id,ViewLayer::ViewLayerID viewLayerID, QMenu* itemMenu, const LayerHash & viewLayers
		);
		bool showingTerminalPoints();

		bool m_showingTerminalPoints;
};

#endif /* PARTSEDITORCONNECTORSPALETTEITEM_H_ */
