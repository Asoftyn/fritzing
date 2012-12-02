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

// TODO:
//
//
//      schematic view
//          use a different cost function: cost is manhattandistance + penalty for direction changes
//          netlabels are the equivalent of jumpers. Use successive numbers for the labels.
//
//      net reordering/rip-up-and-reroute
//          is there a better way than just move by one?
//          must fix up unrouted and via count when currentScore is reordered
//
//      keepout dialog
//   
//      jumperitem
//          need to place this at the moment the route fails
//              can possibly use the expansion for one end of the jumper, but need a new search for the other end
//              must ensure minimum distance from connector
//
//      feedback
//          use qgraphicpixmapitems but update only at new trace time
//              use trace colors for top and bottom, and set at appropriate z level
//
//      raster back to vector
//
//      hold the set of successful traces
//          plus sets of points for vias or jumpers?
//
//      stop-now  
//
//      dynamic cost function based on distance to any target point?
//
//      how to determine splitDNA
//          the other way to do it would be to allow overlaps and draw a second trace back to the nearest connector
//
//      via placement must ensure minimum distance from source
//
//      check clean up is really clearing pointers
//

#include "mazerouter.h"
#include "../../sketch/pcbsketchwidget.h"
#include "../../debugdialog.h"
#include "../../items/virtualwire.h"
#include "../../items/tracewire.h"
#include "../../items/jumperitem.h"
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
#include <QCryptographicHash>

#include <qmath.h>
#include <limits>

//////////////////////////////////////

static const int MaximumProgress = 1000;

static QString CancelledMessage;

static const int DefaultMaxCycles = 10;

static const quint32 GridObstacle = 0xffffffff;
static const quint32 GridSource = 0xfffffffe;
static const quint32 GridTarget = 0xfffffffd;
static const quint32 GridIllegal = 0xfffffffc;

static const uint Layer1Cost = 100;
static const uint CrossLayerCost = 100;
static const uint ViaCost = 1000;

static const uchar GridPointDone = 1;

////////////////////////////////////////////////////////////////////

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
    QStringList svgIDs = partIDs.values(partID);
    if (svgIDs.count() == 0) return false;

    QDomElement tempElement = element;
    while (tempElement.attribute("partID").isEmpty()) {
        QString id = tempElement.attribute("id");
        if (!id.isEmpty() && svgIDs.contains(id)) return true;

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

inline double initialCost(QPointF p1, QPointF p2) {
    //return qAbs(p1.x() - p2.x()) + qAbs(p1.y() - p2.y());
    return qSqrt(GraphicsUtils::distanceSqd(p1, p2));
}

inline double initialCost(QPoint p1, QPoint p2) {
    //return qAbs(p1.x() - p2.x()) + qAbs(p1.y() - p2.y());
    return qSqrt(GraphicsUtils::distanceSqd(p1, p2));
}

inline double aStarCost(QPoint p1, QPoint p2) {
    //return qAbs(p1.x() - p2.x()) * qAbs(p1.y() - p2.y());
    return GraphicsUtils::distanceSqd(p1, p2);
}

////////////////////////////////////////////////////////////////////

bool GridPoint::operator<(const GridPoint& other) const {
    // make sure lower cost is first
    return cost > other.cost;
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

    data = (quint32 *) calloc(x * y * z, 4);   // calloc initializes grid to 0
}

uint Grid::at(int sx, int sy, int sz) {
    return *(data + (sz * y * x) + (sy * x) + sx);
}

void Grid::setAt(int sx, int sy, int sz, uint value) {
   *(data + (sz * y * x) + (sy * x) + sx) = value;
}

QList<QPoint> Grid::init(int sx, int sy, int sz, int width, int height, const QImage & image, quint32 value, bool collectPoints) {
    QList<QPoint> points;
    const uchar * bits1 = image.constScanLine(0);
    int bytesPerLine = image.bytesPerLine();
	for (int iy = sy; iy < sy + height; iy++) {
        int offset = iy * bytesPerLine;
		for (int ix = sx; ix < sx + width; ix++) {
            int byteOffset = (ix >> 3) + offset;
            uchar mask = DRC::BitTable[ix & 7];

            if (*(bits1 + byteOffset) & mask) continue;

            setAt(ix, iy, sz, value);
            if (collectPoints) {
                points.append(QPoint(ix, iy));
            }
			//DebugDialog::debug(QString("p1:%1 p2:%2").arg(p1, 0, 16).arg(p2, 0, 16));
		}
	}

    return points;
}

////////////////////////////////////////////////////////////////////


Score::Score() {
	totalUnroutedCount = totalViaCount = 0;
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
            int c = unroutedCount.value(netIndex);
            unroutedCount.remove(netIndex);
            totalUnroutedCount -= c;
            c = viaCount.value(netIndex);
            viaCount.remove(netIndex);
            totalViaCount -= c;
        }
    }
    ordering = _ordering;
    printOrder("new  ", ordering.order);
}

