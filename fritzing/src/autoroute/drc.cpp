/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2012 Fachhochschule Potsdam - http://fh-potsdam.de

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.a

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

#include "drc.h"
#include "../connectors/svgidlayer.h"
#include "../sketch/pcbsketchwidget.h"
#include "../debugdialog.h"
#include "../items/virtualwire.h"
#include "../items/tracewire.h"
#include "../items/via.h"
#include "../utils/graphicsutils.h"
#include "../utils/folderutils.h"
#include "../connectors/connectoritem.h"
#include "../items/moduleidnames.h"
#include "../processeventblocker.h"
#include "../fsvgrenderer.h"
#include "../viewlayer.h"
#include "../processeventblocker.h"

#include <qmath.h>
#include <QApplication>
#include <QMessageBox>
#include <QPixmap>
#include <QSet>

///////////////////////////////////////////
//
//  deal with rotated board
//
//  deal with invisible connectors
//
//  keepout ui
//
//  singletons
//
//  why pixmap is invisible on overlaps
//
//  why simple overlap is not working
//
//  if a part is already overlapping, leave it out of future checking?
//
//  why are events blocking
//
///////////////////////////////////////////

bool pixelsCollide(QImage * image1, QImage * image2, QImage * image3, int x1, int y1, int x2, int y2, uint clr) {
    bool result = false;
	for (int y = y1; y < y2; y++) {
		for (int x = x1; x < x2; x++) {
			QRgb p1 = image1->pixel(x, y);
			if (p1 == 0xffffffff) continue;

			QRgb p2 = image2->pixel(x, y);
			if (p2 == 0xffffffff) continue;

            image3->setPixel(x, y, clr);
			//DebugDialog::debug(QString("p1:%1 p2:%2").arg(p1, 0, 16).arg(p2, 0, 16));
			result = true;
		}
	}

	return result;
}

///////////////////////////////////////////

DRC::DRC(PCBSketchWidget * sketchWidget, ItemBase * board)
{
    m_cancelled = false;
	m_sketchWidget = sketchWidget;
    m_board = board;
    m_displayItem = NULL;
    m_displayImage = NULL;
    m_plusImage = NULL;
    m_minusImage = NULL;
    foreach (QDomDocument * doc, m_masterDocs) {
        delete doc;
    }
}

DRC::~DRC(void)
{
    if (m_displayItem) {
        delete m_displayItem;
    }
    if (m_plusImage) {
        delete m_plusImage;
    }
    if (m_minusImage) {
        delete m_minusImage;
    }
    if (m_displayImage) {
        delete m_displayImage;
    }
}

bool DRC::start(QString & message) {
    bool result = startAux(message);
    emit wantBothVisible();
    emit setProgressValue(m_maxProgress);
    return result;
}

