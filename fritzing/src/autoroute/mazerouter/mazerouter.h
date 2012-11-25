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
#include <queue>

#include "../../viewgeometry.h"
#include "../../viewlayer.h"
#include "../../commands.h"
#include "../autorouter.h"

struct Net {
    QList<class ConnectorItem *>* net;
    int pinsWithin;
    int id;
};

struct GridPoint {
    int x, y, z, cost;
    uchar flags;

    bool operator<(const GridPoint&) const;
    GridPoint(QPoint, int);
    GridPoint();
};

struct Nearest {
    int i, j;
    double distance;
    ConnectorItem * ic;
    ConnectorItem * jc;
    QPoint gridTarget;

    void swap() {
        int temp = i;
        i = j;
        j = temp;
        ConnectorItem * tempc = ic;
        ic = jc;
        jc = tempc;
    }
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

struct Grid {
    quint32 * data;
    int x;
    int y;
    int z;

    Grid(int x, int y, int layers);

    quint32 at(int x, int y, int z);
    void setAt(int x, int y, int z, quint32 value);
    QList<QPoint> init(int x, int y, int z, int width, int height, const QImage &, quint32 value, bool collectPoints);
};

////////////////////////////////////

class MazeRouter : public Autorouter
{
	Q_OBJECT

public:
	MazeRouter(class PCBSketchWidget *, ItemBase * board, bool adjustIf);
	~MazeRouter(void);

	void start();

signals:
	void setCycleMessage(const QString &);
	void setCycleCount(int);

protected:
    void setUpWidths(double width);
	void updateProgress(int num, int denom);
	void computeMD5(NetOrdering * ordering);
	bool reorder(QList<NetOrdering *> & orderings, NetOrdering *  currentOrdering, NetOrdering * & bestOrdering);
    int findPinsWithin(QList<ConnectorItem *> * net);
    bool makeBoard(QImage & image, double keepout, const QSizeF gridSize);
    bool makeMasters(QString &);
	bool routeNets(NetOrdering *, QVector<int> & netCounters, struct RoutingStatus &, bool makeJumper, NetOrdering * bestOrdering, QImage & boardImage, const QSizeF gridSize);
    void findNearestPair(QList< QList<ConnectorItem *> > & subnets, Nearest &);
    void findNearestPair(QList< QList<ConnectorItem *> > & subnets, int i, QList<ConnectorItem *> & inet, Nearest &);
    QList<QPoint> renderSource(QDomDocument * masterDoc, ViewLayer::ViewLayerSpec, Grid * grid, QList<QDomElement> & netElements, QList<ConnectorItem *> & subnet, quint32 value, bool clearElements, const QRectF & r, bool collectPoints);
    void route(Grid * grid, std::priority_queue<GridPoint> & pq, Nearest & nearest);
    bool expand(GridPoint & gridPoint, Grid * grid, std::priority_queue<GridPoint> & pq, Nearest & nearest);
    GridPoint expandOne(GridPoint gridPoint, Grid * grid, int dx, int dy, int dz, bool crossLayer);
    bool viaWillFit(GridPoint gridPoint, Grid * grid);
    void updateDisplay(int iz);
    void updateDisplay(Grid *, int iz);
    void updateDisplay(GridPoint);

protected:
	LayerList m_viewLayerIDs;
    QHash<ViewLayer::ViewLayerSpec, QDomDocument *> m_masterDocs;
    double m_keepoutMils;
    int m_gridViaSize;
    double m_standardWireWidth;
    QImage * m_displayImage[2];
    QGraphicsPixmapItem * m_displayItem[2];
};

#endif
