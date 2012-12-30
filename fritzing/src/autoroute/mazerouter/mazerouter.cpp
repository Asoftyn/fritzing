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


// with lots of suggestions from http://cc.ee.ntu.edu.tw/~ywchang/Courses/PD/unit6.pdf
// and some help from http://workbench.lafayette.edu/~nestorj/cadapplets/MazeApplet/src/
// more (unused) suggestions at http://embedded.eecs.berkeley.edu/Alumni/pinhong/ee244/4-routing.PDF

// TODO:
//
//      schematic netlabels are the equivalent of jumpers. Use successive numbers for the labels.
//          select all netlabels on trace menu
//
//      via/jumper placement must ensure minimum distance from source
//          jumper placement must be away from vias
//          and previous traces (GridAvoid)
//
//      when multiple traces connect to one via, traces tend to connect to each other instead of the via
//          see shift register example
//
//      cleanup feedback stops dead with phoenix_no_traces after 8% is reached 
//
//      bad jumper placement in atmega256?
//
//      keepout dialog
//
//      test if source touches target
//

// LATER:
//
//      figure out what is taking so long once the router is creating traces
//          addItem is expensive, so is changeConnection
//
//      net reordering/rip-up-and-reroute
//          is there a better way than move back by one?
//
//      raster back to vector
//          curve-fitting? use a bezier?  http://kobus.ca/seminars/ugrad/NM5_curve_s02.pdf
//          lines for all 1 x N patterns?
//          how guarantee that new line/curve doesn't overlap
//              handle this at traceback time? 
//                  set some flags for neighbors being clear and keep the flagged grid points as part of the trace?
//
//      dynamic cost function based on distance to any target point?
//
//      allow multiple routes to reach GridTarget--eliminate all queued GridPoints with greater cost
//      
//      what happens with expanded schematic frame?
//

#include "mazerouter.h"
#include "../../sketch/pcbsketchwidget.h"
#include "../../debugdialog.h"
#include "../../items/virtualwire.h"
#include "../../items/tracewire.h"
#include "../../items/jumperitem.h"
#include "../../items/symbolpaletteitem.h"
#include "../../items/via.h"
#include "../../items/resizableboard.h"
#include "../../utils/graphicsutils.h"
#include "../../utils/graphutils.h"
#include "../../utils/textutils.h"
#include "../../utils/folderutils.h"
#include "../../connectors/connectoritem.h"
#include "../../items/moduleidnames.h"
#include "../../processeventblocker.h"
#include "../../svg/groundplanegenerator.h"
#include "../../svg/svgfilesplitter.h"
#include "../../fsvgrenderer.h"
#include "../drc.h"
#include "../../connectors/svgidlayer.h"

#include <QApplication>
#include <QMessageBox> 
#include <QSettings>

#include <qmath.h>
#include <limits>

//////////////////////////////////////

static int routeNumber = 0;

static QString CancelledMessage;

static const int DefaultMaxCycles = 10;

static const GridValue GridBoardObstacle = std::numeric_limits<GridValue>::max();
static const GridValue GridPartObstacle = GridBoardObstacle - 1;
static const GridValue GridSource = GridBoardObstacle - 2;
static const GridValue GridTarget = GridBoardObstacle - 3;
static const GridValue GridAvoid = GridBoardObstacle - 4;
static const GridValue GridTempObstacle = GridBoardObstacle - 5;
static const GridValue GridSourceFlag = (GridBoardObstacle / 2) + 1;

static const uint Layer1Cost = 100;
static const uint CrossLayerCost = 100;
static const uint ViaCost = 2000;
static const uint AvoidCost = 7;

static const uchar GridPointDone = 1;
static const uchar GridPointStepYPlus = 2;
static const uchar GridPointStepYMinus = 4;
static const uchar GridPointStepXPlus = 8;
static const uchar GridPointStepXMinus = 16;
static const uchar GridPointStepStart = 32;
static const uchar GridPointJumperLeft = 64;
static const uchar GridPointJumperRight = 128;
//static const uchar GridPointNorth = 2;
//static const uchar GridPointEast = 4;
//static const uchar GridPointSouth = 8;
//static const uchar GridPointWest = 16;

static const uchar JumperStart = 1;
static const uchar JumperEnd = 2;
static const uchar JumperLeft = 4;
static const uchar JumperRight = 8;

static const double MinTraceManhattanLength = 0.1;  // pixels

////////////////////////////////////////////////////////////////////

QPointF getStepPoint(QPointF p, uchar flags, double gridPixels) {
    if ((flags & GridPointStepXPlus) != 0) {
        p.setX(p.x() + (gridPixels / 2));
        return p;
    }
    if ((flags & GridPointStepXMinus) != 0) {
        p.setX(p.x() - (gridPixels / 2));
        return p;
    }
    if ((flags & GridPointStepYPlus) != 0) {
        p.setY(p.y() + (gridPixels / 2));
        return p;
    }
    p.setY(p.y() - (gridPixels / 2));
    return p;
}

uchar getStepFlag(GridPoint & gp1, GridPoint & gp2) {
    if (gp1.y == gp2.y) {
        double avg = (gp1.x + gp2.x) / 2.0;
        return (avg > gp1.x) ? GridPointStepXPlus : GridPointStepXMinus;
    }

    double avg = (gp1.y + gp2.y) / 2.0;
    return (avg > gp1.y) ? GridPointStepYPlus : GridPointStepYMinus;

}

QPointF getPixelCenter(GridPoint & gp, QPointF & topLeft, double gridPixels) {
    return QPointF((gp.x * gridPixels) + topLeft.x() + (gridPixels / 2), (gp.y * gridPixels) + topLeft.y() + (gridPixels / 2));
}

uint getColor(GridValue val) {
    if (val == GridBoardObstacle) return 0xff000000;
    else if (val == GridPartObstacle) return 0xff404040;
    else if (val == 0) return 0;
    else if (val == GridSource) return 0xff00ff00;
    else if (val == GridTarget) return 0xffff0000;
    else if (val == GridAvoid) return 0xff808080;
    else if (val == GridTempObstacle) return 0xff00ffff;
    else if (val & GridSourceFlag) return 0xff60d060;
    else return 0xffff6060;
}

void drawBorder(QImage * image, int border) {
    int halfBorder = border / 2;
    QPainter painter;
	painter.begin(image);
	painter.setRenderHint(QPainter::Antialiasing, false);
    QPen pen = painter.pen();
    pen.setWidth(border);
    pen.setColor(0xff000000);
    painter.setPen(pen);
    painter.drawLine(0, halfBorder, image->width() - 1, halfBorder);
    painter.drawLine(0, image->height() - halfBorder, image->width() - 1, image->height() - halfBorder);
    painter.drawLine(halfBorder, 0, halfBorder, image->height() - 1);
    painter.drawLine(image->width() - halfBorder, 0, image->width() - halfBorder, image->height() - 1);
	painter.end();
}

void fastCopy(QImage * from, QImage * to) {
    uchar * fromBits = from->scanLine(0);
    uchar * toBits = to->scanLine(0);
    long bytes = from->bytesPerLine() * from->height();
    memcpy(toBits, fromBits, bytes);
}

bool atLeast(const QPointF & p1, const QPointF & p2) {
    return (qAbs(p1.x() - p2.x()) >= MinTraceManhattanLength) || (qAbs(p1.y() - p2.y()) >= MinTraceManhattanLength);
}

void printOrder(const QString & msg, QList<int> & order) {
    QString string(msg);
    foreach (int i, order) {
        string += " " + QString::number(i);
    }
    DebugDialog::debug(string);
}

QString getPartID(const QDomElement & element) {
    QString partID = element.attribute("partID");
    if (!partID.isEmpty()) return partID;

    QDomNode parent = element.parentNode();
    if (parent.isNull()) return "";

    return getPartID(parent.toElement());
}

bool idsMatch(const QDomElement & element, QMultiHash<QString, QString> & partIDs) {
    QString partID = getPartID(element);
    QStringList IDs = partIDs.values(partID);
    if (IDs.count() == 0) return false;

    QDomElement tempElement = element;
    while (tempElement.attribute("partID").isEmpty()) {
        QString id = tempElement.attribute("id");
        if (!id.isEmpty() && IDs.contains(id)) return true;

        tempElement = tempElement.parentNode().toElement();
    }

    return false;
}

bool byPinsWithin(Net * n1, Net * n2)
{
	if (n1->pinsWithin < n2->pinsWithin) return true;
    if (n1->pinsWithin > n2->pinsWithin) return false;

    return n1->net->count() <= n2->net->count();
}

bool byOrder(Trace & t1, Trace & t2) {
    return (t1.order < t2.order);
}

/* 
inline double initialCost(QPoint p1, QPoint p2) {
    //return qAbs(p1.x() - p2.x()) + qAbs(p1.y() - p2.y());
    return qSqrt(GraphicsUtils::distanceSqd(p1, p2));
}
*/

inline double distanceCost(const QPoint & p1, const QPoint & p2) {
    return GraphicsUtils::distanceSqd(p1, p2);
}

inline double manhattanCost(const QPoint & p1, const QPoint & p2) {
    return qMax(qAbs(p1.x() - p2.x()), qAbs(p1.y() - p2.y()));
}

inline int gridPointInt(Grid * grid, GridPoint & gp) {
    return (gp.z * grid->x * grid->y) + (gp.y * grid->x) + gp.x;
}

bool jumperWillFit(GridPoint & gridPoint, const Grid * grid, int halfSize) {
    for (int z = 0; z < grid->z; z++) {
        for (int y = -halfSize; y <= halfSize; y++) {
            for (int x = -halfSize; x <= halfSize; x++) {
                GridValue val = grid->at(gridPoint.x + x, gridPoint.y + y, z);
                if (val == GridPartObstacle || val == GridBoardObstacle || val == GridSource || val == GridTarget || val == GridAvoid|| val == GridTempObstacle) return false;
            }
        }
    }

    return true;
}

bool schematicJumperWillFitAux(GridPoint & gridPoint, const Grid * grid, int halfSize, int xl, int xr) {
    for (int y = -halfSize; y <= halfSize; y++) {
        for (int x =xl; x <= xr; x++) {
            GridValue val = grid->at(gridPoint.x + x, gridPoint.y + y, 0);
            if (val == GridPartObstacle || val == GridBoardObstacle || val == GridSource || val == GridTarget || val == GridAvoid|| val == GridTempObstacle) {
                return false;
            }

            if (val < gridPoint.baseCost) {
                return false;
            }
        }
    }

    return true;
}

