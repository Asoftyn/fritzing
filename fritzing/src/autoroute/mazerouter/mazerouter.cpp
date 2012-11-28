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

// TODO:
//
//      schematic view
//
//      net reordering (how to decide the best so far?)
//
//      keepout dialog
//   
//      jumperitem
//
//      feedback
//
//      rip-up-and-reroute
//
//      stop-now and best-ordering?  how is best ordering determined?
//
//      dynamic cost function based on distance to any target point
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

static const uint Layer1Cost = 10;
static const uint CrossLayerCost = 20;
static const uint ViaCost = 150;

static const uchar GridPointDone = 1;
static const uchar GridPointVia = 2;


////////////////////////////////////////////////////////////////////

QString getPartID(const QDomElement & element) {
    QString partID = element.attribute("partID");
    if (!partID.isEmpty()) return partID;

    QDomNode parent = element.parentNode();
    if (parent.isNull()) return "";

    return getPartID(parent.toElement());
}

bool byPinsWithin(Net * n1, Net * n2)
{
	if (n1->pinsWithin < n2->pinsWithin) return true;
    if (n1->pinsWithin > n2->pinsWithin) return false;

    return n1->net->count() <= n2->net->count();
}

inline double manhattanDistance(QPointF p1, QPointF p2) {
    //return qAbs(p1.x() - p2.x()) + qAbs(p1.y() - p2.y());
    return GraphicsUtils::distanceSqd(p1, p2);
}

inline int manhattanDistance(QPoint p1, QPoint p2) {
    //return qAbs(p1.x() - p2.x()) * qAbs(p1.y() - p2.y());
    return (int) GraphicsUtils::distanceSqd(p1, p2);
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

    data = (quint32 *) calloc(x * y * z, 4);
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

    double ringThickness, holeSize;
	m_sketchWidget->getViaSize(ringThickness, holeSize);
	m_gridViaSize = qCeil((ringThickness + ringThickness + holeSize + m_keepoutPixels + m_keepoutPixels) / m_standardWireWidth);

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
		cleanUpNets();
		return;
	}

	// will list connectors on both sides separately
	routingStatus.m_netCount = m_allPartConnectorItems.count();

	QVector<int> netCounters(m_allPartConnectorItems.count());
    NetOrdering * bestOrdering = new NetOrdering();   
	for (int i = 0; i < m_allPartConnectorItems.count(); i++) {
		netCounters[i] = (m_allPartConnectorItems[i]->count() - 1) * 2;			// since we use two connectors at a time on a net
        Net * net = new Net;
        net->net = m_allPartConnectorItems[i];
        net->id = i;
        net->pinsWithin = findPinsWithin(net->net);
        bestOrdering->nets << net;
	}

    qSort(bestOrdering->nets.begin(), bestOrdering->nets.end(), byPinsWithin);
	computeMD5(bestOrdering);

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
		cleanUpNets();
		return;
	}

    QSizeF gridSize(m_maxRect.width() / m_standardWireWidth, m_maxRect.height() / m_standardWireWidth);    
    QImage boardImage(qCeil(gridSize.width()), qCeil(gridSize.height()), QImage::Format_Mono);
    boardImage.fill(0);
    makeBoard(boardImage, m_keepoutPixels / m_standardWireWidth, gridSize);       // both values are in pixel units
	ProcessEventBlocker::processEvents(); // to keep the app  from freezing
	if (m_cancelled || m_stopTracing) {
		restoreOriginalState(parentCommand);
		cleanUpNets();
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
		cleanUpNets();
		return;
	}

	bool allDone = false;
	QList<NetOrdering *> orderings;
	orderings.append(bestOrdering);

	bestOrdering->unroutedCount = m_allPartConnectorItems.count() + 1;	// so runEdges doesn't bail out the first time through

	int orderingIndex = 0;
	for (int run = 0; run < m_maxCycles && orderingIndex < orderings.count(); run++) {
		NetOrdering * currentOrdering = orderings.at(orderingIndex++);
		QString score;
		if (run > 0) {
			score = tr("best so far: %1 unrouted").arg(bestOrdering->unroutedCount);
			if (m_sketchWidget->usesJumperItem()) {
				score += tr("/%n jumpers", "", bestOrdering->jumperCount) + tr("/%n vias", "", bestOrdering->totalViaCount);
			}
			emit setProgressMessage(score);
		}
		emit setCycleMessage(tr("round %1 of:").arg(run + 1));
		ProcessEventBlocker::processEvents();

		allDone = routeNets(currentOrdering, netCounters, routingStatus, m_sketchWidget->usesJumperItem(), bestOrdering, boardImage, gridSize);
		if (m_cancelled || allDone || m_stopTracing) break;

		ProcessEventBlocker::processEvents();
		reorder(orderings, currentOrdering, bestOrdering);

		// TODO: only delete the edges that have been reordered
		clearTracesAndJumpers();
		ProcessEventBlocker::processEvents();
	}


	//DebugDialog::debug("done running");


	if (m_cancelled) {
		clearTracesAndJumpers();
		doCancel(parentCommand);
		foreach (NetOrdering * ordering, orderings) delete ordering;
		orderings.clear();
		return;
	}


	QMultiHash<TraceWire *, long> splitDNA;
	if (!allDone) {
		if (orderingIndex == 0) {
			// stop where we are
			foreach (TraceWire * fromWire, m_splitDNA.keys()) {
				foreach (TraceWire * toWire, m_splitDNA.values(fromWire)) {
					splitDNA.insert(fromWire, toWire->id());
				}
			}
		}
		else {
			clearTracesAndJumpers();
			m_sketchWidget->pasteHeart(bestOrdering->saved, true);
			splitDNA = bestOrdering->splitDNA;
			ProcessEventBlocker::processEvents();
		}
	}
	else {
		splitDNA = bestOrdering->splitDNA;
	}

	foreach (NetOrdering * ordering, orderings) delete ordering;
	orderings.clear();

	cleanUpNets();

	addToUndo(splitDNA, parentCommand);

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