bool DRC::startAux(QString & message) {

    bool result = true;
    message = tr("Your sketch is ready for production: there are no connectors or traces that overlap or are too close together.");
    QStringList messages;

    bool bothSidesNow = m_sketchWidget->boardLayers() == 2;

    QList<ConnectorItem *> visited;
    QList< QList<ConnectorItem *> > equis;
    QList< QList<ConnectorItem *> > singletons;
    foreach (QGraphicsItem * item, m_sketchWidget->scene()->items()) {
        ConnectorItem * connectorItem = dynamic_cast<ConnectorItem *>(item);
        if (connectorItem == NULL) continue;
        if (!connectorItem->attachedTo()->isEverVisible()) continue;
        if (connectorItem->attachedTo()->getRatsnest()) continue;
        if (visited.contains(connectorItem)) continue;

        QList<ConnectorItem *> equi;
        equi.append(connectorItem);
        ConnectorItem::collectEqualPotential(equi, bothSidesNow, (ViewGeometry::RatsnestFlag | ViewGeometry::NormalFlag | ViewGeometry::PCBTraceFlag | ViewGeometry::SchematicTraceFlag) ^ m_sketchWidget->getTraceFlag());
        visited.append(equi);
        
        if (equi.count() == 1) {
            singletons.append(equi);
            continue;
        }

        ItemBase * firstPart = connectorItem->attachedTo()->layerKinChief();
        bool gotTwo = false;
        foreach (ConnectorItem * equ, equi) {
            if (equ->attachedTo()->layerKinChief() != firstPart) {
                gotTwo = true;
                break;
            }
        }
        if (!gotTwo) {
            singletons.append(equi);
            continue;
        }

        equis.append(equi);
    }

    m_maxProgress = equis.count() + singletons.count() + 1;
    if (bothSidesNow) m_maxProgress *= 2;
    emit setMaximumProgress(m_maxProgress);
    int progress = 1;
    emit setProgressValue(progress);

	ProcessEventBlocker::processEvents();

    double keepout = 7;  // mils
    double dpi = (1000 / keepout);
    QRectF boardRect = m_board->sceneBoundingRect();
    QRectF sourceRes(0, 0, 
					 boardRect.width() * dpi / GraphicsUtils::SVGDPI, 
                     boardRect.height() * dpi / GraphicsUtils::SVGDPI);

    QSize imgSize(qCeil(sourceRes.width()), qCeil(sourceRes.height()));

    m_plusImage = new QImage(imgSize, QImage::Format_Mono);
    m_plusImage->fill(0xffffffff);

    m_minusImage = new QImage(imgSize, QImage::Format_Mono);
    m_minusImage->fill(0);

    m_displayImage = new QImage(imgSize, QImage::Format_ARGB32);
    m_displayImage->fill(0);

    result = makeBoard(m_minusImage, sourceRes);
    if (!result) {
        message = tr("Fritzing error: unable to render board svg.");
        return false;
    }

    QList<ViewLayer::ViewLayerSpec> layerSpecs;
    layerSpecs << ViewLayer::Bottom;
    if (bothSidesNow) layerSpecs << ViewLayer::Top;

    bool collisions = false;
    int emptyMasterCount = 0;
    foreach (ViewLayer::ViewLayerSpec viewLayerSpec, layerSpecs) {  
        if (viewLayerSpec == ViewLayer::Top) emit wantTopVisible();
        else emit wantBottomVisible();

	    LayerList viewLayerIDs = ViewLayer::copperLayers(viewLayerSpec);
        QRectF masterImageRect;
        bool empty;
	    QString master = m_sketchWidget->renderToSVG(GraphicsUtils::SVGDPI, viewLayerIDs, true, masterImageRect, m_board, GraphicsUtils::StandardFritzingDPI, false, false, empty);
        if (master.isEmpty()) {
            if (++emptyMasterCount == layerSpecs.count()) {
                message = tr("No traces or connectors to check");
                return false;
            }

            progress++;
            continue;
	    }
       
	    QDomDocument * masterDoc = new QDomDocument();
        m_masterDocs.insert(viewLayerSpec, masterDoc);

	    QString errorStr;
	    int errorLine;
	    int errorColumn;
	    bool result = masterDoc->setContent(master, &errorStr, &errorLine, &errorColumn);
	    if (!result) {
            message = tr("Unexpected SVG rendering failure--contact fritzing.org");
		    return false;
	    }

	    ProcessEventBlocker::processEvents();
        if (m_cancelled) {
            message = tr("cancelled");
            return false;
        }


        if (master.contains(FSvgRenderer::NonConnectorName)) {
            makeHoles(masterDoc, m_minusImage, sourceRes, viewLayerSpec);
        }

	    ProcessEventBlocker::processEvents();
        if (m_cancelled) {
            message = tr("cancelled");
            return false;
        }

        QDomElement root = masterDoc->documentElement();
        SvgFileSplitter::changeStrokeWidth(root, 2 * keepout, false, true, true);

        renderOne(masterDoc, m_plusImage, sourceRes);
        //#ifndef QT_NO_DEBUG
	        m_plusImage->save(FolderUtils::getUserDataStorePath("") + QString("/testDRCmaster%1.png").arg(viewLayerSpec));
        //#endif

	    ProcessEventBlocker::processEvents();
        if (m_cancelled) {
            message = tr("cancelled");
            return false;
        }

        if (pixelsCollide(m_plusImage, m_minusImage, m_displayImage, 0, 0, imgSize.width(), imgSize.height(), 0x80ff0000)) {
            QString msg = tr("Too close to a border or a hole (%1 layer)").arg(viewLayerSpec == ViewLayer::Top ? tr("top") : tr("bottom"));
            emit setProgressMessage(msg);
            messages << msg;
            collisions = true;
            updateDisplay(dpi);
            result = false;
        }

        emit setProgressValue(progress++);

	    ProcessEventBlocker::processEvents();
        if (m_cancelled) {
            message = tr("cancelled");
            return false;
        }

    }

    int index = 0;
    foreach (ViewLayer::ViewLayerSpec viewLayerSpec, layerSpecs) {    
        if (viewLayerSpec == ViewLayer::Top) emit wantTopVisible();
        else emit wantBottomVisible();

        QDomDocument * masterDoc = m_masterDocs.value(viewLayerSpec, NULL);
        if (masterDoc == NULL) continue;

	    LayerList viewLayerIDs = ViewLayer::copperLayers(viewLayerSpec);

        QList<ConnectorItem *> singletons;

        foreach (QList<ConnectorItem *> equi, equis) {
            bool inLayer = false;
            foreach (ConnectorItem * equ, equi) {
                if (viewLayerIDs.contains(equ->attachedToViewLayerID())) {
                    inLayer = true;
                    break;
                }
            }
            if (!inLayer) {
                progress++;
                continue;
            }

            // we have a net;
            m_plusImage->fill(0xffffffff);
            m_minusImage->fill(0xffffffff);
            splitNet(masterDoc, equi, m_minusImage, m_plusImage, sourceRes, viewLayerSpec, keepout, index++);
            
            QHash<ConnectorItem *, QRectF> rects;
            QList<Wire *> wires;
            foreach (ConnectorItem * equ, equi) {
                if (viewLayerIDs.contains(equ->attachedToViewLayerID())) {
                    if (equ->attachedToItemType() == ModelPart::Wire) {
                        Wire * wire = qobject_cast<Wire *>(equ->attachedTo());
                        if (!wires.contains(wire)) {
                            wires.append(wire);
                            // could break diagonal wires into a series of rects
                            rects.insert(equ, wire->sceneBoundingRect());
                        }
                    }
                    else {
                        rects.insert(equ, equ->sceneBoundingRect());
                    }
                }
            }

	        ProcessEventBlocker::processEvents();
            if (m_cancelled) {
                message = tr("cancelled");
                return false;
            }

            foreach (ConnectorItem * equ, rects.keys()) {
                QRectF rect = rects.value(equ).intersected(boardRect);
                double l = (rect.left() - boardRect.left()) * dpi / GraphicsUtils::SVGDPI;
                double t = (rect.top() - boardRect.top()) * dpi / GraphicsUtils::SVGDPI;
                double r = (rect.right() - boardRect.left()) * dpi / GraphicsUtils::SVGDPI;
                double b = (rect.bottom() - boardRect.top()) * dpi / GraphicsUtils::SVGDPI;
                DebugDialog::debug(QString("l:%1 t:%2 r:%3 b:%4").arg(l).arg(t).arg(r).arg(b));
                if (pixelsCollide(m_plusImage, m_minusImage, m_displayImage, l, t, r, b, 0x80ff0000)) {
                    ItemBase * itemBase = equ->attachedTo();
                    QString msg = tr("Part %1 '%2' crosses another net on %3 layer")
                        .arg(itemBase->title())
                        .arg(itemBase->instanceTitle())
                        .arg(viewLayerSpec == ViewLayer::Top ? tr("top") : tr("bottom"))
                      ;
                    messages << msg;
                    emit setProgressMessage(msg);
                    collisions = true;
                    updateDisplay(dpi);
                    result = false;
                }
            }

            emit setProgressValue(progress++);

	        ProcessEventBlocker::processEvents();
            if (m_cancelled) {
                message = tr("cancelled");
                return false;
            }
        }
    }

    if (messages.count() > 0) {
        message = messages.join("\n");
    }

    return result;
}