bool schematicJumperWillFit(GridPoint & gridPoint, const Grid * grid, int halfSize) 
{
    if (schematicJumperWillFitAux(gridPoint, grid, halfSize, -(halfSize * 2), 0)) {
        gridPoint.flags |= GridPointJumperLeft;
        return true;
    }
    if (schematicJumperWillFitAux(gridPoint, grid, halfSize, 0, halfSize * 2)) {
        gridPoint.flags |= GridPointJumperRight;
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////

bool GridPoint::operator<(const GridPoint& other) const {
    // make sure lower cost is first
    return qCost > other.qCost;
}

GridPoint::GridPoint(QPoint p, int zed) {
    z = zed;
    x = p.x();
    y = p.y();
    flags = 0;
}

GridPoint::GridPoint() 
{
    flags = 0;
}

////////////////////////////////////////////////////////////////////

Grid::Grid(int sx, int sy, int sz) {
    x = sx;
    y = sy;
    z = sz;

    data = (GridValue *) malloc(x * y * z * sizeof(GridValue));   // calloc initializes grid to 0
}

GridValue Grid::at(int sx, int sy, int sz) const {
    return *(data + (sz * y * x) + (sy * x) + sx);
}

void Grid::setAt(int sx, int sy, int sz, GridValue value) {
   *(data + (sz * y * x) + (sy * x) + sx) = value;
}

QList<QPoint> Grid::init(int sx, int sy, int sz, int width, int height, const QImage & image, GridValue value, bool collectPoints) {
    QList<QPoint> points;
    const uchar * bits1 = image.constScanLine(0);
    int bytesPerLine = image.bytesPerLine();
	for (int iy = sy; iy < sy + height; iy++) {
        int offset = iy * bytesPerLine;
		for (int ix = sx; ix < sx + width; ix++) {
            int byteOffset = (ix >> 3) + offset;
            uchar mask = DRC::BitTable[ix & 7];

            //if (routeNumber > 40) {
            //    DebugDialog::debug(QString("image %1 %2, %3").arg(image.width()).arg(image.height()).arg(image.isNull()));
			//    DebugDialog::debug(QString("init point %1 %2 %3").arg(ix).arg(iy).arg(sz));
			//    DebugDialog::debug(QString("init grid %1 %2 %3, %4").arg(x).arg(y).arg(z).arg((long) data, 0, 16));
            //}
            if ((*(bits1 + byteOffset)) & mask) continue;

            //if (routeNumber > 40) DebugDialog::debug("after mask");

            setAt(ix, iy, sz, value);
            //if (routeNumber > 40)
            //    DebugDialog::debug("set");
            if (collectPoints) {
                points.append(QPoint(ix, iy));
            }
		}
	}

    return points;
}


QList<QPoint> Grid::init4(int sx, int sy, int sz, int width, int height, const QImage * image, GridValue value, bool collectPoints) {
    // pixels are 4 x 4 bits
    QList<QPoint> points;
    const uchar * bits1 = image->constScanLine(0);
    int bytesPerLine = image->bytesPerLine();
	for (int iy = sy; iy < sy + height; iy++) {
        int offset = iy * bytesPerLine * 4;
		for (int ix = sx; ix < sx + width; ix++) {
            int byteOffset = (ix >> 1) + offset;
            uchar mask = ix & 1 ? 0x0f : 0xf0;

            if ((*(bits1 + byteOffset) & mask) != mask) ;
            else if ((*(bits1 + byteOffset + bytesPerLine) & mask) != mask) ;
            else if ((*(bits1 + byteOffset + bytesPerLine + bytesPerLine) & mask) != mask) ;
            else if ((*(bits1 + byteOffset + bytesPerLine + bytesPerLine + bytesPerLine) & mask) != mask) ;
            else continue;  // "pixel" is all white

            setAt(ix, iy, sz, value);
            if (collectPoints) {
                points.append(QPoint(ix, iy));
            }
		}
	}

    return points;
}

void Grid::copy(int fromIndex, int toIndex) {
    memcpy(((uchar *) data) + toIndex * x * y * sizeof(GridValue), ((uchar *) data) + fromIndex * x * y * sizeof(GridValue), x * y * sizeof(GridValue));
}

void Grid::clear() {
    memset(data, 0, x * y * z * sizeof(GridValue));
}

void Grid::free() {
    if (data) {
        ::free(data);
        data = NULL;
    }
}

////////////////////////////////////////////////////////////////////

Score::Score() {
	totalRoutedCount = totalViaCount = 0;
    anyUnrouted = false;
    reorderNet = -1;
}

void Score::setOrdering(const NetOrdering & _ordering) {
    reorderNet = -1;
    if (ordering.order.count() > 0) {
        bool remove = false;
        for (int i = 0; i < ordering.order.count(); i++) {
            if (!remove && (ordering.order.at(i) == _ordering.order.at(i))) continue;

            remove = true;
            int netIndex = ordering.order.at(i);
            traces.remove(netIndex);
            int c = routedCount.value(netIndex);
            routedCount.remove(netIndex);
            totalRoutedCount -= c;
            c = viaCount.value(netIndex);
            viaCount.remove(netIndex);
            totalViaCount -= c;
        }
    }
    ordering = _ordering;
    printOrder("new  ", ordering.order);
}

////////////////////////////////////////////////////////////////////

MazeRouter::MazeRouter(PCBSketchWidget * sketchWidget, QGraphicsItem * board, bool adjustIf) : Autorouter(sketchWidget)
{
    m_netLabelIndex = -1;
    m_grid = NULL;
    m_displayItem[0] = m_displayItem[1] = NULL;
    m_boardImage = m_spareImage = m_displayImage[0] = m_displayImage[1] = NULL;

    CancelledMessage = tr("Autorouter was cancelled.");

	QSettings settings;
	m_maxCycles = settings.value("cmrouter/maxcycles", DefaultMaxCycles).toInt();
		
	m_bothSidesNow = sketchWidget->routeBothSides();
    m_pcbType = sketchWidget->autorouteTypePCB();
	m_board = board;
    m_temporaryBoard = false;

	if (m_board) {
		m_maxRect = m_board->sceneBoundingRect();
	}
	else {
        QRectF itemsBoundingRect;
	    foreach(QGraphicsItem *item,  m_sketchWidget->scene()->items()) {
		    if (!item->isVisible()) continue;

            itemsBoundingRect |= item->sceneBoundingRect();
	    }
		m_maxRect = itemsBoundingRect;  // itemsBoundingRect is not reliable.  m_sketchWidget->scene()->itemsBoundingRect();
		if (adjustIf) {
            m_maxRect.adjust(-m_maxRect.width() / 2, -m_maxRect.height() / 2, m_maxRect.width() / 2, m_maxRect.height() / 2);
		}
        m_board = new QGraphicsRectItem(m_maxRect);
        m_board->setVisible(false);
        m_sketchWidget->scene()->addItem(m_board);
        m_temporaryBoard = true;
	}

    m_standardWireWidth = m_sketchWidget->getAutorouterTraceWidth();

    /*
    // for debugging leave the last result hanging around
    QList<QGraphicsPixmapItem *> pixmapItems;
    foreach (QGraphicsItem * item, m_sketchWidget->scene()->items()) {
        QGraphicsPixmapItem * pixmapItem = dynamic_cast<QGraphicsPixmapItem *>(item);
        if (pixmapItem) pixmapItems << pixmapItem;
    }
    foreach (QGraphicsPixmapItem * pixmapItem, pixmapItems) {
        delete pixmapItem;
    }
    */

	ViewGeometry vg;
	vg.setWireFlags(m_sketchWidget->getTraceFlag());
	ViewLayer::ViewLayerID bottom = sketchWidget->getWireViewLayerID(vg, ViewLayer::Bottom);
	m_viewLayerIDs << bottom;
	if  (m_bothSidesNow) {
		ViewLayer::ViewLayerID top = sketchWidget->getWireViewLayerID(vg, ViewLayer::Top);
		m_viewLayerIDs.append(top);
	}
}

MazeRouter::~MazeRouter()
{
    foreach (QDomDocument * doc, m_masterDocs) {
        delete doc;
    }
    if (m_displayItem[0]) {
        delete m_displayItem[0];
    }
    if (m_displayItem[1]) {
        delete m_displayItem[1];
    }
    if (m_displayImage[0]) {
        delete m_displayImage[0];
    }
    if (m_displayImage[1]) {
        delete m_displayImage[1];
    }
    if (m_temporaryBoard && m_board != NULL) {
        delete m_board;
    }
    if (m_grid) {
        m_grid->free();
        delete m_grid;
    }
    if (m_boardImage) {
        delete m_boardImage;
    }
    if (m_spareImage) {
        delete m_spareImage;
    }
}

void MazeRouter::start()
{	
    if (m_pcbType) {
	    if (m_board == NULL) {
		    QMessageBox::warning(NULL, QObject::tr("Fritzing"), QObject::tr("Cannot autoroute: no board (or multiple boards) found"));
		    return;
	    }
        m_jumperWillFitFunction = jumperWillFit;
        m_costFunction = distanceCost;
        m_traceColors[0] = 0xa0F28A00;
        m_traceColors[1] = 0xa0FFCB33;
    }
    else {
        m_jumperWillFitFunction = schematicJumperWillFit;
        m_costFunction = manhattanCost;
        m_traceColors[0] = m_traceColors[1] = 0xa0303030;
    }

	m_keepoutPixels = m_sketchWidget->getKeepout();			// 15 mils space (in pixels)
    m_gridPixels = qMax(m_standardWireWidth, m_keepoutPixels);
    m_keepoutMils = m_keepoutPixels * GraphicsUtils::StandardFritzingDPI / GraphicsUtils::SVGDPI;
    m_keepoutGrid = m_keepoutPixels / m_gridPixels;
    m_keepoutGridInt = qCeil(m_keepoutGrid);

    double ringThickness, holeSize;
	m_sketchWidget->getViaSize(ringThickness, holeSize);
	int gridViaSize = qCeil((ringThickness + ringThickness + holeSize + m_keepoutPixels + m_keepoutPixels) / m_gridPixels);
    m_halfGridViaSize = gridViaSize / 2;

    QSizeF jumperSize = m_sketchWidget->jumperItemSize();
    int gridJumperSize = qCeil((qMax(jumperSize.width(), jumperSize.height()) + m_keepoutPixels  + m_keepoutPixels) / m_gridPixels);
    m_halfGridJumperSize = gridJumperSize / 2;

	emit setMaximumProgress(m_maxCycles);
	emit setProgressMessage("");
	emit setCycleMessage("round 1 of:");
	emit setCycleCount(m_maxCycles);

	m_sketchWidget->ensureTraceLayersVisible();

	QHash<ConnectorItem *, int> indexer;
	m_sketchWidget->collectAllNets(indexer, m_allPartConnectorItems, false, m_bothSidesNow);

    removeOffBoardAnd(m_pcbType, true, m_bothSidesNow);

	if (m_allPartConnectorItems.count() == 0) {
        QString message = m_pcbType ?  QObject::tr("No connections (on the PCB) to route.") : QObject::tr("No connections to route.");
		QMessageBox::information(NULL, QObject::tr("Fritzing"), message);
		Autorouter::cleanUpNets();
		return;
	}

	QUndoCommand * parentCommand = new QUndoCommand("Autoroute");
	new CleanUpWiresCommand(m_sketchWidget, CleanUpWiresCommand::UndoOnly, parentCommand);
	initUndo(parentCommand);

    NetList netList;
    int totalToRoute = 0;
	for (int i = 0; i < m_allPartConnectorItems.count(); i++) {
        Net * net = new Net;
        net->net = m_allPartConnectorItems[i];

        foreach (ConnectorItem * connectorItem, *(net->net)) {
            connectorItem->debugInfo("all parts");
        }

        QList<ConnectorItem *> todo;
        todo.append(*(net->net));
        while (todo.count() > 0) {
            ConnectorItem * first = todo.takeFirst();
            QList<ConnectorItem *> equi;
            equi.append(first);
	        ConnectorItem::collectEqualPotential(equi, m_bothSidesNow, (ViewGeometry::RatsnestFlag | ViewGeometry::NormalFlag | ViewGeometry::PCBTraceFlag | ViewGeometry::SchematicTraceFlag) ^ m_sketchWidget->getTraceFlag());
            foreach (ConnectorItem * equ, equi) {
                todo.removeOne(equ);
                equ->debugInfo("equi");
            }
            net->subnets.append(equi);
        }

        if (net->subnets.count() < 2) {
            // net is already routed
            continue;
        }

        net->pinsWithin = findPinsWithin(net->net);
        netList.nets << net;
        totalToRoute += net->net->count() - 1;
	}

    qSort(netList.nets.begin(), netList.nets.end(), byPinsWithin);
    NetOrdering initialOrdering;
    int ix = 0;
    foreach (Net * net, netList.nets) {
        // id is the same as the order in netList
        initialOrdering.order << ix;
        net->id = ix++;
    }

	if (m_bothSidesNow) {
		emit wantBothVisible();
	}

	ProcessEventBlocker::processEvents(); // to keep the app  from freezing
	if (m_cancelled || m_stopTracing) {
		restoreOriginalState(parentCommand);
		cleanUpNets(netList);
		return;
	}

    QSizeF gridSize(m_maxRect.width() / m_gridPixels, m_maxRect.height() / m_gridPixels);   
    QSize boardImageSize(qCeil(gridSize.width()), qCeil(gridSize.height())); 
    m_grid = new Grid(boardImageSize.width(), boardImageSize.height(), m_bothSidesNow ? 2 : 1);
    if (m_grid->data == NULL) {
        QMessageBox::information(NULL, QObject::tr("Fritzing"), "Out of memory--unable to proceed");
		restoreOriginalState(parentCommand);
		cleanUpNets(netList);
		return;
    }

    m_boardImage = new QImage(boardImageSize.width() * 4, boardImageSize.height() * 4, QImage::Format_Mono);
    m_spareImage = new QImage(boardImageSize.width() * 4, boardImageSize.height() * 4, QImage::Format_Mono);
    if (m_temporaryBoard) {
        m_boardImage->fill(0xffffffff);
    }
    else {
        m_boardImage->fill(0);
        QRectF r4(QPointF(0, 0), gridSize * 4);
        makeBoard(m_boardImage, m_keepoutGrid, r4);   
    }
    drawBorder(m_boardImage, 4);

	ProcessEventBlocker::processEvents(); // to keep the app  from freezing
	if (m_cancelled || m_stopTracing) {
		restoreOriginalState(parentCommand);
		cleanUpNets(netList);
		return;
	}

    m_displayImage[0] = new QImage(boardImageSize, QImage::Format_ARGB32);
    m_displayImage[0]->fill(0);
    m_displayImage[1] = new QImage(boardImageSize, QImage::Format_ARGB32);
    m_displayImage[1]->fill(0);

    QString message;
    bool gotMasters = makeMasters(message);
	if (m_cancelled || m_stopTracing || !gotMasters) {
		restoreOriginalState(parentCommand);
		cleanUpNets(netList);
		return;
	}

	QList<NetOrdering> allOrderings;
    allOrderings << initialOrdering;
    Score bestScore;
    Score currentScore;
    int run = 0;
	for (; run < m_maxCycles && run < allOrderings.count(); run++) {
		QString msg= tr("best so far: %1 of %2 routed").arg(bestScore.totalRoutedCount).arg(totalToRoute);
		if (m_pcbType) {
			msg +=  tr(" with %n vias", "", bestScore.totalViaCount);
		}
		emit setProgressMessage(msg);
		emit setCycleMessage(tr("round %1 of:").arg(run + 1));
        emit setProgressValue(run);
		ProcessEventBlocker::processEvents();
        currentScore.setOrdering(allOrderings.at(run));
        currentScore.anyUnrouted = false;
		routeNets(netList, false, currentScore, gridSize, allOrderings);
		if (bestScore.ordering.order.count() == 0) {
            bestScore = currentScore;
        }
        else {
            if (currentScore.totalRoutedCount > bestScore.totalRoutedCount) {
                bestScore = currentScore;
            }
            else if (currentScore.totalRoutedCount == bestScore.totalRoutedCount && currentScore.totalViaCount < bestScore.totalViaCount) {
                bestScore = currentScore;
            }
        }
		if (m_cancelled || bestScore.anyUnrouted == false || m_stopTracing) break;
	}

    emit disableButtons();

	//DebugDialog::debug("done running");


	if (m_cancelled) {
		doCancel(parentCommand);
		return;
	}

    if (m_stopTracing) {
        emit setProgressMessage(tr("Routing stopped! Now cleaning up..."));
        if (m_useBest) {
            routeNets(netList, true, bestScore, gridSize, allOrderings);
        }
    }
    else if (!bestScore.anyUnrouted) {
        emit setProgressMessage(tr("Routing complete! Now cleaning up..."));
        emit setProgressValue(m_maxCycles);
    }
    else {
		emit setCycleMessage(tr("round %1 of:").arg(run));
        QString msg;
        if (run < m_maxCycles) msg = tr("Routing cannot complete; stopping at round %1.").arg(run);
        else msg = tr("Routing reached maximum round %1.").arg(m_maxCycles);
        msg += " ";
        msg += tr("Now cleaning up...");
        emit setProgressMessage(msg);
        printOrder("best ", bestScore.ordering.order);
        routeNets(netList, true, bestScore, gridSize, allOrderings);
        emit setProgressValue(m_maxCycles);
    }    
	ProcessEventBlocker::processEvents();

    createTraces(netList, bestScore, parentCommand);

	cleanUpNets(netList);

	new CleanUpWiresCommand(m_sketchWidget, CleanUpWiresCommand::RedoOnly, parentCommand);

    m_sketchWidget->blockUI(true);
    int cc = BaseCommand::totalChildCount(parentCommand);
    emit setMaximumProgress(cc);
    m_cleanupCount = 0;
	m_sketchWidget->pushCommand(parentCommand, this);
    m_sketchWidget->blockUI(false);
	m_sketchWidget->repaint();
	DebugDialog::debug("\n\n\nautorouting complete\n\n\n");

	if (m_offBoardConnectors.count() > 0) {
		QSet<ItemBase *> parts;
		foreach (ConnectorItem * connectorItem, m_offBoardConnectors) {
			parts.insert(connectorItem->attachedTo()->layerKinChief());
		}
        QMessageBox::information(NULL, QObject::tr("Fritzing"), tr("Note: the autorouter did not route %n parts, because they are not located entirely on the board.", "", parts.count()));
	}
}

int MazeRouter::findPinsWithin(QList<ConnectorItem *> * net) {
    int count = 0;
    QRectF r;
    foreach (ConnectorItem * connectorItem, *net) {
        r |= connectorItem->sceneBoundingRect();
    }

    foreach (QGraphicsItem * item, m_sketchWidget->scene()->items(r)) {
        ConnectorItem * connectorItem = dynamic_cast<ConnectorItem *>(item);
        if (connectorItem == NULL) continue;

        if (net->contains(connectorItem)) continue;

        count++;
    }

    return count;
}

bool MazeRouter::makeBoard(QImage * boardImage, double keepoutGrid, const QRectF & renderRect) {
	LayerList viewLayerIDs;
	viewLayerIDs << ViewLayer::Board;
	QRectF boardImageRect;
	bool empty;
	QString boardSvg = m_sketchWidget->renderToSVG(GraphicsUtils::SVGDPI, viewLayerIDs, true, boardImageRect, m_board, GraphicsUtils::StandardFritzingDPI, false, false, empty);
	if (boardSvg.isEmpty()) {
		return false;
	}

    QByteArray boardByteArray;
    QString tempColor("#ffffff");
    QStringList exceptions;
	exceptions << "none" << "";
    if (!SvgFileSplitter::changeColors(boardSvg, tempColor, exceptions, boardByteArray)) {
		return false;
	}

	QSvgRenderer renderer(boardByteArray);
	QPainter painter;
	painter.begin(boardImage);
	painter.setRenderHint(QPainter::Antialiasing, false);
	renderer.render(&painter, renderRect);
	painter.end();

    // board should be white, borders should be black

#ifndef QT_NO_DEBUG
	boardImage->save(FolderUtils::getUserDataStorePath("") + "/mazeMakeBoard.png");
#endif

    // extend it given that the board image is * 4
    DRC::extendBorder(keepoutGrid * 4, boardImage);

#ifndef QT_NO_DEBUG
	boardImage->save(FolderUtils::getUserDataStorePath("") + "/mazeMakeBoard2.png");
#endif

    return true;
}

bool MazeRouter::makeMasters(QString & message) {
    QList<ViewLayer::ViewLayerSpec> layerSpecs;
    layerSpecs << ViewLayer::Bottom;
    if (m_bothSidesNow) layerSpecs << ViewLayer::Top;

    foreach (ViewLayer::ViewLayerSpec viewLayerSpec, layerSpecs) {  
	    LayerList viewLayerIDs = m_sketchWidget->routingLayers(viewLayerSpec);
        QRectF masterImageRect;
        bool empty;
	    QString master = m_sketchWidget->renderToSVG(GraphicsUtils::SVGDPI, viewLayerIDs, true, masterImageRect, m_board, GraphicsUtils::StandardFritzingDPI, false, false, empty);
        if (master.isEmpty()) {
            continue;
	    }

	    QDomDocument * masterDoc = new QDomDocument();
        m_masterDocs.insert(viewLayerSpec, masterDoc);

	    QString errorStr;
	    int errorLine;
	    int errorColumn;
	    if (!masterDoc->setContent(master, &errorStr, &errorLine, &errorColumn)) {
            message = tr("Unexpected SVG rendering failure--contact fritzing.org");
		    return false;
	    }

	    ProcessEventBlocker::processEvents();
        if (m_cancelled) {
            message = CancelledMessage;
            return false;
        }

        QDomElement root = masterDoc->documentElement();
        SvgFileSplitter::forceStrokeWidth(root, 2 * m_keepoutMils, "#000000", true, true);
        //QString forDebugging = masterDoc->toByteArray();
        //DebugDialog::debug("master " + forDebugging);
    }

    return true;
}

bool MazeRouter::routeNets(NetList & netList, bool makeJumper, Score & currentScore, const QSizeF gridSize, QList<NetOrdering> & allOrderings)
{
    RouteThing routeThing;
    routeThing.netElements[0] = NetElements();
    routeThing.netElements[1] = NetElements();
    routeThing.r = QRectF(QPointF(0, 0), gridSize);
    routeThing.r4 = QRectF(QPointF(0, 0), gridSize * 4);
    routeThing.layerSpecs << ViewLayer::Bottom;
    if (m_bothSidesNow) routeThing.layerSpecs << ViewLayer::Top;

    bool result = true;

    initTraceDisplay();
    bool previousTraces = false;
    foreach (int netIndex, currentScore.ordering.order) {
        if (m_cancelled || m_stopTracing) {
            return false;
        }

        Net * net = netList.nets.at(netIndex);
        DebugDialog::debug(QString("routing net %1, subnets %2, traces %3, routed %4")
            .arg(netIndex)
            .arg(net->subnets.count())
            .arg(currentScore.traces.values(netIndex).count())
            .arg(currentScore.routedCount.value(netIndex))        
            );

        if (currentScore.routedCount.value(netIndex) == net->subnets.count() - 1) {
            // this net was fully routed in a previous run
            foreach (Trace trace, currentScore.traces.values(netIndex)) {
                displayTrace(trace);
            }
            previousTraces = true;
            continue;
        }

        if (previousTraces) {
            updateDisplay(m_grid, 0);
            if (m_bothSidesNow) updateDisplay(m_grid, 1);
        }

        if (currentScore.routedCount.value(netIndex) > 0) {
            // should only be here when makeJumpers = true
            // remove the set of routed traces for this net--the net was not completely routed
            // we didn't get all the way through before
            currentScore.totalRoutedCount -= currentScore.routedCount.value(netIndex);
            currentScore.routedCount.insert(netIndex, 0);
            currentScore.totalViaCount -= currentScore.viaCount.value(netIndex);
            currentScore.viaCount.insert(netIndex, 0);
            currentScore.traces.remove(netIndex);    
        }

        //foreach (ConnectorItem * connectorItem, *(net->net)) {
        //    if (connectorItem->attachedTo()->layerKinChief()->id() == 12407630) {
        //        connectorItem->debugInfo("what");
        //        break;
        //    }
        //}

        QList< QList<ConnectorItem *> > subnets;
        foreach (QList<ConnectorItem *> subnet, net->subnets) {
            QList<ConnectorItem *> copy(subnet);
            subnets.append(copy);
        }

        //DebugDialog::debug("find nearest pair");

        findNearestPair(subnets, routeThing.nearest);
        QPointF ip = routeThing.nearest.ic->sceneAdjustedTerminalPoint(NULL) - m_maxRect.topLeft();
        routeThing.gridSourcePoint = QPoint(ip.x() / m_gridPixels, ip.y() / m_gridPixels);
        QPointF jp = routeThing.nearest.jc->sceneAdjustedTerminalPoint(NULL) - m_maxRect.topLeft();
        routeThing.gridTargetPoint = QPoint(jp.x() / m_gridPixels, jp.y() / m_gridPixels);

        m_grid->clear();
        m_grid->init4(0, 0, 0, m_grid->x, m_grid->y, m_boardImage, GridBoardObstacle, false);
        if (m_bothSidesNow) {
            m_grid->copy(0, 1);
        }

        QList<Trace> traces = currentScore.traces.values();
        if (m_pcbType) {
            traceObstacles(traces, netIndex, m_grid, m_keepoutGridInt);
        }
        else {
            traceAvoids(traces, netIndex, routeThing);
        }

        foreach (ViewLayer::ViewLayerSpec viewLayerSpec, routeThing.layerSpecs) {  
            int z = viewLayerSpec == ViewLayer::Bottom ? 0 : 1;

            QDomDocument * masterDoc = m_masterDocs.value(viewLayerSpec);

            //QString temp = masterDoc->toString();

            Markers markers;
            markers.outID = DRC::NotNet;
            markers.inTerminalID = DRC::Net;
            markers.inSvgID = DRC::Net;
            markers.inSvgAndID = DRC::AlsoNet;
            markers.inNoID = DRC::Net;
            DRC::splitNetPrep(masterDoc, *(net->net), markers, routeThing.netElements[z].net, routeThing.netElements[z].alsoNet, routeThing.netElements[z].notNet, true);
            foreach (QDomElement element, routeThing.netElements[z].net) {
                element.setTagName("g");
            }
            foreach (QDomElement element, routeThing.netElements[z].alsoNet) {
                element.setTagName("g");
            }

            static int oinc = 0;
            //DebugDialog::debug("obstacles from board");
            m_spareImage->fill(0xffffffff);
            DRC::renderOne(masterDoc, m_spareImage, routeThing.r4);
            #ifndef QT_NO_DEBUG
                m_spareImage->save(FolderUtils::getUserDataStorePath("") + QString("/obstacles%1.png").arg(oinc++));
            #endif
            m_grid->init4(0, 0, z, m_grid->x, m_grid->y, m_spareImage, GridPartObstacle, false);
            //DebugDialog::debug("obstacles from board done");

            prepSourceAndTarget(masterDoc, routeThing, subnets, z);
        }

        //updateDisplay(m_grid, 0);
        //if (m_bothSidesNow) updateDisplay(m_grid, 1);

        routeThing.unrouted = false;
        if (!routeOne(makeJumper, currentScore, netIndex, routeThing, allOrderings)) {
            result = false;
        }

        while (result && subnets.count() > 2) {
            /*
            DebugDialog::debug(QString("\nnearest %1 %2").arg(nearest.i).arg(nearest.j));
            nearest.ic->debugInfo("\ti");
            nearest.jc->debugInfo("\tj");
            int ix = 0;
            foreach (QList<ConnectorItem *> subnet, subnets) {
                foreach(ConnectorItem * connectorItem, subnet) {
                    connectorItem->debugInfo(QString::number(ix));
                }
                ix++;
            }
            */
            
            result = routeNext(makeJumper, routeThing, subnets, currentScore, netIndex, allOrderings);
        }

        routeThing.netElements[0].net.clear();
        routeThing.netElements[0].notNet.clear();
        routeThing.netElements[0].alsoNet.clear();
        routeThing.netElements[1].net.clear();
        routeThing.netElements[1].notNet.clear();
        routeThing.netElements[1].alsoNet.clear();
        routeThing.sourceQ = std::priority_queue<GridPoint>();
        routeThing.targetQ = std::priority_queue<GridPoint>();

        if (result == false) break;
    }

    return result;
}

bool MazeRouter::routeOne(bool makeJumper, Score & currentScore, int netIndex, RouteThing & routeThing, QList<NetOrdering> & allOrderings) {
    Trace newTrace;
    int viaCount;
    routeThing.bestDistanceToSource = routeThing.bestDistanceToTarget = std::numeric_limits<double>::max();
    newTrace.gridPoints = route(routeThing, viaCount);
    if (m_cancelled || m_stopTracing) {
        return false;
    }

    if (newTrace.gridPoints.count() == 0) {
        if (makeJumper) {
            routeJumper(netIndex, routeThing, currentScore);
        }
        else {
            routeThing.unrouted = true;
            if (currentScore.reorderNet < 0) {
                for (int i = 0; i < currentScore.ordering.order.count(); i++) {
                    if (currentScore.ordering.order.at(i) == netIndex) {
                        if (moveBack(currentScore, i, allOrderings)) {
                            currentScore.reorderNet = netIndex;
                        }
                        break;
                    }
                }
            }

            currentScore.anyUnrouted = true;
            if (currentScore.reorderNet >= 0) {
                // rip up and reroute unless this net is already the first one on the list
                return false;
            }
            // unable to move the 0th net so keep going
        }
    }
    else {
        insertTrace(newTrace, netIndex, currentScore, viaCount, true);
        updateDisplay(0);
        if (m_bothSidesNow) updateDisplay(1);
    }

    return true;
}

bool MazeRouter::routeNext(bool makeJumper, RouteThing & routeThing, QList< QList<ConnectorItem *> > & subnets, Score & currentScore, int netIndex, QList<NetOrdering> & allOrderings) 
{
    bool result = true;

    QList<ConnectorItem *> combined;
    if (routeThing.unrouted) {
        if (routeThing.nearest.i < routeThing.nearest.j) {
            subnets.removeAt(routeThing.nearest.j);
            combined = subnets.takeAt(routeThing.nearest.i);
        }
        else {
            combined = subnets.takeAt(routeThing.nearest.i);
            subnets.removeAt(routeThing.nearest.j);
        }
    }
    else {
        combined.append(subnets.at(routeThing.nearest.i));
        combined.append(subnets.at(routeThing.nearest.j));
        if (routeThing.nearest.i < routeThing.nearest.j) {
            subnets.removeAt(routeThing.nearest.j);
            subnets.removeAt(routeThing.nearest.i);
        }
        else {
            subnets.removeAt(routeThing.nearest.i);
            subnets.removeAt(routeThing.nearest.j);
        }
    }
    subnets.prepend(combined);
    routeThing.nearest.i = 0;
    routeThing.nearest.j = -1;
    routeThing.nearest.distance = std::numeric_limits<double>::max();
    findNearestPair(subnets, 0, combined, routeThing.nearest);
    QPointF ip = routeThing.nearest.ic->sceneAdjustedTerminalPoint(NULL) - m_maxRect.topLeft();
    routeThing.gridSourcePoint = QPoint(ip.x() / m_gridPixels, ip.y() / m_gridPixels);
    QPointF jp = routeThing.nearest.jc->sceneAdjustedTerminalPoint(NULL) - m_maxRect.topLeft();
    routeThing.gridTargetPoint = QPoint(jp.x() / m_gridPixels, jp.y() / m_gridPixels);

    routeThing.sourceQ = std::priority_queue<GridPoint>();
    routeThing.targetQ = std::priority_queue<GridPoint>();

    if (!m_pcbType) {
        QList<Trace> traces = currentScore.traces.values();
        traceAvoids(traces, netIndex, routeThing);
    }

    foreach (ViewLayer::ViewLayerSpec viewLayerSpec, routeThing.layerSpecs) {  
        int z = viewLayerSpec == ViewLayer::Bottom ? 0 : 1;
        QDomDocument * masterDoc = m_masterDocs.value(viewLayerSpec);
        prepSourceAndTarget(masterDoc, routeThing, subnets, z);
    }

    // redraw traces from this net
    foreach (Trace trace, currentScore.traces.values(netIndex)) {
        foreach (GridPoint gridPoint, trace.gridPoints) {
            m_grid->setAt(gridPoint.x, gridPoint.y, gridPoint.z, GridSource);
            gridPoint.qCost = gridPoint.baseCost = /* initialCost(QPoint(gridPoint.x, gridPoint.y), routeThing.gridTarget) + */ 0;
            gridPoint.flags = 0;
            //DebugDialog::debug(QString("pushing trace %1 %2 %3, %4, %5").arg(gridPoint.x).arg(gridPoint.y).arg(gridPoint.z).arg(gridPoint.qCost).arg(routeThing.pq.size()));
            routeThing.sourceQ.push(gridPoint);                  
        }
    }

    //updateDisplay(m_grid, 0);
    //if (m_bothSidesNow) updateDisplay(m_grid, 1);

    routeThing.unrouted = false;
    result = routeOne(makeJumper, currentScore, netIndex, routeThing, allOrderings);

    return result;
}

bool MazeRouter::moveBack(Score & currentScore, int index, QList<NetOrdering> & allOrderings) {
    if (index == 0) {
        return false;  // nowhere to move back to
    }

    QList<int> order(currentScore.ordering.order);
    //printOrder("start", order);
    int netIndex = order.takeAt(index);
    //printOrder("minus", order);
    for (int i = index - 1; i >= 0; i--) {
        bool done = true;
        order.insert(i, netIndex);
        //printOrder("plus ", order);
        foreach (NetOrdering ordering, allOrderings) {
            bool gotOne = true;
            for (int j = 0; j < order.count(); j++) {
                if (order.at(j) != ordering.order.at(j)) {
                    gotOne = false;
                    break;
                }
            }
            if (gotOne) {
                done = false;
                break; 
            }
        }
        if (done == true) {
            NetOrdering newOrdering;
            newOrdering.order = order;
            allOrderings.append(newOrdering);
            //printOrder("done ", newOrdering.order);

            /*
            const static int watch[] = { 0, 1, 2, 3, 6, 12, 14, 4, 5, 7, 8, 9, 10, 11, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64 };
            // {0, 1, 2, 4, 3, 12, 6, 14, 5, 7, 8, 9, 10, 11, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64};
            
            bool matches = true;
            for (int i = 0; i < order.count(); i++) {
                if (order.at(i) != watch[i]) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                DebugDialog::debug("order matches");
            }
            */
            return true;
        }
        order.removeAt(i);
    }

    return false;
}

void MazeRouter::prepSourceAndTarget(QDomDocument * masterDoc, RouteThing & routeThing, QList< QList<ConnectorItem *> > & subnets, int z) 
{
    foreach (QDomElement element, routeThing.netElements[z].notNet) {
        element.setTagName("g");
    }
    foreach (QDomElement element, routeThing.netElements[z].alsoNet) {
        element.setTagName("g");
    }

    foreach (QDomElement element, routeThing.netElements[z].net) {
        SvgFileSplitter::forceStrokeWidth(element, -2 * m_keepoutMils, "#000000", false, false);
    }

    QList<ConnectorItem *> li = subnets.at(routeThing.nearest.i);
    QList<QPoint> sourcePoints = renderSource(masterDoc, z, m_grid, routeThing.netElements[z].net, li, GridSource, true, routeThing.r4);

    foreach (QPoint p, sourcePoints) {
        GridPoint gridPoint(p, z);
        gridPoint.qCost = gridPoint.baseCost = /* initialCost(p, routeThing.gridTarget) + */ 0;
        //DebugDialog::debug(QString("pushing source %1 %2 %3, %4, %5").arg(gridPoint.x).arg(gridPoint.y).arg(gridPoint.z).arg(gridPoint.qCost).arg(routeThing.pq.size()));
        routeThing.sourceQ.push(gridPoint);
    }

    QList<ConnectorItem *> lj = subnets.at(routeThing.nearest.j);
    QList<QPoint> targetPoints = renderSource(masterDoc, z, m_grid, routeThing.netElements[z].net, lj, GridTarget, true, routeThing.r4);
    foreach (QPoint p, targetPoints) {
        GridPoint gridPoint(p, z);
        gridPoint.qCost = gridPoint.baseCost = /* initialCost(p, routeThing.gridTarget) + */ 0;
        //DebugDialog::debug(QString("pushing source %1 %2 %3, %4, %5").arg(gridPoint.x).arg(gridPoint.y).arg(gridPoint.z).arg(gridPoint.qCost).arg(routeThing.pq.size()));
        routeThing.targetQ.push(gridPoint);
    }

    foreach (QDomElement element, routeThing.netElements[z].net) {
        SvgFileSplitter::forceStrokeWidth(element, 2 * m_keepoutMils, "#000000", false, false);
    }

    // restore masterdoc
    foreach (QDomElement element, routeThing.netElements[z].net) {
        element.setTagName(element.attribute("former"));
        element.removeAttribute("net");
    }
    foreach (QDomElement element, routeThing.netElements[z].notNet) {
        element.setTagName(element.attribute("former"));
        element.removeAttribute("net");
    }
    foreach (QDomElement element, routeThing.netElements[z].alsoNet) {
        element.setTagName(element.attribute("former"));
        element.removeAttribute("net");
    }
}

void MazeRouter::findNearestPair(QList< QList<ConnectorItem *> > & subnets, Nearest & nearest) {
    nearest.distance = std::numeric_limits<double>::max();
    nearest.i = nearest.j = -1;
    nearest.ic = nearest.jc = NULL;
    for (int i = 0; i < subnets.count() - 1; i++) {
        QList<ConnectorItem *> inet = subnets.at(i);
        findNearestPair(subnets, i, inet, nearest);
    }
}

void MazeRouter::findNearestPair(QList< QList<ConnectorItem *> > & subnets, int inetix, QList<ConnectorItem *> & inet, Nearest & nearest) {
    for (int j = inetix + 1; j < subnets.count(); j++) {
        QList<ConnectorItem *> jnet = subnets.at(j);
        foreach (ConnectorItem * ic, inet) {
            QPointF ip = ic->sceneAdjustedTerminalPoint(NULL);
            ConnectorItem * icc = ic->getCrossLayerConnectorItem();
            foreach (ConnectorItem * jc, jnet) {
                ConnectorItem * jcc = jc->getCrossLayerConnectorItem();
                if (jc == ic || jcc == ic) continue;

                QPointF jp = jc->sceneAdjustedTerminalPoint(NULL);
                double d = qSqrt(GraphicsUtils::distanceSqd(ip, jp)) / m_gridPixels;
                if (ic->attachedToViewLayerID() != jc->attachedToViewLayerID()) {
                    if (jcc != NULL || icc != NULL) {
                        // may not need a via
                        d += CrossLayerCost;
                    }
                    else {
                        // requires at least one via
                        d += ViaCost;
                    }
                }
                else {
                    if (jcc != NULL && icc != NULL && ic->attachedToViewLayerID() == ViewLayer::Copper1) {
                        // route on the bottom when possible
                        d += Layer1Cost;
                    }
                }
                if (d < nearest.distance) {
                    nearest.distance = d;
                    nearest.i = inetix;
                    nearest.j = j;
                    nearest.ic = ic;
                    nearest.jc = jc;
                }
            }
        }
    }
}

QList<QPoint> MazeRouter::renderSource(QDomDocument * masterDoc, int z, Grid * grid, QList<QDomElement> & netElements, QList<ConnectorItem *> & subnet, GridValue value, bool clearElements, const QRectF & renderRect) {
    if (clearElements) {
        foreach (QDomElement element, netElements) {
            element.setTagName("g");
        }
    }

    m_spareImage->fill(0xffffffff);
    QMultiHash<QString, QString> partIDs;
    QMultiHash<QString, QString> terminalIDs;
    QList<ConnectorItem *> terminalPoints;
    QRectF itemsBoundingRect;
    foreach (ConnectorItem * connectorItem, subnet) {
        ItemBase * itemBase = connectorItem->attachedTo();
        SvgIdLayer * svgIdLayer = connectorItem->connector()->fullPinInfo(itemBase->viewIdentifier(), itemBase->viewLayerID());
        partIDs.insert(QString::number(itemBase->id()), svgIdLayer->m_svgId);
        if (!svgIdLayer->m_terminalId.isEmpty()) {
            terminalIDs.insert(QString::number(itemBase->id()), svgIdLayer->m_terminalId);
            terminalPoints << connectorItem;
        }
        itemsBoundingRect |= connectorItem->sceneBoundingRect();
    }
    foreach (QDomElement element, netElements) {
        if (idsMatch(element, partIDs)) {
            element.setTagName(element.attribute("former"));
        }
        else if (idsMatch(element, terminalIDs)) {
            element.setTagName(element.attribute("former"));
        }
    }

    int x1 = qFloor((itemsBoundingRect.left() - m_maxRect.left()) / m_gridPixels);
    int y1 = qFloor((itemsBoundingRect.top() - m_maxRect.top()) / m_gridPixels);
    int x2 = qCeil((itemsBoundingRect.right() - m_maxRect.left()) / m_gridPixels);
    int y2 = qCeil((itemsBoundingRect.bottom() - m_maxRect.top()) / m_gridPixels);

    DRC::renderOne(masterDoc, m_spareImage, renderRect);
#ifndef QT_NO_DEBUG
    static int rsi = 0;
	m_spareImage->save(FolderUtils::getUserDataStorePath("") + QString("/rendersource%1.png").arg(rsi++));
#endif
    QList<QPoint> points = grid->init4(x1, y1, z, x2 - x1, y2 - y1, m_spareImage, value, true);

    // terminal point hack (mostly for schematic view)
    foreach (ConnectorItem * connectorItem, terminalPoints) {
        QPointF p = connectorItem->sceneAdjustedTerminalPoint(NULL);
        QRectF r = connectorItem->attachedTo()->sceneBoundingRect().adjusted(-m_keepoutPixels, -m_keepoutPixels, m_keepoutPixels, m_keepoutPixels);
        QPointF closest(p.x(), r.top());
        double d = qAbs(p.y() - r.top());
        int dx = 0;
        int dy = -1;
        if (qAbs(p.y() - r.bottom()) < d) {
            d = qAbs(p.y() - r.bottom());
            closest = QPointF(p.x(), r.bottom());
            dy = 1;
            dx = 0;
        }
        if (qAbs(p.x() - r.left()) < d) {
            d = qAbs(p.x() - r.left());
            closest = QPointF(r.left(), p.y());
            dx = -1;
            dy = 0;
        }
        if (qAbs(p.x() - r.right()) < d) {
            d = qAbs(p.x() - r.right());
            closest = QPointF(r.right(), p.y());
            dy = 0;
            dx = 1;
        }

        double y1, y2, x1, x2;
        double yp = (p.y() - m_maxRect.top()) / m_gridPixels;
        double xp = (p.x() - m_maxRect.left()) / m_gridPixels;
        double yc = (closest.y() - m_maxRect.top()) / m_gridPixels;
        double xc = (closest.x() - m_maxRect.left()) / m_gridPixels;
        if (closest.x() == p.x()) {
            x1 = x2 = qRound((p.x() - m_maxRect.left()) / m_gridPixels);
            y1 = (yp < yc) ? qFloor(yp) : qCeil(yp);
            y2 = (yp < yc) ? qCeil(yc) : qFloor(yc);
            if (y2 < y1) {
                double temp = y2;
                y2 = y1;
                y1 = temp;
            }
        }
        else {
            y1 = y2 = qRound((p.y() - m_maxRect.top()) / m_gridPixels);
            x1 = (xp < xc) ? qFloor(xp) : qCeil(xp);
            x2 = (xp < xc) ? qCeil(xc) : qFloor(xc);
            if (x2 < x1) {
                double temp = x2;
                x2 = x1;
                x1 = temp;
            }
        }

        int xo = qRound(xc);
        int yo = qRound(yc);
        // remove obstacles we can draw through
        while (true) {
            xo += dx;
            yo += dy;
            if (grid->at(xo, yo, z) != GridAvoid) break;

            grid->setAt(xo, yo, z, 0);
        }  

        for (int iy = y1; iy <= y2; iy++) {
            for (int ix = x1; ix <= x2; ix++) {
                GridValue val = grid->at(ix, iy, z);
                if (val == GridPartObstacle || val == GridAvoid) {
                    // make an empty path up to the source point
                    grid->setAt(ix, iy, z, 0);
                }
            }
        }
        int xr = qRound(xp);
        int yr = qRound(yp);
        if (grid->at(xr, yr, z) != GridBoardObstacle) {
            grid->setAt(xr, yr, z, value);
            points << QPoint(xr, yr);
        }
        
    }

    return points;
}

QList<GridPoint> MazeRouter::route(RouteThing & routeThing, int & viaCount)
{

    //DebugDialog::debug(QString("start routing %1").arg(routeNumber++));
    viaCount = 0;
    GridPoint done;
    bool result = false;
    while (!routeThing.sourceQ.empty() && !routeThing.targetQ.empty()) {
        GridPoint gp = routeThing.sourceQ.top();
        GridPoint gpt = routeThing.targetQ.top();
        if (gpt.qCost < gp.qCost) {
            gp = gpt;
            routeThing.targetQ.pop();
            routeThing.targetValue = GridSource;
            routeThing.sourceValue = GridTarget;
        }
        else {
            routeThing.sourceQ.pop();
            routeThing.targetValue = GridTarget;
            routeThing.sourceValue = GridSource;
        }

        if (gp.flags & GridPointDone) {
            done = gp;
            result = true;
            break;
        }
        
        expand(gp, routeThing);
        if (m_cancelled || m_stopTracing) {
            break;
        }
    }

    //DebugDialog::debug(QString("routing result %1").arg(result));

    QList<GridPoint> points;
    if (!result) {
        //updateDisplay(m_grid, 0);
        //DebugDialog::debug(QString("done routing no points"));
        return points;
    }
    done.baseCost = std::numeric_limits<GridValue>::max();  // make sure this is the largest value for either traceback
    QList<GridPoint> sourcePoints = traceBack(done, m_grid, viaCount, GridTarget, GridSource);      // trace back to source
    QList<GridPoint> targetPoints = traceBack(done, m_grid, viaCount, GridSource, GridTarget);      // trace back to target
    if (sourcePoints.count() == 0 || targetPoints.count() == 0) {
        DebugDialog::debug("traceback zero points");
        return points;
    }
    else {
        targetPoints.takeFirst();           // redundant point
        foreach (GridPoint gp, targetPoints) points.prepend(gp);
        points.append(sourcePoints);
    }

    clearExpansion(m_grid);  

    //DebugDialog::debug(QString("done routing %1").arg(points.count()));

    return points;
}

QList<GridPoint> MazeRouter::traceBack(GridPoint gridPoint, Grid * grid, int & viaCount, GridValue sourceValue, GridValue targetValue) {
    //DebugDialog::debug(QString("traceback %1 %2 %3").arg(gridPoint.x).arg(gridPoint.y).arg(gridPoint.z));
    QList<GridPoint> points;
    points << gridPoint;
    while (true) {
        if (gridPoint.baseCost == targetValue) {
            // done
            break;
        }

        // can only be one neighbor with lower value
        GridPoint next = traceBackOne(gridPoint, grid, -1, 0, 0, sourceValue, targetValue);
        if (next.baseCost == GridBoardObstacle) {
            next = traceBackOne(gridPoint, grid, 1, 0, 0, sourceValue, targetValue);
            if (next.baseCost == GridBoardObstacle) {
                next = traceBackOne(gridPoint, grid, 0, -1, 0, sourceValue, targetValue);
                if (next.baseCost == GridBoardObstacle) {
                    next = traceBackOne(gridPoint, grid, 0, 1, 0, sourceValue, targetValue);
                    if (next.baseCost == GridBoardObstacle) {
                        next = traceBackOne(gridPoint, grid, 0, 0, -1, sourceValue, targetValue);
                        if (next.baseCost == GridBoardObstacle) {
                            next = traceBackOne(gridPoint, grid, 0, 0, 1, sourceValue, targetValue);
                            if (next.baseCost == GridBoardObstacle) {
                                // traceback failed--is this possible?
                                points.clear();
                                break;      
                            }
                        }
                    }
                }
            }
        }

        //if (grid->at(next.x - 1, next.y, next.z) != GridObstacle) next.flags |= GridPointWest;
        //if (grid->at(next.x + 1, next.y, next.z) != GridObstacle) next.flags |= GridPointEast;
        //if (grid->at(next.x, next.y - 1, next.z) != GridObstacle) next.flags |= GridPointNorth;
        //if (grid->at(next.x, next.y + 1, next.z) != GridObstacle) next.flags |= GridPointSouth;
        points << next;
        if (next.z != gridPoint.z) viaCount++;
        gridPoint = next;
    }

    QString costs("costs ");
    foreach (GridPoint gridPoint, points) {
        costs += QString::number(gridPoint.baseCost) + " ";
    }
    DebugDialog::debug(costs);

    return points;
}

GridPoint MazeRouter::traceBackOne(GridPoint & gridPoint, Grid * grid, int dx, int dy, int dz, GridValue sourceValue, GridValue targetValue) {
    GridPoint next;
    next.baseCost = GridBoardObstacle;

    next.x = gridPoint.x + dx;
    if (next.x < 0 || next.x >= grid->x) {
        return next;
    }

    next.y = gridPoint.y + dy;
    if (next.y < 0 || next.y >= grid->y) {
        return next;
    }

    next.z = gridPoint.z + dz;
    if (next.z < 0 || next.z >= grid->z) {
        return next;
    }

    GridValue nextval = grid->at(next.x, next.y, next.z);
    if (nextval == GridBoardObstacle || nextval == GridPartObstacle || nextval == sourceValue || nextval == 0 || nextval == GridTempObstacle) return next;
    if (nextval == targetValue) {
        // done!
        next.baseCost = targetValue;
        return next;
    }

    if (targetValue == GridSource) {
        if ((nextval & GridSourceFlag) == 0) return next;
        nextval ^= GridSourceFlag;
    }
    else {
        if (nextval & GridSourceFlag) return next;
    }

    if (nextval < gridPoint.baseCost) {
        next.baseCost = nextval;
    }
    return next;
}

void MazeRouter::expand(GridPoint & gridPoint, RouteThing & routeThing)
{
    //static bool debugit = false;
    //if (routeNumber > 41 && routeThing.pq.size() > 8200) debugit = true;

    //if (debugit) {
    //    DebugDialog::debug(QString("expand %1 %2 %3, %4").arg(gridPoint.x).arg(gridPoint.y).arg(gridPoint.z).arg(routeThing.pq.size()));
    //}
    if (gridPoint.x > 0) expandOne(gridPoint, routeThing, -1, 0, 0, false);
    if (gridPoint.x < m_grid->x - 1) expandOne(gridPoint, routeThing, 1, 0, 0, false);
    if (gridPoint.y > 0) expandOne(gridPoint, routeThing, 0, -1, 0, false);
    if (gridPoint.y < m_grid->y - 1) expandOne(gridPoint, routeThing, 0, 1, 0, false);
    if (m_bothSidesNow) {
        if (gridPoint.z > 0) expandOne(gridPoint, routeThing, 0, 0, -1, true);
        if (gridPoint.z < m_grid->z - 1) expandOne(gridPoint, routeThing, 0, 0, 1, true);
    }
    //if (debugit) {
    //    DebugDialog::debug("expand done");
    //}
}

void MazeRouter::expandOne(GridPoint & gridPoint, RouteThing & routeThing, int dx, int dy, int dz, bool crossLayer) {
    GridPoint next;
    next.x = gridPoint.x + dx;
    next.y = gridPoint.y + dy;
    next.z = gridPoint.z + dz;

    bool writeable = false;
    bool avoid = false;
    GridValue nextval = m_grid->at(next.x, next.y, next.z);
    if (nextval == GridPartObstacle || nextval == GridBoardObstacle || nextval == routeThing.sourceValue || nextval == GridTempObstacle) return;
    if (nextval == routeThing.targetValue) {
        DebugDialog::debug("found grid target");
        next.flags |= GridPointDone;
    }
    else if (nextval == 0) {
        writeable = true;
    }
    else if (nextval == GridAvoid) {
        bool contains = true;
        for (int i = 1; i <= 3; i++) {
            if (!routeThing.avoids.contains(((next.y - (i * dy)) * m_grid->x) + next.x - (i * dx))) {
                contains = false;
                break;
            }
        }

        if (contains) {
            // do not allow more than 3 in a row in the same direction?
            return;
        }
        avoid = writeable = true;
        if (dx == 0) {
            if (m_grid->at(next.x - 1, next.y, next.z) == GridAvoid) {
                m_grid->setAt(next.x - 1, next.y, next.z, GridTempObstacle);
            }
            if (m_grid->at(next.x + 1, next.y, next.z) == GridAvoid) {
                m_grid->setAt(next.x + 1, next.y, next.z, GridTempObstacle);
            }
        }
        else {
            if (m_grid->at(next.x, next.y - 1, next.z) == GridAvoid) {
                m_grid->setAt(next.x, next.y - 1, next.z, GridTempObstacle);
            }
            if (m_grid->at(next.x, next.y + 1, next.z) == GridAvoid) {
                m_grid->setAt(next.x, next.y + 1, next.z, GridTempObstacle);
            }
        }
    }
    else {
        // already been here: see if source and target expansions have intersected
        if (routeThing.sourceValue == GridSource) {
            if (nextval & GridSourceFlag) return;

            next.flags |= GridPointDone;
        }
        else {
            if ((nextval & GridSourceFlag) == 0) return;

            next.flags |= GridPointDone;
        }
    }

    // any way to skip viaWillFit or put it off until actually needed?
    if (crossLayer) {
        if (!viaWillFit(next, m_grid)) return;

        // only way to cross layers is with a via
        //QPointF center = getPixelCenter(next, m_maxRect.topLeft(), m_gridPixels);
        //DebugDialog::debug(QString("via will fit %1,%2,%3 %4,%5").arg(next.x).arg(next.y).arg(next.z).arg(center.x()).arg(center.y()));
    }

    next.baseCost = gridPoint.baseCost;
    if (crossLayer) {
        next.baseCost += ViaCost;
    }
    else if (avoid) {
        next.baseCost += AvoidCost;
    }
    next.baseCost++;


    /*
    int increment = 5;
    // assume because of obstacles around the board that we can never be off grid from (next.x, next.y)
    switch(grid->at(next.x - 1, next.y, next.z)) {
        case GridObstacle:
        case GridSource:
        case GridTarget:
            increment--;
        default:
            break;
    }
    switch(grid->at(next.x + 1, next.y, next.z)) {
        case GridObstacle:
        case GridSource:
        case GridTarget:
            increment--;
        default:
            break;
    }
    switch(grid->at(next.x, next.y - 1, next.z)) {
        case GridObstacle:
        case GridSource:
        case GridTarget:
            increment--;
        default:
            break;
    }
    switch(grid->at(next.x, next.y + 1, next.z)) {
        case GridObstacle:
        case GridSource:
        case GridTarget:
            increment--;
        default:
            break;
    }
    next.cost += increment;
    */


    if (nextval == routeThing.targetValue) {
        next.qCost = next.baseCost;
    }
    else {
        double d = (m_costFunction)(QPoint(next.x, next.y), (routeThing.sourceValue == GridSource) ? routeThing.gridTargetPoint : routeThing.gridSourcePoint);
        next.qCost = next.baseCost + d;
        if (routeThing.sourceValue == GridSource) {
            if (d < routeThing.bestDistanceToTarget) {
                routeThing.bestDistanceToTarget = d;
                routeThing.bestLocationToTarget = next;
            }
        }
        else {
            if (d < routeThing.bestDistanceToSource) {
                routeThing.bestDistanceToSource = d;
                routeThing.bestLocationToSource = next;
            }
        }
    }

    // can think about pushing multiple points here
    //DebugDialog::debug(QString("pushing next %1 %2 %3, %4, %5").arg(gridPoint.x).arg(gridPoint.y).arg(gridPoint.z).arg(gridPoint.qCost).arg(routeThing.pq.size()));
    if (routeThing.sourceValue == GridSource) routeThing.sourceQ.push(next);
    else routeThing.targetQ.push(next);

    if (writeable) {
        GridValue flag = (routeThing.sourceValue == GridSource) ? GridSourceFlag : 0;
        m_grid->setAt(next.x, next.y, next.z, next.baseCost | flag);
    }

    //if (routeThing.searchForJumper) {
    //    updateDisplay(next);
    //}
}

bool MazeRouter::viaWillFit(GridPoint & gridPoint, Grid * grid) {
    for (int y = -m_halfGridViaSize; y <= m_halfGridViaSize; y++) {
        for (int x = -m_halfGridViaSize; x <= m_halfGridViaSize; x++) {
            for (int z = 0; z < grid->z; z++) {
                GridValue val = grid->at(gridPoint.x + x, gridPoint.y + y, z);
                if (val == GridPartObstacle || val == GridBoardObstacle || val == GridSource || val == GridTarget || val == GridTempObstacle || val == GridAvoid) return false;
            }
        }
    }
    return true;
}

void MazeRouter::updateDisplay(int iz) {
    QPixmap pixmap = QPixmap::fromImage(*m_displayImage[iz]);
    if (m_displayItem[iz] == NULL) {
        m_displayItem[iz] = new QGraphicsPixmapItem(pixmap);
        m_displayItem[iz]->setFlag(QGraphicsItem::ItemIsSelectable, false);
        m_displayItem[iz]->setFlag(QGraphicsItem::ItemIsMovable, false);
        //m_displayItem[iz]->setPos(iz == 1 ? m_maxRect.topLeft() : m_maxRect.topRight());
        m_displayItem[iz]->setPos(m_maxRect.topLeft());
        m_sketchWidget->scene()->addItem(m_displayItem[iz]);
        m_displayItem[iz]->setZValue(5000);
        //m_displayItem[iz]->setZValue(m_sketchWidget->viewLayers().value(iz == 0 ? ViewLayer::Copper0 : ViewLayer::Copper1)->nextZ());
        m_displayItem[iz]->setScale(m_gridPixels);   // m_maxRect.width() / m_displayImage[iz]->width()
        m_displayItem[iz]->setVisible(true);
    }
    else {
        m_displayItem[iz]->setPixmap(pixmap);
    }
    ProcessEventBlocker::processEvents();
}

void MazeRouter::updateDisplay(Grid * grid, int iz) {
    m_displayImage[iz]->fill(0);
    for (int y = 0; y < grid->y; y++) {
        for (int x = 0; x < grid->x; x++) {
            uint color = getColor(grid->at(x, y, iz));
            if (color) m_displayImage[iz]->setPixel(x, y, color);
        }
    }

    updateDisplay(iz);
}

void MazeRouter::updateDisplay(GridPoint & gridPoint) {
    //static int counter = 0;
    //if (counter++ % 2 == 0) {
        uint color = getColor(m_grid->at(gridPoint.x, gridPoint.y, gridPoint.z));
        if (color) {
            m_displayImage[gridPoint.z]->setPixel(gridPoint.x, gridPoint.y, color);
            updateDisplay(gridPoint.z);
        }
    //}
}

void MazeRouter::clearExpansion(Grid * grid) {
    // TODO: keep a list of expansion points instead?

    for (int z = 0; z < grid->z; z++) {
        for (int y = 0; y < grid->y; y++) {
            for (int x = 0; x < grid->x; x++) {
                GridValue val = grid->at(x, y, z);
                if (val == 0 || val == GridPartObstacle || val == GridBoardObstacle) ;
                else grid->setAt(x, y, z, 0);
            }
        }
    }
}

void MazeRouter::initTraceDisplay() {
    m_displayImage[0]->fill(0);
    m_displayImage[1]->fill(0);
}

void MazeRouter::displayTrace(Trace & trace) {
    if (trace.gridPoints.count() == 0) {
        DebugDialog::debug("trace with no points");
        return;
    }

    int lastz = trace.gridPoints.at(0).z;
    foreach (GridPoint gridPoint, trace.gridPoints) {
        if (gridPoint.z != lastz) {
            for (int y = -m_halfGridViaSize; y <= m_halfGridViaSize; y++) {
                for (int x = -m_halfGridViaSize; x <= m_halfGridViaSize; x++) {
                    m_displayImage[1]->setPixel(x + gridPoint.x, y + gridPoint.y, 0x80ff0000);
                }
            }
            lastz = gridPoint.z;
        }
        else {
            m_displayImage[lastz]->setPixel(gridPoint.x, gridPoint.y, m_traceColors[lastz]);
        }
    }
    
    if (trace.flags) {
        GridPoint gridPoint = trace.gridPoints.first();
        int xl, xr;
        if (trace.flags & JumperLeft) {
            xl = -(m_halfGridJumperSize * 2);
            xr = 0;
        }
        else if (trace.flags & JumperRight) {
            xl = 0;
            xr = m_halfGridJumperSize * 2;
        }
        else {
            xl = -m_halfGridJumperSize;
            xr = m_halfGridJumperSize;
        }
        for (int y = -m_halfGridJumperSize; y <= m_halfGridJumperSize; y++) {
            for (int x = xl; x <= xr; x++) {
                m_displayImage[0]->setPixel(x + gridPoint.x, y + gridPoint.y, 0x800000ff);
            }
        }
    }
}

void MazeRouter::traceObstacles(QList<Trace> & traces, int netIndex, Grid * grid, int ikeepout) {
    // treat traces from previous nets as obstacles
    foreach (Trace trace, traces) {
        if (trace.netIndex == netIndex) continue;

        int lastZ = trace.gridPoints.at(0).z;
        foreach (GridPoint gridPoint, trace.gridPoints) {
            if (gridPoint.z != lastZ) {
                for (int y = -m_halfGridViaSize; y <= m_halfGridViaSize; y++) {
                    for (int x = -m_halfGridViaSize; x <= m_halfGridViaSize; x++) {
                        grid->setAt(gridPoint.x + x, gridPoint.y + y, 0, GridBoardObstacle);
                        grid->setAt(gridPoint.x + x, gridPoint.y + y, 1, GridBoardObstacle);
                    }
                }
                lastZ = gridPoint.z;
            }
            else {
                for (int y = -ikeepout; y <= ikeepout; y++) {
                    for (int x = -ikeepout; x <= ikeepout; x++) {
                        grid->setAt(gridPoint.x + x, gridPoint.y + y, gridPoint.z, GridBoardObstacle);
                    }
                }
            }
            
        }

        if (trace.flags) {
            GridPoint gridPoint = trace.gridPoints.first();
            // jumper is always centered in this case
            for (int y = -m_halfGridJumperSize; y <= m_halfGridJumperSize; y++) {
                for (int x = -m_halfGridJumperSize; x <= m_halfGridJumperSize; x++) {
                    grid->setAt(gridPoint.x + x, gridPoint.y + y, 0, GridBoardObstacle);
                    if (m_bothSidesNow) {
                        grid->setAt(gridPoint.x + x, gridPoint.y + y, 1, GridBoardObstacle);
                    }
                }
            }
        }
    }
}

void MazeRouter::traceAvoids(QList<Trace> & traces, int netIndex, RouteThing & routeThing) {
    // treat traces from previous nets as semi-obstacles
    routeThing.avoids.clear();
    foreach (Trace trace, traces) {
        if (trace.netIndex == netIndex) continue;

        foreach (GridPoint gridPoint, trace.gridPoints) {
            for (int y = -m_keepoutGridInt; y <= m_keepoutGridInt; y++) {
                for (int x = -m_keepoutGridInt; x <= m_keepoutGridInt; x++) {
                    GridValue val = m_grid->at(gridPoint.x + x, gridPoint.y + y, 0);
                    if (val == GridPartObstacle || val == GridBoardObstacle || val == GridSource || val == GridTarget) continue;
                        
                    m_grid->setAt(gridPoint.x + x, gridPoint.y + y, 0, GridAvoid);
                    routeThing.avoids.insert(((gridPoint.y + y) * m_grid->x) + x + gridPoint.x);
                }
            }
        }

        if (trace.flags) {
            GridPoint gridPoint = trace.gridPoints.first();
            int xl, xr;
            if (gridPoint.flags & GridPointJumperLeft) {
                xl = -(m_halfGridJumperSize * 2);
                xr = 0;
            }
            else {
                xl = 0;
                xr = m_halfGridJumperSize * 2;
            }

            for (int y = -m_halfGridJumperSize; y <= m_halfGridJumperSize; y++) {
                for (int x = xl; x <= xr; x++) {
                    m_grid->setAt(gridPoint.x + x, gridPoint.y + y, 0, GridBoardObstacle);
                }
            }
        }
    }
}

void MazeRouter::cleanUpNets(NetList & netList) {
    foreach(Net * net, netList.nets) {
        delete net;
    }
    netList.nets.clear();
    Autorouter::cleanUpNets();
}

void MazeRouter::createTraces(NetList & netList, Score & bestScore, QUndoCommand * parentCommand) {
    QPointF topLeft = m_maxRect.topLeft();
    foreach (int netIndex, bestScore.ordering.order) {
        //DebugDialog::debug(QString("tracing net %1").arg(netIndex));
        QList<Trace> traces = bestScore.traces.values(netIndex);
        qSort(traces.begin(), traces.end(), byOrder);
        QList<TraceWire *> newTraces;
        QList<Via *> newVias;
        QList<JumperItem *> newJumperItems;
        QList<SymbolPaletteItem *> newNetLabels;
    
        JumperItem * jumperItem = NULL;
        SymbolPaletteItem * netLabel = NULL;
        for (int tix = 0; tix < traces.count(); tix++) {
            Trace trace = traces.at(tix);
            QList<GridPoint> gridPoints = trace.gridPoints;
            // TODO: nicer curve-fitting
            removeColinear(gridPoints);
            removeSteps(gridPoints);

            if (trace.flags & JumperStart) {
                Trace trace2 = traces.at(tix + 1);
                if (m_pcbType) {
	                long newID = ItemBase::getNextID();
	                ViewGeometry viewGeometry;
	                ItemBase * itemBase = m_sketchWidget->addItem(m_sketchWidget->referenceModel()->retrieveModelPart(ModuleIDNames::JumperModuleIDName), 
												                  ViewLayer::ThroughHoleThroughTop_TwoLayers, BaseCommand::SingleView, viewGeometry, newID, -1, NULL);

	                jumperItem = dynamic_cast<JumperItem *>(itemBase);
	                jumperItem->setAutoroutable(true);
	                m_sketchWidget->scene()->addItem(jumperItem);
                    QPointF c1 = getPixelCenter(trace.gridPoints.first(), topLeft, m_gridPixels);
                    QPointF c2 = getPixelCenter(trace2.gridPoints.first(), topLeft, m_gridPixels);
	                jumperItem->resize(c1, c2);
                    addJumperToUndo(jumperItem, parentCommand);
                    newJumperItems << jumperItem;
                }
                else {
                    netLabel = makeNetLabel(trace.gridPoints.first(), NULL, trace.flags);
                    addNetLabelToUndo(netLabel, parentCommand);
                    newNetLabels << netLabel;
                }
            }
            else if (trace.flags & JumperEnd) {
                if (m_pcbType) {
                    // keep the jumperItem we have from JumperStart
                }
                else {
                    netLabel = makeNetLabel(trace.gridPoints.first(), netLabel, trace.flags);
                    addNetLabelToUndo(netLabel, parentCommand);
                    newNetLabels << netLabel;
                }
            }
            else jumperItem = NULL;

            Net * net = netList.nets.at(netIndex);
            bool onTraceS, onTraceD;
            QPointF traceAnchorS, traceAnchorD;
            ConnectorItem * sourceConnectorItem = NULL;
            if (jumperItem) {
                onTraceS = onTraceD = false;
                sourceConnectorItem = (trace.flags & JumperStart) ? jumperItem->connector0() : jumperItem->connector1();
            }
            else if (netLabel) {
                sourceConnectorItem = netLabel->connector0();
            }
            else {
                sourceConnectorItem = findAnchor(gridPoints.first(), topLeft, net, newTraces, newVias, traceAnchorS, onTraceS, NULL);
            }
            if (sourceConnectorItem == NULL) {
                DebugDialog::debug("missing source connector");
                continue;
            }
            
            ConnectorItem * destConnectorItem = findAnchor(gridPoints.last(), topLeft, net, newTraces, newVias, traceAnchorD, onTraceD, sourceConnectorItem);
            if (destConnectorItem == NULL) {
                DebugDialog::debug("missing dest connector");
                continue;
            }

            if (sourceConnectorItem->attachedTo() == destConnectorItem->attachedTo()) {
                sourceConnectorItem->debugInfo("source");
                destConnectorItem->debugInfo("dest");
            }
            
            QPointF sourcePoint = sourceConnectorItem->sceneAdjustedTerminalPoint(NULL);
            QPointF destPoint = destConnectorItem->sceneAdjustedTerminalPoint(NULL);

            bool skipFirst = false;
            bool skipLast = false;

            GridPoint gp = gridPoints.last();
            QPointF center = getPixelCenter(gp, topLeft, m_gridPixels);
            if (!atLeast(center, destPoint)) {
                // don't need this last point
                skipLast = true;
            }

            gp = gridPoints.first();
            center = getPixelCenter(gp, topLeft, m_gridPixels);
            if (!atLeast(center, sourcePoint)) {
                skipFirst = true;               
            }

            QList<PointZ> newPoints;
            for (int i = 0; i < gridPoints.count(); i++) {
                GridPoint gp1 = gridPoints.at(i);
                QPointF c1 = getPixelCenter(gp1, topLeft, m_gridPixels);
                PointZ v1(c1, gp1.z);
                newPoints << v1;
                if ((gp1.flags & GridPointStepStart) == 0) continue;

                GridPoint gp2 = gridPoints.at(i + 1);
                QPointF c2 = getPixelCenter(gp2, topLeft, m_gridPixels);

                QPointF p = getStepPoint(c1, gp1.flags, m_gridPixels);
                PointZ v2(p, gp1.z);
                newPoints << v2;

                QPointF q = getStepPoint(c2, gp2.flags, m_gridPixels);
                PointZ v3(q, gp2.z);
                newPoints << v3;

                //DebugDialog::debug(QString("remove2 %1,%2 %3,%4").arg(c1.x()).arg(c1.y()).arg(p.x()).arg(p.y()));
                //DebugDialog::debug(QString("\t%1,%2 %3,%4").arg(q.x()).arg(q.y()).arg(c2.x()).arg(c2.y()));

            }

            if (skipLast) {
                newPoints.takeLast();
            }
            PointZ point = newPoints.takeFirst();
            if (skipFirst) {
                point = newPoints.takeFirst();
            }

            int lastz = point.z;
            ConnectorItem * nextSource = NULL;
            if (onTraceS) {
                if (!atLeast(sourcePoint, traceAnchorS)) {
                    onTraceS = false;
                }
                else if (atLeast(point.p, traceAnchorS)) {
                    onTraceS = false;
                }
            }
            if (onTraceS) {
                TraceWire * traceWire1 = drawOneTrace(sourcePoint, traceAnchorS, m_standardWireWidth, lastz == 0 ? ViewLayer::Bottom : ViewLayer::Top);
                addWireToUndo(traceWire1, parentCommand);
                addConnectionToUndo(sourceConnectorItem, traceWire1->connector0(), parentCommand);
                newTraces << traceWire1;
               
                TraceWire* traceWire2 = drawOneTrace(traceAnchorS, point.p, m_standardWireWidth, lastz == 0 ? ViewLayer::Bottom : ViewLayer::Top);
                addWireToUndo(traceWire2, parentCommand);
                addConnectionToUndo(traceWire1->connector1(), traceWire2->connector0(), parentCommand);
                nextSource = traceWire2->connector1();
                newTraces << traceWire2;
            }
            else {
                TraceWire * traceWire = drawOneTrace(sourcePoint, point.p, m_standardWireWidth, lastz == 0 ? ViewLayer::Bottom : ViewLayer::Top);
                addWireToUndo(traceWire, parentCommand);
                addConnectionToUndo(sourceConnectorItem, traceWire->connector0(), parentCommand);
                nextSource = traceWire->connector1();
                newTraces << traceWire;
            }

            foreach (PointZ newPoint, newPoints) {
                if (newPoint.z == lastz) {
                    TraceWire * traceWire = drawOneTrace(point.p, newPoint.p, m_standardWireWidth, lastz == 0 ? ViewLayer::Bottom : ViewLayer::Top);
                    addWireToUndo(traceWire, parentCommand);
                    addConnectionToUndo(nextSource, traceWire->connector0(), parentCommand);
                    nextSource = traceWire->connector1();
                    newTraces << traceWire;
                }
                else {
	                long newID = ItemBase::getNextID();
	                ViewGeometry viewGeometry;
	                double ringThickness, holeSize;
	                m_sketchWidget->getViaSize(ringThickness, holeSize);
                    double halfVia = (ringThickness + ringThickness + holeSize) / 2;

	                viewGeometry.setLoc(QPointF(newPoint.p.x() - halfVia - Hole::OffsetPixels, newPoint.p.y() - halfVia - Hole::OffsetPixels));
	                ItemBase * itemBase = m_sketchWidget->addItem(m_sketchWidget->referenceModel()->retrieveModelPart(ModuleIDNames::ViaModuleIDName), 
										                ViewLayer::ThroughHoleThroughTop_TwoLayers, BaseCommand::SingleView, viewGeometry, newID, -1, NULL);

	                //DebugDialog::debug(QString("back from adding via %1").arg((long) itemBase, 0, 16));
	                Via * via = qobject_cast<Via *>(itemBase);
	                via->setAutoroutable(true);
	                via->setHoleSize(QString("%1in,%2in") .arg(holeSize / GraphicsUtils::SVGDPI) .arg(ringThickness / GraphicsUtils::SVGDPI), false);
                    addViaToUndo(via, parentCommand);
                    addConnectionToUndo(nextSource, via->connectorItem(), parentCommand);
                    nextSource = via->connectorItem();
                    newVias << via;
                    lastz = newPoint.z;
                }
                point = newPoint;
            }
            if (onTraceD) {
                if (!atLeast(destPoint, traceAnchorD)) {
                    onTraceD = false;
                }
                else if (!atLeast(point.p, traceAnchorD)) {
                    onTraceD = false;
                }
            }
            if (onTraceD) {
                TraceWire * traceWire1 = drawOneTrace(point.p, traceAnchorD, m_standardWireWidth, lastz == 0 ? ViewLayer::Bottom : ViewLayer::Top);
                addWireToUndo(traceWire1, parentCommand);
                addConnectionToUndo(nextSource, traceWire1->connector0(), parentCommand);
                newTraces << traceWire1;

                TraceWire * traceWire2 = drawOneTrace(traceAnchorD, destPoint, m_standardWireWidth, lastz == 0 ? ViewLayer::Bottom : ViewLayer::Top);
                addWireToUndo(traceWire2, parentCommand);
                addConnectionToUndo(traceWire1->connector1(), traceWire2->connector0(), parentCommand);
                addConnectionToUndo(traceWire2->connector1(), destConnectorItem, parentCommand);
                newTraces << traceWire2;
            }
            else {
                TraceWire * traceWire = drawOneTrace(point.p, destPoint, m_standardWireWidth, lastz == 0 ? ViewLayer::Bottom : ViewLayer::Top);
                addWireToUndo(traceWire, parentCommand);
                addConnectionToUndo(nextSource, traceWire->connector0(), parentCommand);
                addConnectionToUndo(traceWire->connector1(), destConnectorItem, parentCommand);
                newTraces << traceWire;
            }
        }

        foreach (TraceWire * traceWire, newTraces) {
            delete traceWire;
        }
        foreach (SymbolPaletteItem * netLabel, newNetLabels) {
            delete netLabel;
        }
        foreach (Via * via, newVias) {
            via->removeLayerKin();
            delete via;
        }
        foreach (JumperItem * jumperItem, newJumperItems) {
            jumperItem->removeLayerKin();
            delete jumperItem;
        }
    }
}

ConnectorItem * MazeRouter::findAnchor(GridPoint gp, QPointF topLeft, Net * net, QList<TraceWire *> & newTraces, QList<Via *> & newVias, QPointF & p, bool & onTrace, ConnectorItem * already) 
{
    QRectF gridRect(gp.x * m_gridPixels + topLeft.x(), gp.y * m_gridPixels + topLeft.y(), m_gridPixels, m_gridPixels);
    ConnectorItem * connectorItem = findAnchor(gp, gridRect, net, newTraces, newVias, p, onTrace, already);
    if (connectorItem) return connectorItem;

    gridRect.adjust(-m_gridPixels, -m_gridPixels, m_gridPixels, m_gridPixels);
    return findAnchor(gp, gridRect, net, newTraces, newVias, p, onTrace, already);
}

ConnectorItem * MazeRouter::findAnchor(GridPoint gp, const QRectF & gridRect, Net * net, QList<TraceWire *> & newTraces, QList<Via *> & newVias, QPointF & p, bool & onTrace, ConnectorItem * already) 
{
    ConnectorItem * alreadyCross = NULL;
    if (already != NULL) alreadyCross = already->getCrossLayerConnectorItem();
    QList<TraceWire *> traceWires;
    foreach (QGraphicsItem * item, m_sketchWidget->scene()->items(gridRect)) {
        ConnectorItem * connectorItem = dynamic_cast<ConnectorItem *>(item);
        if (connectorItem) {
            if (!connectorItem->attachedTo()->isEverVisible()) continue;
            if (connectorItem == already) continue;
            if (connectorItem == alreadyCross) continue;

            if (already != NULL && connectorItem->attachedTo() == already->attachedTo()) {
                ConnectorItem * cross = connectorItem->getCrossLayerConnectorItem();
                if (cross != NULL) {
                    if (cross == already) continue;
                    if (cross == alreadyCross) continue;
                }
            }

            bool isCandidate = 
                (gp.z == 0 && m_sketchWidget->attachedToBottomLayer(connectorItem)) ||
                (gp.z == 1 && m_sketchWidget->attachedToTopLayer(connectorItem))
             ;
            if (!isCandidate) continue;

            if (net->net->contains(connectorItem)) ;
            else {
                TraceWire * traceWire = qobject_cast<TraceWire *>(connectorItem->attachedTo());
                if (traceWire == NULL) {
                    Via * via = qobject_cast<Via *>(connectorItem->attachedTo());
                    if (via == NULL) isCandidate = false;
                    else isCandidate = newVias.contains(via);
                }
                else isCandidate = newTraces.contains(traceWire);
            }
            if (!isCandidate) continue;

            onTrace = false;
            p = connectorItem->sceneAdjustedTerminalPoint(NULL);
            return connectorItem;
        }

        TraceWire * traceWire = dynamic_cast<TraceWire *>(item);
        if (traceWire == NULL) continue;
        if (!traceWire->isEverVisible()) continue;

        // only do traces if no connectorItem is found
        traceWires.append(traceWire);
    }

    foreach (TraceWire * traceWire, traceWires) {
        bool isCandidate = (gp.z == 0 && m_sketchWidget->attachedToBottomLayer(traceWire->connector0()))
                        || (gp.z == 1 && m_sketchWidget->attachedToTopLayer(traceWire->connector0()));
        if (!isCandidate) continue;

        if (newTraces.contains(traceWire)) ;
        else if (net->net->contains(traceWire->connector0())) ;
        else continue;

        onTrace = true;
        QPointF center = gridRect.center();
        QPointF p0 = traceWire->connector0()->sceneAdjustedTerminalPoint(NULL);
        QPointF p1 = traceWire->connector1()->sceneAdjustedTerminalPoint(NULL);
        double d0 = GraphicsUtils::distanceSqd(p0, center);
        double d1 = GraphicsUtils::distanceSqd(p1, center);
        double dx, dy, distanceSegment;
        bool atEndpoint;
        GraphicsUtils::distanceFromLine(center.x(), center.y(), p0.x(), p0.y(), p1.x(), p1.y(), dx, dy, distanceSegment, atEndpoint);
        if (atEndpoint) {
            DebugDialog::debug("at endpoint shouldn't happen");
        }
        p.setX(dx);
        p.setY(dy);
        if (d0 <= d1) {
            return traceWire->connector0();
        }
        else {
            return traceWire->connector1();
        }
    }

    DebugDialog::debug("overlap not found");
    return NULL;
}

void MazeRouter::removeColinear(QList<GridPoint> & gridPoints) {
    	// eliminate redundant colinear points
	int ix = 0;
	while (ix < gridPoints.count() - 2) {
		GridPoint p1 = gridPoints[ix];
		GridPoint p2 = gridPoints[ix + 1];
        if (p1.z == p2.z) {
		    GridPoint p3 = gridPoints[ix + 2];
            if (p2.z == p3.z) {
		        if (p1.x == p2.x && p2.x == p3.x) {
			        gridPoints.removeAt(ix + 1);
			        continue;
		        }
		        else if (p1.y == p2.y && p2.y == p3.y) {
			        gridPoints.removeAt(ix + 1);
			        continue;
		        }
            }
        }
		ix++;
	}
}

void MazeRouter::removeSteps(QList<GridPoint> & gridPoints) {
    // eliminate 45 degree runs
	for (int ix = 0; ix < gridPoints.count() - 2; ix++) {
        removeStep(ix, gridPoints);
	}
}

void MazeRouter::removeStep(int ix, QList<GridPoint> & gridPoints) {
	GridPoint p1 = gridPoints[ix];
	GridPoint p2 = gridPoints[ix + 1];
    if (p1.z != p2.z) return;

	GridPoint p3 = gridPoints[ix + 2];
    if (p2.z != p3.z) return;

    int dx1 = p2.x - p1.x;
    int dy1 = p2.y - p1.y;
    if ((qAbs(dx1) == 1 && dy1 == 0) || (dx1 == 0 && qAbs(dy1) == 1)) ;
    else return;

    int dx2 = p3.x - p2.x;
    int dy2 = p3.y - p2.y;
    bool step = false;
    if (dx1 == 0) {
        step = (dy2 == 0 && qAbs(dx2) == 1);
    }
    else {
        step = (dx2 == 0 && qAbs(dy2) == 1);
    }
    if (!step) return;

    int count = 1;
    int flag = 0;
    for (int jx = ix + 3; jx < gridPoints.count(); jx++, flag++) {
        GridPoint p4 = gridPoints[jx];
        int dx3 = p4.x - p3.x;
        int dy3 = p4.y - p3.y;
        if (flag % 2 == 0) {
            if (dx3 == dx1 && dy3 == dy1) {
                count++;
            }
            else break;
        }
        else {
            if (dx3 == dx2 && dy3 == dy2) {
                count++;
            }
            else break;
        }
        p2 = p3;
        p3 = p4;
    }

    gridPoints[ix].flags = getStepFlag(gridPoints[ix], gridPoints[ix + 1]) | GridPointStepStart;
    int jx = ix + count + 1;
    gridPoints[jx].flags = getStepFlag(gridPoints[jx], gridPoints[jx - 1]);

    /*
    QPointF topLeft = m_maxRect.topLeft();
    DebugDialog::debug(QString("removing %1").arg(count));
    for (int i = 0; i < count + 2; i++) {
        QPointF p = getPixelCenter(gridPoints[ix + i], topLeft, m_gridPixels);
        DebugDialog::debug(QString("\t%1,%2 %3,%4").arg(gridPoints[ix + i].x).arg(gridPoints[ix + i].y).arg(p.x()).arg(p.y()));
    }
    */

    while (--count >= 0) {
        gridPoints.removeAt(ix + 1);
    }

}

void MazeRouter::addConnectionToUndo(ConnectorItem * from, ConnectorItem * to, QUndoCommand * parentCommand) 
{
	ChangeConnectionCommand * ccc = new ChangeConnectionCommand(m_sketchWidget, BaseCommand::CrossView, 
											from->attachedToID(), from->connectorSharedID(),
											to->attachedToID(), to->connectorSharedID(),
											ViewLayer::specFromID(from->attachedToViewLayerID()),
											true, parentCommand);
	ccc->setUpdateConnections(false);
}

void MazeRouter::addViaToUndo(Via * via, QUndoCommand * parentCommand) {
	new AddItemCommand(m_sketchWidget, BaseCommand::CrossView, ModuleIDNames::ViaModuleIDName, via->viewLayerSpec(), via->getViewGeometry(), via->id(), false, -1, parentCommand);
	new SetPropCommand(m_sketchWidget, via->id(), "hole size", via->holeSize(), via->holeSize(), true, parentCommand);
	new CheckStickyCommand(m_sketchWidget, BaseCommand::SingleView, via->id(), false, CheckStickyCommand::RemoveOnly, parentCommand);
}

void MazeRouter::addJumperToUndo(JumperItem * jumperItem, QUndoCommand * parentCommand) {
	jumperItem->saveParams();
	QPointF pos, c0, c1;
	jumperItem->getParams(pos, c0, c1);

	new AddItemCommand(m_sketchWidget, BaseCommand::CrossView, ModuleIDNames::JumperModuleIDName, jumperItem->viewLayerSpec(), jumperItem->getViewGeometry(), jumperItem->id(), false, -1, parentCommand);
	new ResizeJumperItemCommand(m_sketchWidget, jumperItem->id(), pos, c0, c1, pos, c0, c1, parentCommand);
	new CheckStickyCommand(m_sketchWidget, BaseCommand::SingleView, jumperItem->id(), false, CheckStickyCommand::RemoveOnly, parentCommand);
}

void MazeRouter::addNetLabelToUndo(SymbolPaletteItem * netLabel, QUndoCommand * parentCommand) {
	new AddItemCommand(m_sketchWidget, BaseCommand::CrossView, netLabel->moduleID(), netLabel->viewLayerSpec(), netLabel->getViewGeometry(), netLabel->id(), false, -1, parentCommand);
	new SetPropCommand(m_sketchWidget, netLabel->id(), "label", netLabel->getLabel(), netLabel->getLabel(), true, parentCommand);
}

void MazeRouter::insertTrace(Trace & newTrace, int netIndex, Score & currentScore, int viaCount, bool incRouted) {
    if (newTrace.gridPoints.count() == 0) {
        DebugDialog::debug("trace with no points");
        return;
    }

    //DebugDialog::debug(QString("insert trace %1").arg(newTrace.gridPoints.count()));

    newTrace.netIndex = netIndex;
    newTrace.order = currentScore.traces.values(netIndex).count();
    currentScore.traces.insert(netIndex, newTrace);
    if (incRouted) {
        currentScore.routedCount.insert(netIndex, currentScore.routedCount.value(netIndex) + 1);
        currentScore.totalRoutedCount++;
    }
    currentScore.viaCount.insert(netIndex, currentScore.viaCount.value(netIndex, 0) + viaCount);
    currentScore.totalViaCount += viaCount;
    displayTrace(newTrace);

    //DebugDialog::debug(QString("done insert trace"));

}

void MazeRouter::incProgress() {
    emit setProgressValue(m_cleanupCount++);
}

void MazeRouter::setMaxCycles(int maxCycles) 
{
	Autorouter::setMaxCycles(maxCycles);
    emit setMaximumProgress(maxCycles);
}

SymbolPaletteItem * MazeRouter::makeNetLabel(GridPoint & center, SymbolPaletteItem * pairedNetLabel, uchar traceFlags) {
    // flags & JumperLeft means position the netlabel to the left of center, the netlabel points right

    if (m_netLabelIndex < 0) {
        m_netLabelIndex = 0;
        foreach (QGraphicsItem * item, m_sketchWidget->scene()->items()) {
            SymbolPaletteItem * netLabel = dynamic_cast<SymbolPaletteItem *>(item);
            if (netLabel == NULL || !netLabel->isNetLabel()) continue;

            bool ok;
            int ix = netLabel->getLabel().toInt(&ok);
            if (ok && ix > m_netLabelIndex) m_netLabelIndex = ix;
        }
    }

    if (pairedNetLabel == NULL) {
        m_netLabelIndex++;
    }

	long newID = ItemBase::getNextID();
    ViewGeometry viewGeometry;
	ItemBase * itemBase = m_sketchWidget->addItem(m_sketchWidget->referenceModel()->retrieveModelPart(traceFlags & JumperLeft ? ModuleIDNames::NetLabelModuleIDName : ModuleIDNames::LeftNetLabelModuleIDName), 
												    ViewLayer::ThroughHoleThroughTop_TwoLayers, BaseCommand::SingleView, viewGeometry, newID, -1, NULL);

	SymbolPaletteItem * netLabel = dynamic_cast<SymbolPaletteItem *>(itemBase);
	netLabel->setAutoroutable(true);
    netLabel->setLabel(QString::number(m_netLabelIndex));
    QPointF c1 = getPixelCenter(center, m_maxRect.topLeft(), m_gridPixels);
    QSizeF size = netLabel->boundingRect().size();
    int x = c1.x();
    if (traceFlags & JumperLeft) {
        x -= size.width();
    }
    netLabel->setPos(x, c1.y() - (size.height() / 2));
    DebugDialog::debug(QString("ix:%1 tl:%2 %3,%4").arg(m_netLabelIndex).arg(traceFlags).arg(x).arg(c1.y() - (size.height() / 2)));
    netLabel->saveGeometry();
    return netLabel;
}

void MazeRouter::routeJumper(int netIndex, RouteThing & routeThing, Score & currentScore) 
{
    if (routeThing.bestDistanceToTarget == std::numeric_limits<GridValue>::max() || routeThing.bestDistanceToSource == std::numeric_limits<GridValue>::max()) {
        // never got started on this route
        return;
    }

    updateDisplay(0);
    if (m_bothSidesNow) updateDisplay(1);

    bool routeBothEnds = true;
    Trace sourceTrace;
    if (!m_pcbType) {
        //  is there already a net label on this net?

        // TODO: which subnet has the jumper?
        foreach (Trace trace, currentScore.traces.values(netIndex)) {
            if (trace.flags & JumperStart) {
                sourceTrace = trace;
                routeBothEnds = false;
                break;
            }
        }
    }

    updateDisplay(m_grid, 0);
    if (m_bothSidesNow) updateDisplay(m_grid, 1);

    GridPoint gp1 = lookForJumper(routeThing.bestLocationToTarget, GridSource, routeThing.gridTargetPoint);
    if (gp1.baseCost == GridBoardObstacle) return;

    GridPoint gp2 = lookForJumper(routeThing.bestLocationToSource, GridTarget, routeThing.gridSourcePoint);
    if (gp2.baseCost == GridBoardObstacle) return;

    int sourceViaCount;
    if (routeBothEnds) {
        sourceTrace.flags = JumperStart;
        if (gp1.flags & GridPointJumperLeft) sourceTrace.flags |= JumperLeft;
        else if (gp1.flags & GridPointJumperRight) sourceTrace.flags |= JumperRight;
        sourceTrace.gridPoints = traceBack(gp1, m_grid, sourceViaCount, GridTarget, GridSource);   // trace back to source
    }
    
    Trace destTrace;
    destTrace.flags = JumperEnd;
    if (gp2.flags & GridPointJumperLeft) destTrace.flags |= JumperLeft;
    else if (gp2.flags & GridPointJumperRight) destTrace.flags |= JumperRight;
    int targetViaCount;
    destTrace.gridPoints = traceBack(gp2, m_grid, targetViaCount, GridSource, GridTarget);          // trace back to target

    if (routeBothEnds) {
        insertTrace(sourceTrace, netIndex, currentScore, sourceViaCount, false);
    }
    insertTrace(destTrace, netIndex, currentScore, targetViaCount, true);
    updateDisplay(0);
    if (m_bothSidesNow) updateDisplay(1);

    clearExpansion(m_grid);  
}

GridPoint MazeRouter::lookForJumper(GridPoint initial, GridValue targetValue, QPoint targetLocation) {
    QSet<int> already;
    std::priority_queue<GridPoint> pq;
    initial.qCost = 0;
    pq.push(initial);
    already.insert(gridPointInt(m_grid, initial));
    while (!pq.empty()) {
        GridPoint gp = pq.top();
        pq.pop();

        GridValue bc = gp.baseCost = m_grid->at(gp.x, gp.y, gp.z);
        if (targetValue == GridSource) bc ^= GridSourceFlag;
        if (bc > 255) bc = 255;


        m_displayImage[gp.z]->setPixel(gp.x, gp.y, 0xff000000 | (bc << 16) | (bc << 8) | bc);
        updateDisplay(gp.z);

        if ((*m_jumperWillFitFunction)(gp, m_grid, m_halfGridJumperSize)) {
            if (targetValue == GridSource) gp.baseCost ^= GridSourceFlag;

            m_displayImage[gp.z]->setPixel(gp.x, gp.y, 0xff0000ff);
            updateDisplay(gp.z);
            updateDisplay(gp.z);

            QPointF p = getPixelCenter(gp, m_maxRect.topLeft(), m_gridPixels);
            DebugDialog::debug(QString("jumper location %1").arg(gp.flags), p);

            return gp;
        }

        if (gp.x > 0) expandOneJ(gp, pq, -1, 0, 0, targetValue, targetLocation, already);          
        if (gp.x < m_grid->x - 1) expandOneJ(gp, pq, 1, 0, 0, targetValue, targetLocation, already);
        if (gp.y > 0) expandOneJ(gp, pq, 0, -1, 0, targetValue, targetLocation, already);
        if (gp.y < m_grid->y - 1) expandOneJ(gp, pq, 0, 1, 0, targetValue, targetLocation, already);
        if (m_bothSidesNow) {
            if (gp.z > 0) expandOneJ(gp, pq, 0, 0, -1, targetValue, targetLocation, already);
            if (gp.z < m_grid->z - 1) expandOneJ(gp, pq, 0, 0, 1, targetValue, targetLocation, already);
        }
    }

    GridPoint failed;
    failed.baseCost = GridBoardObstacle;
    return failed;
}

void MazeRouter::expandOneJ(GridPoint & gridPoint, std::priority_queue<GridPoint> & pq, int dx, int dy, int dz, GridValue targetValue, QPoint targetLocation, QSet<int> & already)
{
    GridPoint next;
    next.x = gridPoint.x + dx;
    next.y = gridPoint.y + dy;
    next.z = gridPoint.z + dz;
    int gpi = gridPointInt(m_grid, next);
    if (already.contains(gpi)) return;

    already.insert(gpi);
    GridValue nextval = m_grid->at(next.x, next.y, next.z);
    if (nextval == GridPartObstacle || nextval == GridBoardObstacle || nextval == GridSource || nextval == GridTempObstacle || nextval == GridTarget || nextval == GridAvoid) return;
    else if (nextval == 0) return;
    else {
        // already been here: see if it's the right expansion
        if (targetValue == GridSource) {
            if ((nextval & GridSourceFlag) == 0) return;
        }
        else {
            if (nextval & GridSourceFlag) return;
        }
    }

    double d = (m_costFunction)(QPoint(next.x, next.y), targetLocation);
    next.qCost = d;
    next.baseCost = 0;

    //DebugDialog::debug(QString("pushing next %1 %2 %3, %4, %5").arg(gridPoint.x).arg(gridPoint.y).arg(gridPoint.z).arg(gridPoint.qCost).arg(routeThing.pq.size()));
    pq.push(next);
}

void MazeRouter::removeOffBoardAnd(bool isPCBType, bool removeSingletons, bool bothSides) {
    QRectF boardRect;
    if (m_board) boardRect = m_board->sceneBoundingRect();
	// remove any vias or jumperItems that will be deleted, also remove off-board items
	for (int i = m_allPartConnectorItems.count() - 1; i >= 0; i--) {
		QList<ConnectorItem *> * connectorItems = m_allPartConnectorItems.at(i);
        if (removeSingletons) {
            if (connectorItems->count() < 2) {
                connectorItems->clear();
            }
            else if (connectorItems->count() == 2) {
                if (connectorItems->at(0) == connectorItems->at(1)->getCrossLayerConnectorItem()) {
                    connectorItems->clear();
                }
            }
        }
		for (int j = connectorItems->count() - 1; j >= 0; j--) {
			ConnectorItem * connectorItem = connectorItems->at(j);
			//connectorItem->debugInfo("pci");
			bool doRemove = false;
			if (connectorItem->attachedToItemType() == ModelPart::Via) {
				Via * via = qobject_cast<Via *>(connectorItem->attachedTo()->layerKinChief());
				doRemove = via->getAutoroutable();
			}
			else if (connectorItem->attachedToItemType() == ModelPart::Jumper) {
				JumperItem * jumperItem = qobject_cast<JumperItem *>(connectorItem->attachedTo()->layerKinChief());
				doRemove = jumperItem->getAutoroutable();
			}
			else if (connectorItem->attachedToItemType() == ModelPart::Symbol) {
				SymbolPaletteItem * netLabel = qobject_cast<SymbolPaletteItem *>(connectorItem->attachedTo()->layerKinChief());
				doRemove = netLabel->getAutoroutable() && netLabel->isNetLabel();
			}
            if (!bothSides && connectorItem->attachedToViewLayerID() == ViewLayer::Copper1) doRemove = true;
            if (!doRemove && isPCBType) {
                if (!connectorItem->sceneBoundingRect().intersects(boardRect)) {
                    doRemove = true;
                }
            }
			if (doRemove) {
				connectorItems->removeAt(j);
			}
		}
		if (connectorItems->count() == 0) {
			m_allPartConnectorItems.removeAt(i);
			delete connectorItems;
		}
	}
}