////////////////////////////////////////////////////////////////////

MazeRouter::MazeRouter(PCBSketchWidget * sketchWidget, ItemBase * board, bool adjustIf) : Autorouter(sketchWidget)
{
    m_displayItem[0] = m_displayItem[1] = NULL;
    m_displayImage[0] = m_displayImage[1] = NULL;

    CancelledMessage = tr("Autorouter was cancelled.");

	QSettings settings;
	m_maxCycles = settings.value("cmrouter/maxcycles", DefaultMaxCycles).toInt();
		
	m_bothSidesNow = sketchWidget->routeBothSides();
	m_board = board;

	if (m_board) {
		m_maxRect = m_board->sceneBoundingRect();
	}
	else {
		m_maxRect = m_sketchWidget->scene()->itemsBoundingRect();
		if (adjustIf) {
			    m_maxRect.adjust(-m_maxRect.width() / 2, -m_maxRect.height() / 2, m_maxRect.width() / 2, m_maxRect.height() / 2);
		}
	}

    m_standardWireWidth = m_sketchWidget->getAutorouterTraceWidth();

    // for debugging leave the last result hanging around
    QList<QGraphicsPixmapItem *> pixmapItems;
    foreach (QGraphicsItem * item, m_sketchWidget->scene()->items()) {
        QGraphicsPixmapItem * pixmapItem = dynamic_cast<QGraphicsPixmapItem *>(item);
        if (pixmapItem) pixmapItems << pixmapItem;
    }
    foreach (QGraphicsPixmapItem * pixmapItem, pixmapItems) {
        delete pixmapItem;
    }

	ViewGeometry vg;
	vg.setWireFlags(m_sketchWidget->getTraceFlag());
	ViewLayer::ViewLayerID copper0 = sketchWidget->getWireViewLayerID(vg, ViewLayer::Bottom);
	m_viewLayerIDs << copper0;
	if  (m_bothSidesNow) {
		ViewLayer::ViewLayerID copper1 = sketchWidget->getWireViewLayerID(vg, ViewLayer::Top);
		m_viewLayerIDs.append(copper1);
	}
}

MazeRouter::~MazeRouter()
{
    foreach (QDomDocument * doc, m_masterDocs) {
        delete doc;
    }
    if (m_displayItem[0]) {
        //delete m_displayItem[0];
    }
    if (m_displayItem[1]) {
        //delete m_displayItem[1];
    }
    if (m_displayImage[0]) {
        delete m_displayImage[0];
    }
    if (m_displayImage[1]) {
        delete m_displayImage[1];
    }
}