void DRC::makeHoles(QDomDocument * masterDoc, QImage * image, QRectF & sourceRes, ViewLayer::ViewLayerSpec viewLayerSpec) {
    QSet<QString> holeIDs;

    foreach (QGraphicsItem * item, m_sketchWidget->scene()->items()) {
        ItemBase * itemBase = dynamic_cast<ItemBase *>(item);
        if (itemBase == NULL) continue;
        if (itemBase->itemType() != ModelPart::Hole) continue;

        holeIDs.insert(QString::number(itemBase->id()));
    }

    QList<QDomElement> todo;
    QList<QDomElement> holes;
    QList<QDomElement> notHoles;
    todo << masterDoc->documentElement();
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
            holes.append(element);
            element.setAttribute("fill", "black");
        }
        else {
            notHoles.append(element);
            element.setAttribute("former", element.tagName());
            element.setTagName("g");
        }

    }

    renderOne(masterDoc, image, sourceRes);
    //#ifndef QT_NO_DEBUG
	    image->save(FolderUtils::getUserDataStorePath("") + QString("/testDRCBoardHoles%1.png").arg(viewLayerSpec));
    //#endif

    // restore doc without holes
    foreach (QDomElement element, holes) {
        element.setTagName("g");
    }
    foreach (QDomElement element, notHoles) {
        element.setTagName(element.attribute("former"));
    }
}

