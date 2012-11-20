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
//      holes
//
//      keepout dialog
//   
//      jumperitem
//
//      via search
//
//      datastructure for grid
//
//      feedback
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

#include <qmath.h>
#include <limits>
#include <QApplication>
#include <QMessageBox> 
#include <QSettings>
#include <QCryptographicHash>
#include <QSvgRenderer>

#include <vector>
using namespace std;

static const int MaximumProgress = 1000;
static int TileStandardWireWidth = 0;
static int TileHalfStandardWireWidth = 0;
static double StandardWireWidth = 0;
static double HalfStandardWireWidth = 0;

static QString CancelledMessage;

static const int DefaultMaxCycles = 10;

////////////////////////////////////////////////////////////////////

bool byPinsWithin(Net * n1, Net * n2)
{
	if (n1->pinsWithin < n2->pinsWithin) return true;
    if (n1->pinsWithin > n2->pinsWithin) return false;

    return n1->net->count() <= n2->net->count();
}

////////////////////////////////////////////////////////////////////

MazeRouter::MazeRouter(PCBSketchWidget * sketchWidget, ItemBase * board, bool adjustIf) : Autorouter(sketchWidget)
{
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
	setUpWidths(m_sketchWidget->getAutorouterTraceWidth());

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
	m_keepout = m_sketchWidget->getKeepout();			// 15 mils space

	emit setMaximumProgress(MaximumProgress);
	emit setProgressMessage("");
	emit setCycleMessage("round 1 of:");
	emit setCycleCount(m_maxCycles);

	RoutingStatus routingStatus;
	routingStatus.zero();

	m_sketchWidget->ensureTraceLayersVisible();

	QHash<ConnectorItem *, int> indexer;
	m_sketchWidget->collectAllNets(indexer, m_allPartConnectorItems, false, m_bothSidesNow);

    removeOffBoard(isPCBType);

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


    int gridWidth = qCeil(m_maxRect.width() / StandardWireWidth);
    int gridHeight = qCeil(m_maxRect.height() / StandardWireWidth);
    std::vector< std::vector<qint32> > grid(gridHeight, std::vector<qint32>(gridWidth, 0 ));
    
    QImage boardImage(gridWidth, gridHeight, QImage::Format_Mono);
    boardImage.fill(0);
    makeBoard(boardImage, m_keepout / StandardWireWidth);       // both values are in pixel units
	ProcessEventBlocker::processEvents(); // to keep the app  from freezing
	if (m_cancelled || m_stopTracing) {
		restoreOriginalState(parentCommand);
		cleanUpNets();
		return;
	}

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

		//allDone = routeNets(currentOrdering, netCounters, routingStatus, m_sketchWidget->usesJumperItem(), bestOrdering);
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





void MazeRouter::setUpWidths(double width)
{
	StandardWireWidth = width;
	HalfStandardWireWidth = StandardWireWidth / 2;										
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

bool MazeRouter::makeBoard(QImage & image, double keepout) {
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
	renderer.render(&painter);
	painter.end();

    // board should be white, borders should be black

    QImage copy = image.copy();

#ifndef QT_NO_DEBUG
	image.save(FolderUtils::getUserDataStorePath("") + "/mazeMakeBoard.png");
#endif

    int h = image.height();
    int w = image.width();
    for (int x = 0; x < w; x++) {
        for (int k = 0; k < keepout; k++) {
            image.setPixel(x, k, 0);
            image.setPixel(x, h - k - 1, 0);
        }
    }
    int ikeepout = qCeil(keepout);
    for (int y = 0; y < h; y++) {
        for (int k = 0; k < keepout; k++) {
            image.setPixel(k, y, 0);
            image.setPixel(w - k - 1, y, 0);
        }
        for (int x = 0; x < w; x++) {
            if (copy.pixel(x, y) != 0xff000000) {
                continue;
            }

            for (int dy = y - ikeepout; dy <= y + ikeepout; dy++) {
                if (dy < 0) continue;
                if (dy >= h) continue;
                for (int dx = x - ikeepout; dx <= x + ikeepout; dx++) {
                    if (dx < 0) continue;
                    if (dx >= w) continue;

                    // extend border by keepout
                    image.setPixel(dx, dy, 0);
                }
            }
        }
    }

#ifndef QT_NO_DEBUG
	image.save(FolderUtils::getUserDataStorePath("") + "/mazeMakeBoard2.png");
#endif

	viewLayerIDs.clear();
    viewLayerIDs << ViewLayer::Copper0 << ViewLayer::Copper1;
    QRectF copperImageRect;
	QString master = m_sketchWidget->renderToSVG(GraphicsUtils::SVGDPI, viewLayerIDs, true, copperImageRect, m_board, GraphicsUtils::StandardFritzingDPI, false, false, empty);
    if (!master.contains(FSvgRenderer::NonConnectorName)) {
        // no holes found
        return true;
    }
       
    QString errorStr;
	int errorLine;
	int errorColumn;	
    QDomDocument doc;
    doc.setContent(master, &errorStr, &errorLine, &errorColumn);

    QSet<QString> holeIDs;

    foreach (QGraphicsItem * item, m_sketchWidget->scene()->collidingItems(m_board)) {
        ItemBase * itemBase = dynamic_cast<ItemBase *>(item);
        if (itemBase == NULL) continue;
        if (itemBase->itemType() != ModelPart::Hole) continue;

        holeIDs.insert(QString::number(itemBase->id()));
    }

    if (holeIDs.count() == 0) {
        // shouldn't happen
        return true;
    }

    QList<QDomElement> todo;
    todo << doc.documentElement();
    bool firstTime = true;
    while (!todo.isEmpty()) {
        QDomElement element = todo.takeFirst();
        QDomElement child = element.firstChildElement();
        while (!child.isNull()) {
            todo << child;
            child = child.nextSiblingElement();
        }
        if (firstTime) {
            // don't include the root <svg> element
            firstTime = false;
            continue;
        }

        QString partID = element.attribute("partID");
        if (!partID.isEmpty()) {
            if (holeIDs.contains(partID)) {
                // make sure all sub elements of Hole part will be added to the board image
                QList<QDomElement> subtodo;
                subtodo << element;
                while (!subtodo.isEmpty()) {
                    QDomElement subelement = subtodo.takeFirst();
                    subelement.setAttribute("id", FSvgRenderer::NonConnectorName);
                    QDomElement child = subelement.firstChildElement();
                    while (!child.isNull()) {
                        subtodo << child;
                        child = child.nextSiblingElement();
                    }
                }
            }
        }

        if (element.tagName() == "g") continue;

        if (element.attribute("id").contains(FSvgRenderer::NonConnectorName)) {
            element.setAttribute("fill", "black");
            element.setAttribute("stroke", "black");
            QString sw = element.attribute("stroke-width", "0");
            double strokeWidth = sw.toDouble();
            strokeWidth += m_keepout * GraphicsUtils::StandardFritzingDPI / GraphicsUtils::SVGDPI;
            element.setAttribute("stroke-width", strokeWidth);
        }
        else {
            element.setTagName("g");
        }
    }

	QSvgRenderer renderer2(doc.toByteArray());
	QPainter painter2;
	painter2.begin(&image);
	painter2.setRenderHint(QPainter::Antialiasing, false);
	renderer2.render(&painter2);
	painter2.end();

#ifndef QT_NO_DEBUG
	image.save(FolderUtils::getUserDataStorePath("") + "/mazeMakeHoles.png");
#endif

    return true;
}


bool MazeRouter::makeMasters(QString & message) {
    QList<ViewLayer::ViewLayerSpec> layerSpecs;
    layerSpecs << ViewLayer::Bottom;
    if (m_bothSidesNow) layerSpecs << ViewLayer::Top;

    double keepout = m_keepout * GraphicsUtils::StandardFritzingDPI / GraphicsUtils::SVGDPI;

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
        SvgFileSplitter::forceStrokeWidth(root, 2 * keepout, "#000000", true);
    }

    return true;

}