void MazeRouter::start()
{	
    bool isPCBType = m_sketchWidget->autorouteTypePCB();
	if (isPCBType && m_board == NULL) {
		QMessageBox::warning(NULL, QObject::tr("Fritzing"), QObject::tr("Cannot autoroute: no board (or multiple boards) found"));
		return;
	}

	m_maximumProgressPart = 1;
	m_currentProgressPart = 0;
	m_keepoutPixels = m_sketchWidget->getKeepout();			// 15 mils space (in pixels)
    m_keepoutMils = m_keepoutPixels * GraphicsUtils::StandardFritzingDPI / GraphicsUtils::SVGDPI;
    m_keepoutGrid = m_keepoutPixels / m_standardWireWidth;

    double ringThickness, holeSize;
	m_sketchWidget->getViaSize(ringThickness, holeSize);
	m_gridViaSize = qCeil((ringThickness + ringThickness + holeSize + m_keepoutPixels + m_keepoutPixels) / m_standardWireWidth);
    m_halfGridViaSize = m_gridViaSize / 2;

	emit setMaximumProgress(MaximumProgress);
	emit setProgressMessage("");
	emit setCycleMessage("round 1 of:");
	emit setCycleCount(m_maxCycles);

	RoutingStatus routingStatus;
	routingStatus.zero();

	m_sketchWidget->ensureTraceLayersVisible();

	QHash<ConnectorItem *, int> indexer;
	m_sketchWidget->collectAllNets(indexer, m_allPartConnectorItems, false, m_bothSidesNow);

    removeOffBoard(isPCBType, true);

	if (m_allPartConnectorItems.count() == 0) {
        QString message = isPCBType ?  QObject::tr("No connections (on the PCB) to route.") : QObject::tr("No connections to route.");
		QMessageBox::information(NULL, QObject::tr("Fritzing"), message);
		Autorouter::cleanUpNets();
		return;
	}

	// will list connectors on both sides separately
	routingStatus.m_netCount = m_allPartConnectorItems.count();

	QVector<int> netCounters(m_allPartConnectorItems.count());
    NetList netList;
	for (int i = 0; i < m_allPartConnectorItems.count(); i++) {
		netCounters[i] = (m_allPartConnectorItems[i]->count() - 1) * 2;			// since we use two connectors at a time on a net
        Net * net = new Net;
        net->net = m_allPartConnectorItems[i];

        QList<ConnectorItem *> todo;
        todo.append(*(net->net));
        while (todo.count() > 0) {
            ConnectorItem * first = todo.takeFirst();
            QList<ConnectorItem *> equi;
            equi.append(first);
	        ConnectorItem::collectEqualPotential(equi, m_bothSidesNow, (ViewGeometry::RatsnestFlag | ViewGeometry::NormalFlag | ViewGeometry::PCBTraceFlag | ViewGeometry::SchematicTraceFlag) ^ m_sketchWidget->getTraceFlag());
            foreach (ConnectorItem * equ, equi) {
                todo.removeOne(equ);
            }
            net->subnets.append(equi);
        }

        if (net->subnets.count() < 2) {
            // net is already routed
            continue;
        }

        net->pinsWithin = findPinsWithin(net->net);
        netList.nets << net;
	}

    qSort(netList.nets.begin(), netList.nets.end(), byPinsWithin);
    NetOrdering initialOrdering;
    int ix = 0;
    foreach (Net * net, netList.nets) {
        // id is the same as the order in netList
        initialOrdering.order << ix;
        net->id = ix++;
    }

	QUndoCommand * parentCommand = new QUndoCommand("Autoroute");
	new CleanUpWiresCommand(m_sketchWidget, CleanUpWiresCommand::UndoOnly, parentCommand);

	if (m_bothSidesNow) {
		emit wantBothVisible();
		ProcessEventBlocker::processEvents();
	}

	initUndo(parentCommand);

	ProcessEventBlocker::processEvents(); // to keep the app  from freezing
	if (m_cancelled || m_stopTracing) {
		restoreOriginalState(parentCommand);
		cleanUpNets(netList);
		return;
	}

    QSizeF gridSize(m_maxRect.width() / m_standardWireWidth, m_maxRect.height() / m_standardWireWidth);    
    QImage boardImage(qCeil(gridSize.width()), qCeil(gridSize.height()), QImage::Format_Mono);
    boardImage.fill(0);
    makeBoard(boardImage, m_keepoutGrid, gridSize);       
	ProcessEventBlocker::processEvents(); // to keep the app  from freezing
	if (m_cancelled || m_stopTracing) {
		restoreOriginalState(parentCommand);
		cleanUpNets(netList);
		return;
	}

    m_displayImage[0] = new QImage(boardImage.size(), QImage::Format_ARGB32);
    m_displayImage[0]->fill(0);
    m_displayImage[1] = new QImage(boardImage.size(), QImage::Format_ARGB32);
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
    bestScore.totalUnroutedCount = std::numeric_limits<int>::max();   // so that first run doesn't exit too soon
    Score currentScore;
    int run = 0;
	for (; run < m_maxCycles && run < allOrderings.count(); run++) {
		QString msg;
		if (run > 0) {
			msg = tr("best so far: %1 unrouted").arg(bestScore.totalUnroutedCount);
			if (m_sketchWidget->usesJumperItem()) {
				msg +=  tr("/%n vias", "", bestScore.totalViaCount);
			}
			emit setProgressMessage(msg);
		}
		emit setCycleMessage(tr("round %1 of:").arg(run + 1));
		ProcessEventBlocker::processEvents();
        currentScore.setOrdering(allOrderings.at(run));
		routeNets(netList, m_sketchWidget->usesJumperItem(), currentScore, bestScore, boardImage, gridSize, allOrderings);
		if (m_cancelled || currentScore.totalUnroutedCount == 0 || m_stopTracing) break;

		if (bestScore.ordering.order.count() == 0) {
            bestScore = currentScore;
        }
        else {
            if (currentScore.totalUnroutedCount < bestScore.totalUnroutedCount) {
                bestScore = currentScore;
            }
            else if (currentScore.totalUnroutedCount == bestScore.totalUnroutedCount && currentScore.totalViaCount < bestScore.totalViaCount) {
                bestScore = currentScore;
            }
        }
	}


	//DebugDialog::debug("done running");


	if (m_cancelled) {
		clearTracesAndJumpers();
		doCancel(parentCommand);
		return;
	}


	if (currentScore.totalUnroutedCount != 0) {
		if (run == 0) {
			// stop where we are
		}
		else {
			clearTracesAndJumpers();
			//m_sketchWidget->pasteHeart(bestOrdering->saved, true);
			ProcessEventBlocker::processEvents();
		}
	}

	cleanUpNets(netList);

	clearTracesAndJumpers();

	new CleanUpWiresCommand(m_sketchWidget, CleanUpWiresCommand::RedoOnly, parentCommand);

	m_sketchWidget->pushCommand(parentCommand);
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

void MazeRouter::updateProgress(int num, int denom) 
{
	emit setProgressValue((int) MaximumProgress * (m_currentProgressPart + (num / (double) denom)) / (double) m_maximumProgressPart);
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

bool MazeRouter::makeBoard(QImage & image, double keepoutGrid, const QSizeF gridSize) {
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
	painter.begin(&image);
	painter.setRenderHint(QPainter::Antialiasing, false);
    QRectF r(QPointF(0, 0), gridSize);
	renderer.render(&painter /*, r */);
	painter.end();

    // board should be white, borders should be black

#ifndef QT_NO_DEBUG
	image.save(FolderUtils::getUserDataStorePath("") + "/mazeMakeBoard.png");
#endif

    DRC::extendBorder(keepoutGrid, &image);

#ifndef QT_NO_DEBUG
	image.save(FolderUtils::getUserDataStorePath("") + "/mazeMakeBoard2.png");
#endif

    return true;
}

bool MazeRouter::makeMasters(QString & message) {
    QList<ViewLayer::ViewLayerSpec> layerSpecs;
    layerSpecs << ViewLayer::Bottom;
    if (m_bothSidesNow) layerSpecs << ViewLayer::Top;

    foreach (ViewLayer::ViewLayerSpec viewLayerSpec, layerSpecs) {  
        if (viewLayerSpec == ViewLayer::Top) emit wantTopVisible();
        else emit wantBottomVisible();

	    LayerList viewLayerIDs = ViewLayer::copperLayers(viewLayerSpec);
        viewLayerIDs.removeOne(ViewLayer::GroundPlane0);
        viewLayerIDs.removeOne(ViewLayer::GroundPlane1);
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
    }

    return true;
}

bool MazeRouter::routeNets(NetList & netList, bool makeJumper, Score & currentScore, Score & bestScore, QImage & boardImage, const QSizeF gridSize, QList<NetOrdering> & allOrderings)
{
    RouteThing routeThing;
    routeThing.r = QRectF(QPointF(0, 0), gridSize);
    routeThing.layerSpecs << ViewLayer::Bottom;
    if (m_bothSidesNow) routeThing.layerSpecs << ViewLayer::Top;
    routeThing.ikeepout = qCeil(m_keepoutGrid);

    bool result = true;

    initTraceDisplay();
    bool previousTraces = false;
    foreach (int netIndex, currentScore.ordering.order) {
        if (m_cancelled || m_stopTracing) {
            return false;
        }

        if (currentScore.traces.values(netIndex).count() > 0) {
            // traces were generated in a previous run
            foreach (Trace trace, currentScore.traces.values(netIndex)) {
                drawTrace(trace);
            }
            previousTraces = true;
            continue;
        }

        if (previousTraces) {
            updateDisplay(0);
            if (m_bothSidesNow) updateDisplay(1);
        }

        Net * net = netList.nets.at(netIndex);

        foreach (ConnectorItem * connectorItem, *(net->net)) {
            if (connectorItem->attachedTo()->layerKinChief()->id() == 12407630) {
                connectorItem->debugInfo("what");
                break;
            }
        }

        QList< QList<ConnectorItem *> > subnets;
        foreach (QList<ConnectorItem *> subnet, net->subnets) {
            QList<ConnectorItem *> copy(subnet);
            subnets.append(copy);
        }
        Nearest nearest;
        findNearestPair(subnets, nearest);
        if (subnets.at(nearest.i).count() < subnets.at(nearest.j).count()) {
            nearest.swap();
        }

        QPointF jp = nearest.jc->sceneAdjustedTerminalPoint(NULL) - m_board->sceneBoundingRect().topLeft();
        nearest.gridTarget = QPoint(jp.x() / m_standardWireWidth, jp.y() / m_standardWireWidth);
        Grid * grid = new Grid(boardImage.width(), boardImage.height(), m_bothSidesNow ? 2 : 1);
        std::priority_queue<GridPoint> pq;

        traceObstacles(currentScore.traces.values(), netIndex, grid, routeThing.ikeepout);
        static int oi = 0;

        foreach (ViewLayer::ViewLayerSpec viewLayerSpec, routeThing.layerSpecs) {  
            int z = viewLayerSpec == ViewLayer::Bottom ? 0 : 1;

            if (z == 1) emit wantTopVisible();
            else emit wantBottomVisible();

            QList<QDomElement> alsoNetElements;
            QDomDocument * masterDoc = m_masterDocs.value(viewLayerSpec);

            //QString temp = masterDoc->toString();
            DRC::splitNetPrep(masterDoc, *(net->net), m_keepoutMils, DRC::NotNet, z == 0 ? nearest.netElements0 : nearest.netElements1, alsoNetElements, z == 0 ? nearest.notNetElements0 : nearest.notNetElements1, true, true);
            foreach (QDomElement element, z == 0 ? nearest.netElements0 : nearest.netElements1) {
                element.setTagName("g");
            }
            QImage obstaclesImage = boardImage.copy();
            DRC::renderOne(masterDoc, &obstaclesImage, routeThing.r);


#ifndef QT_NO_DEBUG
            obstaclesImage.save(FolderUtils::getUserDataStorePath("") + QString("/obstacles%1.png").arg(oi));
#endif

            grid->init(0, 0, z, grid->x, grid->y, obstaclesImage, GridObstacle, false);
            //updateDisplay(grid, z);

            prepSourceAndTarget(masterDoc, grid, subnets, z, pq, z == 0 ? nearest.netElements0 : nearest.netElements1, z == 0 ? nearest.notNetElements0 : nearest.notNetElements1, nearest, routeThing.r);
        }

        nearest.unrouted = false;
        if (!routeOne(currentScore, bestScore, netIndex, grid, pq, nearest, allOrderings)) {
            result = false;
        }

        //updateDisplay(grid, 0);
        //if (m_bothSidesNow) updateDisplay(grid, 1);

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
            
            result = routeNext(routeThing, subnets, currentScore, bestScore, netIndex, grid, pq, nearest, allOrderings);
        }

        delete grid;
        // restore masterdoc
        foreach (QDomElement element, nearest.netElements0) {
            SvgFileSplitter::forceStrokeWidth(element, 2 * m_keepoutMils, "#000000", false, false);
        }
        foreach (QDomElement element, nearest.netElements1) {
            SvgFileSplitter::forceStrokeWidth(element, 2 * m_keepoutMils, "#000000", false, false);
        }

        if (result == false) break;
    }

    return result;
}

bool MazeRouter::routeOne(Score & currentScore, Score & bestScore, int netIndex, Grid * grid, std::priority_queue<GridPoint> & pq, Nearest & nearest, QList<NetOrdering> & allOrderings) {
    Trace newTrace;
    int viaCount;
    newTrace.gridPoints = route(grid, pq, nearest, viaCount);
    if (m_cancelled || m_stopTracing) {
        return false;
    }

    if (newTrace.gridPoints.count() == 0) {
        nearest.unrouted = true;
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

        currentScore.unroutedCount.insert(netIndex, currentScore.unroutedCount.value(netIndex) + 1);
        if (++currentScore.totalUnroutedCount > bestScore.totalUnroutedCount) {
            // no need to try to route this to the end
            return false;
        }
    }
    else {
        newTrace.netIndex = netIndex;
        newTrace.subnetIndexI = nearest.i;
        newTrace.subnetIndexJ = nearest.j;
        currentScore.traces.insert(netIndex, newTrace);
        currentScore.viaCount.insert(netIndex, currentScore.viaCount.value(netIndex, 0) + viaCount);
        currentScore.totalViaCount += viaCount;
        drawTrace(newTrace);
        updateDisplay(0);
        if (m_bothSidesNow) updateDisplay(1);
    }

    return true;
}

bool MazeRouter::routeNext(RouteThing & routeThing, QList< QList<ConnectorItem *> > & subnets, Score & currentScore, Score & bestScore, int netIndex, Grid * grid, std::priority_queue<GridPoint> & pq, Nearest & nearest, QList<NetOrdering> & allOrderings) 
{
    bool result = true;

    QList<ConnectorItem *> combined;
    if (nearest.unrouted) {
        if (nearest.i < nearest.j) {
            subnets.removeAt(nearest.j);
            combined = subnets.takeAt(nearest.i);
        }
        else {
            combined = subnets.takeAt(nearest.i);
            subnets.removeAt(nearest.j);
        }
    }
    else {
        combined.append(subnets.at(nearest.i));
        combined.append(subnets.at(nearest.j));
        if (nearest.i < nearest.j) {
            subnets.removeAt(nearest.j);
            subnets.removeAt(nearest.i);
        }
        else {
            subnets.removeAt(nearest.i);
            subnets.removeAt(nearest.j);
        }
    }
    subnets.prepend(combined);
    nearest.i = 0;
    nearest.j = -1;
    nearest.distance = std::numeric_limits<double>::max();
    findNearestPair(subnets, 0, combined, nearest);
    quint32 value = GridSource;
    if (subnets.at(nearest.i).count() < subnets.at(nearest.j).count()) {
        nearest.swap();
        value = GridTarget;
    }

    QPointF jp = nearest.jc->sceneAdjustedTerminalPoint(NULL) - m_board->sceneBoundingRect().topLeft();
    nearest.gridTarget = QPoint(jp.x() / m_standardWireWidth, jp.y() / m_standardWireWidth);

    pq = std::priority_queue<GridPoint>();

    foreach (ViewLayer::ViewLayerSpec viewLayerSpec, routeThing.layerSpecs) {  
        int z = viewLayerSpec == ViewLayer::Bottom ? 0 : 1;
        QDomDocument * masterDoc = m_masterDocs.value(viewLayerSpec);
        prepSourceAndTarget(masterDoc, grid, subnets, z, pq, z == 0 ? nearest.netElements0 : nearest.netElements1, z == 0 ? nearest.notNetElements0 : nearest.notNetElements1, nearest, routeThing.r);
    }

    // redraw traces from this net
    foreach (Trace trace, currentScore.traces.values(netIndex)) {
        if (value == GridTarget) {
            foreach (GridPoint gridPoint, trace.gridPoints) {
                grid->setAt(gridPoint.x, gridPoint.y, gridPoint.z, GridTarget);
            }
        }
        else {
            foreach (GridPoint gridPoint, trace.gridPoints) {
                grid->setAt(gridPoint.x, gridPoint.y, gridPoint.z, GridSource);
                int crossLayerCost = 0;
                if (nearest.jc->attachedToViewLayerID() == ViewLayer::Copper0 && gridPoint.z == 1) {
                    crossLayerCost = Layer1Cost;
                }
                else if (nearest.ic->attachedToViewLayerID() == ViewLayer::Copper1 && gridPoint.z == 0) {
                    crossLayerCost = Layer1Cost;
                }

                gridPoint.cost = initialCost(QPoint(gridPoint.x, gridPoint.y), nearest.gridTarget) + crossLayerCost;
                gridPoint.flags = 0;
                pq.push(gridPoint);                  
            }
        }
    }

    //updateDisplay(grid, 0);
    //if (m_bothSidesNow) updateDisplay(grid, 1);

    nearest.unrouted = false;
    result = routeOne(currentScore, bestScore, netIndex, grid, pq, nearest, allOrderings);

    return result;
}

bool MazeRouter::moveBack(Score & currentScore, int index, QList<NetOrdering> & allOrderings) {
    if (index == 0) return false;  // nowhere to move back to

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
            return true;
        }
        order.removeAt(i);
    }

    return false;
}

