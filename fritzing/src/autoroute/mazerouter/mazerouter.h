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

$Revision: 6557 $:
$Author: irascibl@gmail.com $:
$Date: 2012-10-12 15:09:05 +0200 (Fr, 12 Okt 2012) $

********************************************************************/

#ifndef MAZEROUTER_H
#define MAZEROUTER_H

#include <QAction>
#include <QHash>
#include <QVector>
#include <QList>
#include <QSet>
#include <QPointF>
#include <QGraphicsItem>
#include <QLine>
#include <QProgressDialog>
#include <QUndoCommand>
#include <QPointer>

#include <limits>

#include "../../viewgeometry.h"
#include "../../viewlayer.h"
#include "../../commands.h"
#include "../autorouter.h"


struct Net {
    QList<class ConnectorItem *>* net;
    int pinsWithin;
    int id;
};

struct NetOrdering {
    QList<Net *> nets;
	int unroutedCount;
	int jumperCount;
	int viaCount;
	int totalViaCount;
	QByteArray md5sum;
	QByteArray saved;
	QMultiHash<TraceWire *, long> splitDNA;

	NetOrdering() {
		unroutedCount = jumperCount = viaCount = totalViaCount = 0;
	}

	double score();
};


////////////////////////////////////

class MazeRouter : public Autorouter
{
	Q_OBJECT

public:
	MazeRouter(class PCBSketchWidget *, ItemBase * board, bool adjustIf);
	~MazeRouter(void);

	void start();

public:

signals:
	void setCycleMessage(const QString &);
	void setCycleCount(int);

protected:
    void setUpWidths(double width);
	void updateProgress(int num, int denom);
	void computeMD5(NetOrdering * ordering);
	bool reorder(QList<NetOrdering *> & orderings, NetOrdering *  currentOrdering, NetOrdering * & bestOrdering);
    int findPinsWithin(QList<ConnectorItem *> * net);
    bool makeBoard(QImage & image, double keepout);
    bool makeMasters(QString &);

protected:
	LayerList m_viewLayerIDs;
    QHash<ViewLayer::ViewLayerSpec, QDomDocument *> m_masterDocs;
};

#endif
