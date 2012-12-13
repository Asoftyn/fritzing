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

typedef double (*CostFunction)(const QPoint & p1, const QPoint & p2);
typedef quint64 GridValue;

struct GridPoint {
    int x, y, z;
    GridValue baseCost;
    double qCost;
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
    int order;
    uchar flags;
    QList<GridPoint> gridPoints;
    
    Trace() {
        flags = 0;
    }
};

struct NetOrdering {
    QList<int> order;
};

struct Score {
    NetOrdering ordering;
    QMultiHash<int, Trace> traces;
    QHash<int, int> routedCount;
    QHash<int, int> viaCount;
	int totalRoutedCount;
	int totalViaCount;
    int reorderNet;
    bool anyUnrouted;

	Score();
    void setOrdering(const NetOrdering &);
};

struct Nearest {
    int i, j;
    double distance;
    ConnectorItem * ic;
    ConnectorItem * jc;

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
    GridValue * data;
    int x;
    int y;
    int z;

    Grid(int x, int y, int layers);

    GridValue at(int x, int y, int z);
    void setAt(int x, int y, int z, GridValue value);
    QList<QPoint> init(int x, int y, int z, int width, int height, const QImage &, GridValue value, bool collectPoints);
    QList<QPoint> init4(int x, int y, int z, int width, int height, const QImage *, GridValue value, bool collectPoints);
    void clear();
    void free();
};


struct NetElements {
    QList<QDomElement> net;
    QList<QDomElement> alsoNet;
    QList<QDomElement> notNet;
};

struct RouteThing {
    QRectF r;
    QList<ViewLayer::ViewLayerSpec> layerSpecs;
    int ikeepout;
    Nearest nearest;
    std::priority_queue<GridPoint> pq;
    bool makeJumper;
    double jumperDistance;
    GridPoint jumperLocation;
    QPoint gridTarget;
    bool unrouted;
    NetElements netElements[2];
    QSet <int> avoids;

    bool isAvoid(GridPoint &, Grid *);
    bool isAvoid(int x, int y, Grid *);
};

////////////////////////////////////

class MazeRouter : public Autorouter
{
	Q_OBJECT

public:
	MazeRouter(class PCBSketchWidget *, QGraphicsItem * board, bool adjustIf);
	~MazeRouter(void);

	void start();

protected:
    void setUpWidths(double width);
	void updateProgress(int num, int denom);
    int findPinsWithin(QList<ConnectorItem *> * net);
    bool makeBoard(QImage *, double keepout, const QSizeF gridSize);
    bool makeMasters(QString &);
	bool routeNets(NetList &, bool makeJumper, Score & currentScore, const QSizeF gridSize, QList<NetOrdering> & allOrderings);
    bool routeOne(bool makeJumper, Score & currentScore, int netIndex, RouteThing &, QList<NetOrdering> & allOrderings);
    void findNearestPair(QList< QList<ConnectorItem *> > & subnets, Nearest &);
    void findNearestPair(QList< QList<ConnectorItem *> > & subnets, int i, QList<ConnectorItem *> & inet, Nearest &);
    QList<QPoint> renderSource(QDomDocument * masterDoc, int z, Grid * grid, QList<QDomElement> & netElements, QList<ConnectorItem *> & subnet, GridValue value, bool clearElements, const QRectF & r, bool collectPoints);
    QList<GridPoint> route(RouteThing &, int & viaCount);
    void expand(GridPoint &, RouteThing &);
    void expandOne(GridPoint &, RouteThing &, int dx, int dy, int dz, bool crossLayer);
    bool viaWillFit(GridPoint &, Grid * grid);
    QList<GridPoint> traceBack(GridPoint, Grid *, int & viaCount);
    GridPoint traceBackOne(GridPoint &, Grid *, int dx, int dy, int dz);
    void updateDisplay(int iz);
    void updateDisplay(Grid *, int iz);
    void updateDisplay(GridPoint &);
    void clearExpansion(Grid * grid);
    void prepSourceAndTarget(QDomDocument * masterdoc, RouteThing &, QList< QList<ConnectorItem *> > & subnets, int z); 
    bool moveBack(Score & currentScore, int index, QList<NetOrdering> & allOrderings);
    void drawTrace(Trace &);
    void initTraceDisplay();
    void traceObstacles(QList<Trace> & traces, int netIndex, Grid * grid, int ikeepout);
    void traceAvoids(QList<Trace> & traces, int netIndex, RouteThing & routeThing);
    bool routeNext(bool makeJumper, RouteThing &, QList< QList<ConnectorItem *> > & subnets, Score & currentScore, int netIndex, QList<NetOrdering> & allOrderings);
    void cleanUpNets(NetList &);
    void createTraces(NetList & netList, Score & bestScore, QUndoCommand * parentCommand);
    void removeColinear(QList<GridPoint> & gridPoints);
    void removeSteps(QList<GridPoint> & gridPoints);
    ConnectorItem * findAnchor(GridPoint gp, QPointF topLeft, Net * net, QList<TraceWire *> & newTraces, QList<Via *> & newVias, QPointF & p, bool & onTrace);
    ConnectorItem * findAnchor(GridPoint gp, const QRectF &, Net * net, QList<TraceWire *> & newTraces, QList<Via *> & newVias, QPointF & p, bool & onTrace);
    void addConnectionToUndo(ConnectorItem * from, ConnectorItem * to, QUndoCommand * parentCommand);
    void addViaToUndo(Via *, QUndoCommand * parentCommand);
    void addJumperToUndo(JumperItem *, QUndoCommand * parentCommand);
    void removeStep(int ix, QList<GridPoint> & gridPoints);
    void clearExpansionForJumper(Grid * grid, GridValue sourceOrTarget, std::priority_queue<GridPoint> & pq);
    void routeJumper(int netIndex, RouteThing &, Score & currentScore);
    void jumperWillFit(GridPoint & gridPoint, RouteThing &);
    void insertTrace(Trace & newTrace, int netIndex, Score & currentScore, int viaCount);

protected:
	LayerList m_viewLayerIDs;
    QHash<ViewLayer::ViewLayerSpec, QDomDocument *> m_masterDocs;
    double m_keepoutMils;
    double m_keepoutGrid;
    int m_halfGridViaSize;
    int m_halfGridJumperSize;
    double m_gridPixels;
    double m_standardWireWidth;
    QImage * m_displayImage[2];
    QImage * m_boardImage;
    QImage * m_spareImage;
    QGraphicsPixmapItem * m_displayItem[2];
    bool m_temporaryBoard;
    CostFunction m_costFunction;
    uint m_traceColors[2];
    Grid * m_grid;
};

#endif