void MazeRouter::computeMD5(NetOrdering * ordering) {
	QByteArray buffer;
	foreach (Net * net, ordering->nets) {
		QByteArray b((const char *) &net->id, sizeof(int));
		buffer.append(b);
	}

	ordering->md5sum = QCryptographicHash::hash(buffer, QCryptographicHash::Md5);
}

bool MazeRouter::reorder(QList<NetOrdering *> & orderings, NetOrdering * currentOrdering, NetOrdering * & bestOrdering) {
    return false;
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

bool MazeRouter::makeBoard(QImage & image, double keepout, const QSizeF gridSize) {
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
	renderer.render(&painter, r);
	painter.end();

    // board should be white, borders should be black

#ifndef QT_NO_DEBUG
	image.save(FolderUtils::getUserDataStorePath("") + "/mazeMakeBoard.png");
#endif

    DRC::extendBorder(keepout / m_standardWireWidth, &image);

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

bool MazeRouter::routeNets(NetOrdering * ordering, QVector<int> & netCounters, RoutingStatus & routingStatus, bool makeJumper, NetOrdering * bestOrdering, QImage & boardImage, const QSizeF gridSize)
{
    QRectF r(QPointF(0, 0), gridSize);
    QList<ViewLayer::ViewLayerSpec> layerSpecs;
    layerSpecs << ViewLayer::Bottom;
    if (m_bothSidesNow) layerSpecs << ViewLayer::Top;

    foreach (Net * net, ordering->nets) {
        QList< QList<ConnectorItem *> > subnets;
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
            subnets.append(equi);
        }

        Nearest nearest;
        findNearestPair(subnets, nearest);
        if (subnets.at(nearest.i).count() > subnets.at(nearest.j).count()) {
            nearest.swap();
        }

        QPointF jp = nearest.jc->sceneAdjustedTerminalPoint(NULL) - m_board->sceneBoundingRect().topLeft();
        nearest.gridTarget = QPoint(jp.x() / m_standardWireWidth, jp.y() / m_standardWireWidth);
        Grid * grid = new Grid(boardImage.width(), boardImage.height(), m_bothSidesNow ? 2 : 1);
        std::priority_queue<GridPoint> pq;

        foreach (ViewLayer::ViewLayerSpec viewLayerSpec, layerSpecs) {  
            if (viewLayerSpec == ViewLayer::Top) emit wantTopVisible();
            else emit wantBottomVisible();

	        LayerList viewLayerIDs = ViewLayer::copperLayers(viewLayerSpec);
            viewLayerIDs.removeOne(ViewLayer::GroundPlane0);
            viewLayerIDs.removeOne(ViewLayer::GroundPlane1);

            QList<QDomElement> netElements;
            QList<QDomElement> alsoNetElements;
            QList<QDomElement> notNetElements;
            QDomDocument * masterDoc = m_masterDocs.value(viewLayerSpec);
            DRC::splitNetPrep(masterDoc, *(net->net), m_keepoutMils, DRC::NotNet, netElements, alsoNetElements, notNetElements, true, true);
            foreach (QDomElement element, netElements) {
                element.setTagName("g");
            }
            QImage obstaclesImage = boardImage.copy();
            DRC::renderOne(masterDoc, &obstaclesImage, r);

            int z = viewLayerSpec == ViewLayer::Bottom ? 0 : 1;

#ifndef QT_NO_DEBUG
            obstaclesImage.save(FolderUtils::getUserDataStorePath("") + QString("/obstacles%1.png").arg(z));
#endif

            grid->init(0, 0, z, grid->x, grid->y, obstaclesImage, GridObstacle, false);

            updateDisplay(grid, z);

            foreach (QDomElement element, notNetElements) {
                element.setTagName("g");
            }

            QList<ConnectorItem *> li = subnets.at(nearest.i);
            QList<QPoint> sourcePoints = renderSource(masterDoc, viewLayerSpec, grid, netElements, li, GridSource, false, r, true);

            int crossLayerCost = 0;
            if (nearest.ic->attachedToViewLayerID() == ViewLayer::Copper0 && viewLayerSpec == ViewLayer::Top) {
                crossLayerCost = Layer1Cost;
            }
            else if (nearest.ic->attachedToViewLayerID() == ViewLayer::Copper1 && viewLayerSpec == ViewLayer::Bottom) {
                crossLayerCost = Layer1Cost;
            }

            foreach (QPoint p, sourcePoints) {
                GridPoint gridPoint(p, z);
                gridPoint.cost = manhattanDistance(p, nearest.gridTarget) + crossLayerCost;
                pq.push(gridPoint);
            }

            QList<ConnectorItem *> lj = subnets.at(nearest.j);
            renderSource(masterDoc, viewLayerSpec, grid, netElements, lj, GridTarget, true, r, false);

            updateDisplay(grid, z);

            // restore masterdoc
            foreach (QDomElement element, netElements) {
                element.setTagName(element.attribute("former"));
                element.removeAttribute("net");
                SvgFileSplitter::forceStrokeWidth(element, 2 * m_keepoutMils, "#000000", false, false);
            }
            foreach (QDomElement element, notNetElements) {
                element.setTagName(element.attribute("former"));
                element.removeAttribute("notnet");
            }
        }

        bool result = route(grid, pq, nearest);

        // for each remaining connector in net
        //      figure out nearest to src
        //      render dest and convert (use rect)
        //      route the maze using A* (include hack for number of free neighbors)
        //      backtrace and remove maze flood
    }


    return true;
}

void MazeRouter::findNearestPair(QList< QList<ConnectorItem *> > & subnets, Nearest & nearest) {
    nearest.distance = std::numeric_limits<double>::max();
    nearest.i = nearest.j = -1;
    for (int i = 0; i < subnets.count() - 1; i++) {
        QList<ConnectorItem *> inet = subnets.at(i);
        findNearestPair(subnets, i, inet, nearest);
    }
}

void MazeRouter::findNearestPair(QList< QList<ConnectorItem *> > & subnets, int i, QList<ConnectorItem *> & inet, Nearest & nearest) {
    for (int j = i + 1; j < subnets.count(); j++) {
        QList<ConnectorItem *> jnet = subnets.at(j);
        foreach (ConnectorItem * ic, inet) {
            QPointF ip = ic->sceneAdjustedTerminalPoint(NULL);
            ConnectorItem * icc = ic->getCrossLayerConnectorItem();
            foreach (ConnectorItem * jc, jnet) {
                ConnectorItem * jcc = jc->getCrossLayerConnectorItem();
                if (jc == ic || jcc == ic) continue;

                QPointF jp = jc->sceneAdjustedTerminalPoint(NULL);
                double d = manhattanDistance(ip, jp) / m_standardWireWidth;
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
                    nearest.i = i;
                    nearest.j = j;
                    nearest.ic = ic;
                    nearest.jc = jc;
                }
            }
        }
    }
}