bool DRC::makeBoard(QImage * image, QRectF & sourceRes) {
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
	painter.begin(image);
	painter.setRenderHint(QPainter::Antialiasing, false);
	DebugDialog::debug("boardbounds", sourceRes);
	renderer.render(&painter, sourceRes);
	painter.end();

    // board should be white, borders should be black

#ifndef QT_NO_DEBUG
	image->save(FolderUtils::getUserDataStorePath("") + "/testDRCBoard.png");
#endif

    return true;
}

void DRC::splitNet(QDomDocument * masterDoc, QList<ConnectorItem *> & equi, QImage * minusImage, QImage * plusImage, QRectF & sourceRes, ViewLayer::ViewLayerSpec viewLayerSpec, double keepout, int index) {
    QMultiHash<QString, QString> partIDs;
    QSet<QString> wireIDs;
    foreach (ConnectorItem * equ, equi) {
        ItemBase * itemBase = equ->attachedTo();
        if (itemBase == NULL) continue;

        if (itemBase->itemType() == ModelPart::Wire) {
            wireIDs.insert(QString::number(itemBase->id()));
        }

        SvgIdLayer * svgIdLayer = equ->connector()->fullPinInfo(itemBase->viewIdentifier(), itemBase->viewLayerID());
        partIDs.insert(QString::number(itemBase->id()), svgIdLayer->m_svgId);
    }

    QList<QDomElement> todo;
    QList<QDomElement> net;
    QList<QDomElement> alsoNet;
    QList<QDomElement> notNet;
    todo << masterDoc->documentElement();
    bool firstTime = true;
    while (!todo.isEmpty()) {
        QDomElement element = todo.takeFirst();
        QString string;
        QTextStream stream(&string);
        element.save(stream, 0);

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
            QStringList svgIDs = partIDs.values(partID);
            if (svgIDs.count() == 0) {
                markSubs(element, "notnet");
            }
            else if (wireIDs.contains(partID)) {
                markSubs(element, "net");
            }
            else {
                splitSubs(element, "net", "alsoNet", svgIDs);
            }
        }

        if (element.tagName() == "g") continue;

        if (element.attribute("net") == "net") {
            net.append(element);
            SvgFileSplitter::changeStrokeWidth(element, -2 * keepout, false, true, false);
        }
        else if (element.attribute("net") == "alsoNet") {
            alsoNet.append(element);
            element.setAttribute("former", element.tagName());
            element.setTagName("g");
        }
        else {
            // assume unmarked element belongs to notNet
            notNet.append(element);
            element.setAttribute("former", element.tagName());
            element.setTagName("g");
        }
    }

    renderOne(masterDoc, plusImage, sourceRes);
    #ifndef QT_NO_DEBUG
	    plusImage->save(FolderUtils::getUserDataStorePath("") + QString("/testDRCNet%1_%2.png").arg(viewLayerSpec).arg(index));
    #else
        Q_UNUSED(viewLayerSpec);
        Q_UNUSED(index);
    #endif

    // restore doc without net
    foreach (QDomElement element, net) {
        SvgFileSplitter::changeStrokeWidth(element, 2 * keepout, false, true, false);
        element.setAttribute("former", element.tagName());
        element.removeAttribute("net");
        element.setTagName("g");
    }
    foreach (QDomElement element, notNet) {
        element.setTagName(element.attribute("former"));
        element.removeAttribute("net");
    }

    renderOne(masterDoc, minusImage, sourceRes);
    #ifndef QT_NO_DEBUG
	    minusImage->save(FolderUtils::getUserDataStorePath("") + QString("/testDRCNotNet%1_%2.png").arg(viewLayerSpec).arg(index));
    #endif

     // master doc restored to original state
    foreach (QDomElement element, net) {
        element.setTagName(element.attribute("former"));
    }
    foreach (QDomElement element, alsoNet) {
        element.setTagName(element.attribute("former"));
        element.removeAttribute("net");
    }
}