void MazeRouter::prepSourceAndTarget(QDomDocument * masterDoc, Grid * grid, QList< QList<ConnectorItem *> > & subnets, int z, std::priority_queue<GridPoint> & pq, QList<QDomElement> & netElements, QList<QDomElement> & notNetElements, Nearest & nearest, QRectF & r) 
{
    foreach (QDomElement element, notNetElements) {
        element.setTagName("g");
    }

    QList<ConnectorItem *> li = subnets.at(nearest.i);
    QList<QPoint> sourcePoints = renderSource(masterDoc, z, grid, netElements, li, GridSource, true, r, true);

    int crossLayerCost = 0;
    if (nearest.jc->attachedToViewLayerID() == ViewLayer::Copper0 && z == 1) {
        crossLayerCost = Layer1Cost;
    }
    else if (nearest.jc->attachedToViewLayerID() == ViewLayer::Copper1 && z == 0) {
        crossLayerCost = Layer1Cost;
    }

    foreach (QPoint p, sourcePoints) {
        GridPoint gridPoint(p, z);
        gridPoint.cost = initialCost(p, nearest.gridTarget) + crossLayerCost;
        pq.push(gridPoint);
    }

    QList<ConnectorItem *> lj = subnets.at(nearest.j);
    renderSource(masterDoc, z, grid, netElements, lj, GridTarget, true, r, false);

    //updateDisplay(grid, z);

    // restore masterdoc (except for netElements stroke-width)
    foreach (QDomElement element, netElements) {
        element.setTagName(element.attribute("former"));
        element.removeAttribute("net");
    }
    foreach (QDomElement element, notNetElements) {
        element.setTagName(element.attribute("former"));
        element.removeAttribute("net");
    }
}