QList<QPoint> MazeRouter::renderSource(QDomDocument * masterDoc, ViewLayer::ViewLayerSpec viewLayerSpec, Grid * grid, QList<QDomElement> & netElements, QList<ConnectorItem *> & subnet, quint32 value, bool clearElements, const QRectF & r, bool collectPoints) {
    if (clearElements) {
        foreach (QDomElement element, netElements) {
            element.setTagName("g");
        }
    }

    QImage image(grid->x, grid->y, QImage::Format_Mono);
    image.fill(0xffffffff);
    QStringList partIDs;
    QRectF itemsBoundingRect;
    foreach (ConnectorItem * connectorItem, subnet) {
        partIDs.append(QString::number(connectorItem->attachedToID()));
        itemsBoundingRect |= connectorItem->sceneBoundingRect();
    }
    foreach (QDomElement element, netElements) {
        QString partID = getPartID(element);
        if (partIDs.contains(partID)) {
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
    static int rsi = 0;
	image.save(FolderUtils::getUserDataStorePath("") + QString("/rendersource%1.png").arg(rsi++));
#endif
    return grid->init(x1, y1, viewLayerSpec == ViewLayer::Bottom ? 0 : 1, x2 - x1, y2 - y1, image, value, collectPoints);
}

bool MazeRouter::route(Grid * grid, std::priority_queue<GridPoint> & pq, Nearest & nearest)
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
    }

    if (!result) {
        return false;
    }

    QList<GridPoint> points = traceBack(target, grid);
    sourceToTarget(grid);
    foreach (GridPoint gridPoint, points) {
        grid->setAt(gridPoint.x, gridPoint.y, gridPoint.z, GridTarget);
    }

    updateDisplay(grid, 0);
    if (m_bothSidesNow) {
        updateDisplay(grid, 1);
    }

    return points.count() > 0;
}

