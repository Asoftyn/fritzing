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


struct GridPoint {
    int x, y, z;
    double cost;
    uchar flags;

    bool operator<(const GridPoint&) const;
    GridPoint(QPoint, int);
    GridPoint();
};

struct Net {
    QList<class ConnectorItem *>* net;
    QList< QList<ConnectorItem *> > subnets;
    int pinsWithin;
    int id;
};

struct NetList {
    QList<Net *> nets;
};

struct Trace {
    int netIndex;
    int subnetIndexI;
    int subnetIndexJ;
    QList<GridPoint> gridPoints;
};


struct NetOrdering {
    QList<int> order;
};

struct Score {
    NetOrdering ordering;
    QMultiHash<int, Trace> traces;
    QHash<int, int> unroutedCount;
    QHash<int, int> viaCount;
	int totalUnroutedCount;
	int totalViaCount;
    int reorderNet;

	Score();
    void setOrdering(const NetOrdering &);
};

struct Nearest {
    int i, j;
    double distance;
    ConnectorItem * ic;
    ConnectorItem * jc;
    QPoint gridTarget;
    bool unrouted;

    void swap() {
        int temp = i;
        i = j;
        j = temp;
        ConnectorItem * tempc = ic;
        ic = jc;
        jc = tempc;
    }
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
    int findPinsWithin(QList<ConnectorItem *> * net);
    bool makeBoard(QImage & image, double keepout, const QSizeF gridSize);
    bool makeMasters(QString &);
	bool routeNets(NetList &, bool makeJumper, Score & currentScore, Score & bestScore, QImage & boardImage, const QSizeF gridSize, QList<NetOrdering> & allOrderings);
    bool routeOne(Score & currentScore, Score & bestScore, int netIndex, Grid *, std::priority_queue<GridPoint> &, Nearest &, QList<NetOrdering> & allOrderings);
    void findNearestPair(QList< QList<ConnectorItem *> > & subnets, Nearest &);
    void findNearestPair(QList< QList<ConnectorItem *> > & subnets, int i, QList<ConnectorItem *> & inet, Nearest &);
    QList<QPoint> renderSource(QDomDocument * masterDoc, int z, Grid * grid, QList<QDomElement> & netElements, QList<ConnectorItem *> & subnet, quint32 value, bool clearElements, const QRectF & r, bool collectPoints);
    QList<GridPoint> route(Grid * grid, std::priority_queue<GridPoint> &, Nearest &);
    bool expand(GridPoint &, Grid * grid, std::priority_queue<GridPoint> &, Nearest &);
    GridPoint expandOne(GridPoint &, Grid * grid, int dx, int dy, int dz, bool crossLayer);
    bool viaWillFit(GridPoint &, Grid * grid);
    QList<GridPoint> traceBack(GridPoint &, Grid * grid);
    GridPoint traceOne(GridPoint &, Grid * grid, int val);
    GridPoint traceBackOne(GridPoint &, Grid * grid, int dx, int dy, int dz, quint32 val);
    void updateDisplay(int iz);
    void updateDisplay(Grid *, int iz);
    void updateDisplay(GridPoint &);
    void clearExpansion(Grid * grid);
    void prepSourceAndTarget(QDomDocument * masterdoc, Grid * grid, QList< QList<ConnectorItem *> > & subnets, int z, std::priority_queue<GridPoint> &, QList<QDomElement> & netElements, QList<QDomElement> & notNetElements, Nearest & nearest, QRectF &); 
    bool moveBack(Score & currentScore, int index, QList<NetOrdering> & allOrderings);

protected:
	LayerList m_viewLayerIDs;
    QHash<ViewLayer::ViewLayerSpec, QDomDocument *> m_masterDocs;
    double m_keepoutMils;
    double m_keepoutGrid;
    int m_gridViaSize;
    double m_standardWireWidth;
    QImage * m_displayImage[2];
    QGraphicsPixmapItem * m_displayItem[2];
};

#endif