void MazeRouter::findNearestPair(QList< QList<ConnectorItem *> > & subnets, Nearest & nearest) {
    nearest.distance = std::numeric_limits<double>::max();
    nearest.i = nearest.j = -1;
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
                double d = initialCost(ip, jp) / m_standardWireWidth;
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

QList<QPoint> MazeRouter::renderSource(QDomDocument * masterDoc, int z, Grid * grid, QList<QDomElement> & netElements, QList<ConnectorItem *> & subnet, quint32 value, bool clearElements, const QRectF & r, bool collectPoints) {
    if (clearElements) {
        foreach (QDomElement element, netElements) {
            element.setTagName("g");
        }
    }

    QImage image(grid->x, grid->y, QImage::Format_Mono);
    image.fill(0xffffffff);
    QMultiHash<QString, QString> partIDs;
    QRectF itemsBoundingRect;
    foreach (ConnectorItem * connectorItem, subnet) {
        ItemBase * itemBase = connectorItem->attachedTo();
        SvgIdLayer * svgIdLayer = connectorItem->connector()->fullPinInfo(itemBase->viewIdentifier(), itemBase->viewLayerID());
        partIDs.insert(QString::number(itemBase->id()), svgIdLayer->m_svgId);
        itemsBoundingRect |= connectorItem->sceneBoundingRect();
    }
    foreach (QDomElement element, netElements) {
        if (idsMatch(element, partIDs)) {
            element.setTagName(element.attribute("former"));
        }
    }

    QRectF boardRect = m_board->sceneBoundingRect();
    int x1 = qFloor((itemsBoundingRect.left() - boardRect.left()) / m_standardWireWidth);
    int y1 = qFloor((itemsBoundingRect.top() - boardRect.top()) / m_standardWireWidth);
    int x2 = qCeil((itemsBoundingRect.right() - boardRect.left()) / m_standardWireWidth);
    int y2 = qCeil((itemsBoundingRect.bottom() - boardRect.top()) / m_standardWireWidth);

    DRC::renderOne(masterDoc, &image, r);
#ifndef QT_NO_DEBUG
    //static int rsi = 0;
	//image.save(FolderUtils::getUserDataStorePath("") + QString("/rendersource%1.png").arg(rsi++));
#endif
    return grid->init(x1, y1, z, x2 - x1, y2 - y1, image, value, collectPoints);
}

QList<GridPoint> MazeRouter::route(Grid * grid, std::priority_queue<GridPoint> & pq, Nearest & nearest, int & viaCount)
{
    bool result = false;
    GridPoint target;
    while (!pq.empty()) {
        GridPoint gp = pq.top();
        pq.pop();

        if (gp.flags & GridPointDone) {
            result = true;
            target = gp;
            break;
        }

        expand(gp, grid, pq, nearest);
        if (m_cancelled || m_stopTracing) {
            break;
        }
    }


    if (!result) {
        //updateDisplay(grid, 0);
        //if (m_bothSidesNow) updateDisplay(grid, 1);
        QList<GridPoint> points;
        return points;
    }

    QList<GridPoint> points = traceBack(target, grid, viaCount);
    //updateDisplay(grid, 0);
    //if (m_bothSidesNow) updateDisplay(grid, 1);
    clearExpansion(grid);  

    return points;
}

QList<GridPoint> MazeRouter::traceBack(GridPoint & gridPoint, Grid * grid, int & viaCount) {
    viaCount = 0;
    QList<GridPoint> points;
    points << gridPoint;
    while (true) {
        quint32 val = grid->at(gridPoint.x, gridPoint.y, gridPoint.z);
        if (val == GridSource) {
            break;
        }

        GridPoint next = traceBackOne(gridPoint, grid, -1, 0, 0, val);
        if (next.cost != GridIllegal) ;
        else {
            next = traceBackOne(gridPoint, grid, 1, 0, 0, val);
            if (next.cost != GridIllegal) ;
            else {
                next = traceBackOne(gridPoint, grid, 0, -1, 0, val);
                if (next.cost != GridIllegal) ;
                else {
                    next = traceBackOne(gridPoint, grid, 0, 1, 0, val);
                    if (next.cost != GridIllegal) ;
                    else {
                        next = traceBackOne(gridPoint, grid, 0, 0, -1, val);
                        if (next.cost != GridIllegal) ;
                        else {
                            next = traceBackOne(gridPoint, grid, 0, 0, 1, val);
                            if (next.cost != GridIllegal) ;
                            else {
                                // traceback failed--is this possible?
                                points.clear();
                                break;      
                            }
                        }
                    }
                }
            }
        }

        points << next;
        if (next.z != gridPoint.z) viaCount++;
        gridPoint = next;
    }

    return points;
}

GridPoint MazeRouter::traceBackOne(GridPoint & gridPoint, Grid * grid, int dx, int dy, int dz, quint32 val) {
    GridPoint next;
    next.cost = GridIllegal;

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

    quint32 nextval = grid->at(next.x, next.y, next.z);
    switch (nextval) {
        case GridObstacle:
            return next;
        case GridTarget:
            return next;
        case GridSource:
            next.cost = GridSource;
            return next;
        case 0:
            // never got involved
            return next;
        default:
            if (nextval < val) {
                next.cost = nextval;
            }
            return next;
    }

}

bool MazeRouter::expand(GridPoint & gridPoint, Grid * grid, std::priority_queue<GridPoint> & pq, Nearest & nearest)
{
    QList<GridPoint> nexts;
    nexts << expandOne(gridPoint, grid, -1, 0, 0, false) <<
                expandOne(gridPoint, grid, 1, 0, 0, false) <<
                expandOne(gridPoint, grid, 0, -1, 0, false) <<
                expandOne(gridPoint, grid, 0, 1, 0, false) <<
                expandOne(gridPoint, grid, 0, 0, -1, true) <<
                expandOne(gridPoint, grid, 0, 0, 1, true);
    foreach (GridPoint next, nexts) {
        if (next.cost == GridIllegal || next.cost == GridSource || next.cost == GridObstacle) continue;
                
        next.cost += aStarCost(QPoint(next.x, next.y), nearest.gridTarget);
        pq.push(next);
    }

    return false;
}

GridPoint MazeRouter::expandOne(GridPoint & gridPoint, Grid * grid, int dx, int dy, int dz, bool crossLayer) {
    GridPoint next;
    next.cost = GridIllegal;

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

    quint32 nextval = grid->at(next.x, next.y, next.z);
    switch (nextval) {
        case GridObstacle:
        case GridSource:
            return next;
        case GridTarget:
            next.flags |= GridPointDone;
            break;
        case 0:
            // got a new point
            break;
        default:
            // already been here
            return next;
    }

    if (crossLayer && !viaWillFit(next, grid)) {
        return next;
    }

    quint32 cost = grid->at(gridPoint.x, gridPoint.y, gridPoint.z);
    if (cost == GridSource) {
        cost = 0;
    }
    if (crossLayer) {
        cost += ViaCost;
    }
    cost++;

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

    if (nextval != GridTarget) {
        grid->setAt(next.x, next.y, next.z, cost);
    }

    next.cost = cost;
    //updateDisplay(next);

    return next;
}

bool MazeRouter::viaWillFit(GridPoint & gridPoint, Grid * grid) {
    int z = gridPoint.z == 1 ? 0 : 1;
    for (int y = -m_halfGridViaSize; y <= m_halfGridViaSize; y++) {
        for (int x = -m_halfGridViaSize; x <= m_halfGridViaSize; x++) {
            switch(grid->at(gridPoint.x + x, gridPoint.y + y, gridPoint.z)) {
                case GridObstacle:
                case GridSource:
                case GridTarget:
                    return false;
                default:
                    break;
            }
            switch(grid->at(gridPoint.x + x, gridPoint.y + y, z)) {
                case GridObstacle:
                case GridSource:
                case GridTarget:
                    return false;
                default:
                    break;
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
        //m_displayItem[iz]->setPos(iz == 1 ? m_board->sceneBoundingRect().topLeft() : m_board->sceneBoundingRect().topRight());
        m_displayItem[iz]->setPos(m_board->sceneBoundingRect().topLeft());
        m_sketchWidget->scene()->addItem(m_displayItem[iz]);
        m_displayItem[iz]->setZValue(5000);
        //m_displayItem[iz]->setZValue(m_sketchWidget->viewLayers().value(iz == 0 ? ViewLayer::Copper0 : ViewLayer::Copper1)->nextZ());
        m_displayItem[iz]->setScale(m_board->sceneBoundingRect().width() / m_displayImage[iz]->width());
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
            uint color;
            quint32 val = grid->at(x, y, iz);
            switch (val) {
                case GridObstacle:
                    color = 0xff000000;
                    break;
                case GridSource:
                    color = 0xff00ff00;
                    break;
                case GridTarget:
                    color = 0xffffff00;
                    break;
                case 0:
                    continue;
                default:
                    color = 0xffff0000;
                    break;
            }

            m_displayImage[iz]->setPixel(x, y, color);
        }
    }

    updateDisplay(iz);
}

void MazeRouter::updateDisplay(GridPoint & gridPoint) {
    static int counter = 0;
    if (counter++ % 20 == 0) {
        m_displayImage[gridPoint.z]->setPixel(gridPoint.x, gridPoint.y, 0xffff0000);
        updateDisplay(gridPoint.z);
    }
}

void MazeRouter::clearExpansion(Grid * grid) {
    // TODO: keep a list of points expansion points instead?

    for (int z = 0; z < grid->z; z++) {
        for (int y = 0; y < grid->y; y++) {
            for (int x = 0; x < grid->x; x++) {
                switch (grid->at(x, y, z)) {
                    case GridObstacle:
                    case 0:
                        break;
                    case GridTarget:
                    case GridSource:
                    default:
                        grid->setAt(x, y, z, 0);
                        break;
                }
            }
        }
    }
}

void MazeRouter::initTraceDisplay() {
    m_displayImage[0]->fill(0);
    m_displayImage[1]->fill(0);
}

void MazeRouter::drawTrace(Trace & trace) {
    int lastz = trace.gridPoints.at(0).z;
    foreach (GridPoint gridPoint, trace.gridPoints) {
        if (gridPoint.z != lastz) {
            for (int y = -m_halfGridViaSize; y <= m_halfGridViaSize; y++) {
                for (int x = -m_halfGridViaSize; x <= m_halfGridViaSize; x++) {
                    m_displayImage[0]->setPixel(x + gridPoint.x, y + gridPoint.y, 0x80ff0000);
                    m_displayImage[1]->setPixel(x + gridPoint.x, y + gridPoint.y, 0x80ff0000);
                }
            }
        }
        else {
            uint color = (gridPoint.z == 0) ? 0x80F28A80 : 0x808af280;
            m_displayImage[gridPoint.z]->setPixel(gridPoint.x, gridPoint.y, color);
        }
        lastz = gridPoint.z;
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
                        grid->setAt(gridPoint.x + x, gridPoint.y + y, 0, GridObstacle);
                        grid->setAt(gridPoint.x + x, gridPoint.y + y, 1, GridObstacle);
                    }
                }
            }
            else {
                for (int y = -ikeepout; y <= ikeepout; y++) {
                    for (int x = -ikeepout; x <= ikeepout; x++) {
                        grid->setAt(gridPoint.x + x, gridPoint.y + y, gridPoint.z, GridObstacle);
                    }
                }
            }
            lastZ = gridPoint.z;
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