QList<GridPoint> MazeRouter::traceBack(GridPoint & gridPoint, Grid * grid) {
    QList<GridPoint> points;
    points << gridPoint;
    while (true) {
        quint32 val = grid->at(gridPoint.x, gridPoint.y, gridPoint.z);
        if (val == GridSource) {
            break;
        }

        GridPoint next = traceBackOne(gridPoint, grid, -1, 0, 0, val);
        if (next.cost != GridIllegal) {
            points << next;
            gridPoint = next;
            continue;
        }
        next = traceBackOne(gridPoint, grid, 1, 0, 0, val);
        if (next.cost != GridIllegal) {
            points << next;
            gridPoint = next;
            continue;
        }
        next = traceBackOne(gridPoint, grid, 0, -1, 0, val);
        if (next.cost != GridIllegal) {
            points << next;
            gridPoint = next;
            continue;
        }
        next = traceBackOne(gridPoint, grid, 0, 1, 0, val);
        if (next.cost != GridIllegal) {
            points << next;
            gridPoint = next;
            continue;
        }
        next = traceBackOne(gridPoint, grid, 0, 0, -1, val);
        if (next.cost != GridIllegal) {
            points << next;
            gridPoint = next;
            continue;
        }
        next = traceBackOne(gridPoint, grid, 0, 0, 1, val);
        if (next.cost != GridIllegal) {
            points << next;
            gridPoint = next;
            continue;
        }

        // traceback failed--is this possible?
        points.clear();
        break;      
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
        switch (next.cost) {
            case GridIllegal:
            case GridSource:
            case GridObstacle:
                continue;
            default:
                next.cost += manhattanDistance(QPoint(next.x, next.y), nearest.gridTarget);
                pq.push(next);
        } 
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

    next.cost = grid->at(gridPoint.x, gridPoint.y, gridPoint.z);
    if (next.cost == GridSource) {
        next.cost = 0;
    }
    if (crossLayer) {
        next.cost += ViaCost;
        next.flags |= GridPointVia;
    }
    next.cost++;

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
        grid->setAt(next.x, next.y, next.z, next.cost);
    }

    updateDisplay(next);

    return next;
}

bool MazeRouter::viaWillFit(GridPoint & gridPoint, Grid * grid) {
    int z = gridPoint.z == 1 ? 0 : 1;
    for (int y = 0; y < m_gridViaSize; y++) {
        for (int x = 0; x < m_gridViaSize; x++) {
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
        m_displayItem[iz]->setPos(iz == 0 ? m_board->sceneBoundingRect().topLeft() : m_board->sceneBoundingRect().bottomLeft());
        m_sketchWidget->scene()->addItem(m_displayItem[iz]);
        m_displayItem[iz]->setZValue(5000);
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
            qint32 val = grid->at(x, y, iz);
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
                    color = val | 0xff000000;
                    break;
            }

            m_displayImage[iz]->setPixel(x, y, color);
        }
    }

    updateDisplay(iz);
}

void MazeRouter::updateDisplay(GridPoint & gridPoint) {
    m_displayImage[gridPoint.z]->setPixel(gridPoint.x, gridPoint.y, 0xff000000 | gridPoint.cost);
    updateDisplay(gridPoint.z);
}

void MazeRouter::sourceToTarget(Grid * grid) {
    for (int z = 0; z < grid->z; z++) {
        for (int y = 0; y < grid->y; y++) {
            for (int x = 0; x < grid->x; x++) {
                switch (grid->at(x, y, z)) {
                    case GridObstacle:
                    case GridTarget:
                        break;
                    case GridSource:
                        grid->setAt(x, y, z, GridTarget);
                        break;
                    default:
                        grid->setAt(x, y, z, 0);
                        break;
                }
            }
        }
    }
}