void DRC::renderOne(QDomDocument * masterDoc, QImage * image, QRectF & sourceRes) {
    QByteArray byteArray = masterDoc->toByteArray();
	QSvgRenderer renderer(byteArray);
	QPainter painter;
	painter.begin(image);
	painter.setRenderHint(QPainter::Antialiasing, false);
	renderer.render(&painter, sourceRes);
	painter.end();
}

void DRC::markSubs(QDomElement & root, const QString & mark) {
    QList<QDomElement> todo;
    todo << root;
    while (!todo.isEmpty()) {
        QDomElement element = todo.takeFirst();
        element.setAttribute("net", mark);
        QDomElement child = element.firstChildElement();
        while (!child.isNull()) {
            todo << child;
            child = child.nextSiblingElement();
        }
    }
}

void DRC::splitSubs(QDomElement & root, const QString & mark1, const QString & mark2, const QStringList & svgIDs)
{
    QList<QDomElement> todo;
    todo << root;
    while (!todo.isEmpty()) {
        QDomElement element = todo.takeFirst();
        QString svgID = element.attribute("id");
        if (!svgID.isEmpty()) {
            if (svgIDs.contains(svgID)) {
                markSubs(element, mark1);
                // all children are marked so don't add these to todo
                continue;
            }
        }

        QDomElement child = element.firstChildElement();
        while (!child.isNull()) {
            todo << child;
            child = child.nextSiblingElement();
        }
    }
    todo << root;
    while (!todo.isEmpty()) {
        QDomElement element = todo.takeFirst();
        if (element.attribute("net") != mark1) {
            element.setAttribute("net", mark2);
        }

        QDomElement child = element.firstChildElement();
        while (!child.isNull()) {
            todo << child;
            child = child.nextSiblingElement();
        }
    }
}

void DRC::updateDisplay(double dpi) {
    if (m_displayItem) {
        delete m_displayItem;
        m_displayItem = NULL;
    }

    QPixmap pixmap = QPixmap::fromImage(*m_displayImage);
    m_displayItem = new QGraphicsPixmapItem(pixmap);
    m_displayItem->setPos(m_board->pos());
    m_sketchWidget->scene()->addItem(m_displayItem);
    m_displayItem->setZValue(5000);
    m_displayItem->setScale(GraphicsUtils::SVGDPI / dpi);
    m_displayItem->setVisible(true);
    ProcessEventBlocker::processEvents();
}

void DRC::cancel() {
	m_cancelled = true;
}
