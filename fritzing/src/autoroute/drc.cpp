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
#include "../sketch/pcbsketchwidget.h"
#include "../debugdialog.h"
#include "../items/virtualwire.h"
#include "../items/tracewire.h"
#include "../items/jumperitem.h"
#include "../utils/graphicsutils.h"
#include "../utils/folderutils.h"
#include "../connectors/connectoritem.h"
#include "../items/moduleidnames.h"
#include "../processeventblocker.h"
#include "../fsvgrenderer.h"
#include "../viewlayer.h"

#include <qmath.h>
#include <QApplication>
#include <QMessageBox>

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

			DebugDialog::debug(QString("p1:%1 p2:%2").arg(p1, 0, 16).arg(p2, 0, 16));

			result = true;
		}
	}

	return result;
}

///////////////////////////////////////////

DRC::DRC(PCBSketchWidget * sketchWidget, ItemBase * board)
{
	m_sketchWidget = sketchWidget;
    m_board = board;
}

DRC::~DRC(void)
{
}

bool DRC::start(QString & message) {
    bool result = true;
    message = tr("Your sketch is ready for production: there are no connectors or traces that overlap or are too close together.");


    double keepout = 7;  // mils
    double dpi = (1000 / keepout) * 2;
    QRectF source = m_board->sceneBoundingRect();
	source.moveTo(0, 0);
    QRectF sourceRes(0, 0, 
					 source.width() * dpi / GraphicsUtils::SVGDPI, 
                     source.height() * dpi / GraphicsUtils::SVGDPI);

    QSize imgSize(qCeil(sourceRes.width()), qCeil(sourceRes.height()));

    m_plusImage = new QImage(imgSize, QImage::Format_Mono);
    m_plusImage->fill(0xffffffff);

    m_minusImage = new QImage(imgSize, QImage::Format_Mono);
    m_minusImage->fill(0);

    m_displayImage = new QImage(imgSize, QImage::Format_ARGB32);
    m_displayImage->fill(0);

	LayerList viewLayerIDs;
	viewLayerIDs << ViewLayer::Board;
	QRectF boardImageRect;
	bool empty;
	QString boardSvg = m_sketchWidget->renderToSVG(GraphicsUtils::SVGDPI, viewLayerIDs, true, boardImageRect, m_board, GraphicsUtils::StandardFritzingDPI, false, false, empty);
	if (boardSvg.isEmpty()) {
        message = tr("Fritzing error: unable to render board svg (1).");
		return false;
	}

    QByteArray boardByteArray;
    QString tempColor("#ffffff");
    QStringList exceptions;
	exceptions << "none" << "";
    if (!SvgFileSplitter::changeColors(boardSvg, tempColor, exceptions, boardByteArray)) {
		return NULL;
	}

	QSvgRenderer renderer(boardByteArray);
	QPainter painter;
	painter.begin(m_minusImage);
	painter.setRenderHint(QPainter::Antialiasing, false);
	DebugDialog::debug("boardbounds", sourceRes);
	renderer.render(&painter, sourceRes);
	painter.end();

    // board should be white, borders should be black

#ifndef QT_NO_DEBUG
	m_minusImage->save(FolderUtils::getUserDataStorePath("") + "/testDRCBoard.png");
#endif

    bool bothSidesNow = m_sketchWidget->boardLayers() == 2;
    QList<ViewLayer::ViewLayerSpec> layerSpecs;
    layerSpecs << ViewLayer::Bottom;
    if (bothSidesNow) layerSpecs << ViewLayer::Top;

    QList<QString> masters;
    int emptyMasterCount = 0;
    foreach (ViewLayer::ViewLayerSpec viewLayerSpec, layerSpecs) {    
	    viewLayerIDs = ViewLayer::copperLayers(viewLayerSpec);
        QRectF masterImageRect;
	    QString master = m_sketchWidget->renderToSVG(GraphicsUtils::SVGDPI, viewLayerIDs, true, masterImageRect, m_board, GraphicsUtils::StandardFritzingDPI, false, false, empty);
	    masters.append(master);
        if (master.isEmpty()) {
            if (++emptyMasterCount == layerSpecs.count()) {
                message = tr("No traces or connectors to check");
                return false;
            }

            continue;
	    }
       
	    QDomDocument masterDoc;
	    QString errorStr;
	    int errorLine;
	    int errorColumn;
	    bool result = masterDoc.setContent(master, &errorStr, &errorLine, &errorColumn);
	    if (!result) {
            message = tr("Unexpected SVG rendering failure--contact fritzing.org");
		    return false;
	    }

        if (master.contains(FSvgRenderer::NonConnectorName)) {
            makeHoles(masterDoc, m_minusImage, sourceRes);
        }

        QDomElement root = masterDoc.documentElement();
        SvgFileSplitter::changeStrokeWidth(root, 2 * keepout, false, true);

        QByteArray byteArray = masterDoc.toByteArray();
	    QSvgRenderer renderer(byteArray);
	    QPainter painter;
	    painter.begin(m_plusImage);
	    painter.setRenderHint(QPainter::Antialiasing, false);
	    renderer.render(&painter, sourceRes);
	    painter.end();
        #ifndef QT_NO_DEBUG
	        m_plusImage->save(FolderUtils::getUserDataStorePath("") + "/testDRCmaster.png");
        #endif


        if (pixelsCollide(m_plusImage, m_minusImage, m_displayImage, 0, 0, imgSize.width(), imgSize.height(), 0xffff0000)) {
            message = tr("Too close to a border or a hole");
            result = false;
        }

    }




	

    /*


	QByteArray copperByteArray;
	if (!SvgFileSplitter::changeStrokeWidth(svg, m_strokeWidthIncrement, false, true, copperByteArray)) {
		return NULL;
	}


    m_keepout = GraphicsUtils::mils2pixels(7, GraphicsUtils::SVGDPI);

	QHash<ConnectorItem *, int> indexer;
    bool bothSides = true;
	m_sketchWidget->collectAllNets(indexer, m_allPartConnectorItems, false, bothSides);

    */

    return result;
}

void DRC::cleanup() {
	foreach (QList<ConnectorItem *> * connectorItems, m_allPartConnectorItems) {
		delete connectorItems;
	}
	m_allPartConnectorItems.clear();
}

void DRC::makeHoles(QDomDocument & masterDoc, QImage * image, QRectF & sourceRes) {
    QList<QDomElement> todo;
    QList<QDomElement> holes;
    QList<QDomElement> notHoles;
    todo << masterDoc.documentElement();
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

    QByteArray byteArray = masterDoc.toByteArray();
	QSvgRenderer renderer(byteArray);
	QPainter painter;
	painter.begin(image);
	painter.setRenderHint(QPainter::Antialiasing, false);
	renderer.render(&painter, sourceRes);
	painter.end();
    #ifndef QT_NO_DEBUG
	    image->save(FolderUtils::getUserDataStorePath("") + "/testDRCBoardHoles.png");
    #endif

    // restore doc without holes
    foreach (QDomElement element, holes) {
        element.setTagName("g");
    }
    foreach (QDomElement element, notHoles) {
        element.setTagName(element.attribute("former"));
    }
}