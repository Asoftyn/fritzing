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
#include "../utils/textutils.h"
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
#include <QSettings>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QRadioButton>

///////////////////////////////////////////
//
//
//  deal with invisible connectors
//
//  if a part is already overlapping, leave it out of future checking?
//
//
///////////////////////////////////////////

static const QString TreatAsNet = "alsoNet";
const QString DRC::NotNet = "notnet";

const uchar DRC::BitTable[] = { 128, 64, 32, 16, 8, 4, 2, 1 }; 

bool pixelsCollide(QImage * image1, QImage * image2, QImage * image3, int x1, int y1, int x2, int y2, uint clr, QList<QPointF> & points) {
    bool result = false;
    const uchar * bits1 = image1->constScanLine(0);
    const uchar * bits2 = image2->constScanLine(0);
    int bytesPerLine = image1->bytesPerLine();
	for (int y = y1; y < y2; y++) {
        int offset = y * bytesPerLine;
		for (int x = x1; x < x2; x++) {
			//QRgb p1 = image1->pixel(x, y);
			//if (p1 == 0xffffffff) continue;

			//QRgb p2 = image2->pixel(x, y);
			//if (p2 == 0xffffffff) continue;

            int byteOffset = (x >> 3) + offset;
            uchar mask = DRC::BitTable[x & 7];

            if (*(bits1 + byteOffset) & mask) continue;
            if (*(bits2 + byteOffset) & mask) continue;

            image3->setPixel(x, y, clr);
			//DebugDialog::debug(QString("p1:%1 p2:%2").arg(p1, 0, 16).arg(p2, 0, 16));
			result = true;
            if (points.count() < 1000) {
                points.append(QPointF(x, y));
            }
		}
	}

	return result;
}

QStringList getNames(CollidingThing * collidingThing) {
    QStringList names;
    QList<ItemBase *> itemBases;
    if (collidingThing->connectorItem) {
        itemBases << collidingThing->connectorItem->attachedTo();
    }
    foreach (ItemBase * itemBase, itemBases) {
        if (itemBase) {
            itemBase = itemBase->layerKinChief();
            QString name = QObject::tr("Part %1 '%2'").arg(itemBase->title()) .arg(itemBase->instanceTitle());
            names << name;
        }
    }
    
    return names;
}

///////////////////////////////////////////////

DRCResultsDialog::DRCResultsDialog(const QString & message, const QStringList & messages, const QList<CollidingThing *> & collidingThings, 
                                    QGraphicsPixmapItem * displayItem, QImage * displayImage, PCBSketchWidget * sketchWidget, QWidget *parent) 
                 : QDialog(parent) 
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_messages = messages;
    m_sketchWidget = sketchWidget;
    m_displayItem = displayItem;
    if (m_displayItem) {
        m_displayItem->setFlags(0);
    }
    m_displayImage = displayImage;
    m_collidingThings = collidingThings;

	this->setWindowTitle(tr("DRC Results"));

	QVBoxLayout * vLayout = new QVBoxLayout(this);

    QLabel * label = new QLabel(message);
    label->setWordWrap(true);
	vLayout->addWidget(label);

    label = new QLabel(tr("Click on an item in the list to highlight of overlap it refers to."));
    label->setWordWrap(true);
	vLayout->addWidget(label);

    label = new QLabel(tr("Note: the list items and the red highlighting will not update as you edit your sketch--you must rerun the DRC. The highlighting will disappear when you close this dialog."));
    label->setWordWrap(true);
	vLayout->addWidget(label);

    QListWidget *listWidget = new QListWidget();
    for (int ix = 0; ix < messages.count(); ix++) {
        QListWidgetItem * item = new QListWidgetItem(messages.at(ix));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setData(Qt::UserRole, ix);
        listWidget->addItem(item);
    }
    vLayout->addWidget(listWidget);
	connect(listWidget, SIGNAL(itemPressed(QListWidgetItem *)), this, SLOT(pressedSlot(QListWidgetItem *)));
	connect(listWidget, SIGNAL(itemClicked(QListWidgetItem *)), this, SLOT(releasedSlot(QListWidgetItem *)));

	QDialogButtonBox * buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(close()));

	vLayout->addWidget(buttonBox);
	this->setLayout(vLayout);
}

DRCResultsDialog::~DRCResultsDialog() {
    if (m_displayItem != NULL && m_sketchWidget != NULL) {
        delete m_displayItem;
    }
    if (m_displayImage != NULL) {
        delete m_displayImage;
    }
    foreach (CollidingThing * collidingThing, m_collidingThings) {
        delete collidingThing;
    }
    m_collidingThings.clear();
}

void DRCResultsDialog::pressedSlot(QListWidgetItem * item) {
    if (item == NULL) return;

    int ix = item->data(Qt::UserRole).toInt();
    CollidingThing * collidingThing = m_collidingThings.at(ix);
    foreach (QPointF p, collidingThing->atPixels) {
        m_displayImage->setPixel(p.x(), p.y(), 2 /* 0xffffff00 */);
    }
    QPixmap pixmap = QPixmap::fromImage(*m_displayImage);
    m_displayItem->setPixmap(pixmap);
    if (collidingThing->connectorItem) {
        m_sketchWidget->selectItem(collidingThing->connectorItem->attachedTo());
    }
}

void DRCResultsDialog::releasedSlot(QListWidgetItem * item) {
    if (item == NULL) return;

    int ix = item->data(Qt::UserRole).toInt();
    CollidingThing * collidingThing = m_collidingThings.at(ix);
    foreach (QPointF p, collidingThing->atPixels) {
        m_displayImage->setPixel(p.x(), p.y(), 1 /* 0x80ff0000 */);
    }
    QPixmap pixmap = QPixmap::fromImage(*m_displayImage);
    m_displayItem->setPixmap(pixmap);
}

///////////////////////////////////////////

static const QString KeepoutSetting("DRC_Keepout");
static const double KeepoutDefault = 10;  // mils

double getKeepoutSetting(bool & inches) 
{
    double keepout = KeepoutDefault;  // mils
    QSettings settings;
    QString keepoutSetting = settings.value(KeepoutSetting, QString("%1in").arg(KeepoutDefault / 1000)).toString();
    inches = !keepoutSetting.endsWith("mm");
    bool ok;
    double candidate = TextUtils::convertToInches(keepoutSetting, &ok, false);
    if (ok) {
        keepout = candidate * 1000; // mils
    }
    return keepout;
}

DRCKeepoutDialog::DRCKeepoutDialog(QWidget *parent) : QDialog(parent) 
{
	this->setWindowTitle(tr("DRC Keepout Setting"));

	QVBoxLayout * vLayout = new QVBoxLayout(this);

	QLabel * label = new QLabel(tr("The 'keepout' is the minimum amount of space you want to keep between a connector or trace on one net and a connector or trace in another net"));
    label->setWordWrap(true);
	vLayout->addWidget(label);

    label = new QLabel(tr("A keepout of 0.01 inch (0.254 mm) is a good default."));
    label->setWordWrap(true);
	vLayout->addWidget(label);

	label = new QLabel(tr("Note: the smaller the keepout, the slower the DRC will run."));
	vLayout->addWidget(label);

    QFrame * frame = new QFrame;
    QHBoxLayout * frameLayout = new QHBoxLayout;

    m_spinBox = new QDoubleSpinBox;
    m_spinBox->setDecimals(4);
    connect(m_spinBox, SIGNAL(valueChanged(double)), this, SLOT(keepoutEntry()));
    connect(m_spinBox, SIGNAL(valueChanged(const QString &)), this, SLOT(keepoutEntry()));
    frameLayout->addWidget(m_spinBox);

    m_inRadio = new QRadioButton("in");
    frameLayout->addWidget(m_inRadio);
    connect(m_inRadio, SIGNAL(clicked()), this, SLOT(toInches()));

    m_mmRadio = new QRadioButton("mm");
    frameLayout->addWidget(m_mmRadio);
    connect(m_mmRadio, SIGNAL(clicked()), this, SLOT(toMM()));

    frameLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));

    bool inches;
    m_keepoutMils = getKeepoutSetting(inches);
    if (inches) {
        toInches();
        m_inRadio->setChecked(true);
    }
    else {
        toMM();
        m_mmRadio->setChecked(true);
    }

    frame->setLayout(frameLayout);
    vLayout->addWidget(frame);

	QDialogButtonBox * buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(saveAndAccept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	vLayout->addWidget(buttonBox);
	this->setLayout(vLayout);
}

DRCKeepoutDialog::~DRCKeepoutDialog() {
}


void DRCKeepoutDialog::toInches() {
    m_spinBox->blockSignals(true);
    m_spinBox->setRange(.001, 1);
    m_spinBox->setSingleStep(.001);
    m_spinBox->setValue(m_keepoutMils / 1000);
    m_spinBox->blockSignals(false);
}

void DRCKeepoutDialog::toMM() {
    m_spinBox->blockSignals(true);
    m_spinBox->setRange(.001 * 25.4, 1);
    m_spinBox->setSingleStep(.01);
    m_spinBox->setValue(m_keepoutMils * 25.4 / 1000);
    m_spinBox->blockSignals(false);
}

void DRCKeepoutDialog::keepoutEntry() {
    double k = m_spinBox->value();
    if (m_inRadio->isChecked()) {
        m_keepoutMils = k * 1000;
    }
    else {
        m_keepoutMils = k * 1000 / 25.4;
    }
}

void DRCKeepoutDialog::saveAndAccept() {
    QSettings settings;
    QString keepoutSetting;
    if (m_inRadio->isChecked()) {
        keepoutSetting = QString("%1in").arg(m_keepoutMils / 1000);
    }
    else {
        keepoutSetting = QString("%1mm").arg(m_keepoutMils * 25.4 / 1000);
    }
    settings.setValue(KeepoutSetting, keepoutSetting);
    accept();
}

///////////////////////////////////////////////

static QString CancelledMessage;

///////////////////////////////////////////

DRC::DRC(PCBSketchWidget * sketchWidget, ItemBase * board)
{
    CancelledMessage = tr("DRC was cancelled.");

    m_cancelled = false;
	m_sketchWidget = sketchWidget;
    m_board = board;
    m_displayItem = NULL;
    m_displayImage = NULL;
    m_plusImage = NULL;
    m_minusImage = NULL;
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
    foreach (QDomDocument * doc, m_masterDocs) {
        delete doc;
    }
}

bool DRC::start(bool showOkMessage) {
	QString message;
    QStringList messages;
    QList<CollidingThing *> collidingThings;

    bool result = startAux(message, messages, collidingThings);
    if (result) {
        if (messages.count() == 0) {
            message = tr("Your sketch is ready for production: there are no connectors or traces that overlap or are too close together.");
        }
        else {
            message = tr("The areas on your board highlighted in red are connectors and traces which may overlap or be too close together. ") +
					 tr("Reposition them and run the DRC again to find more problems");
        }
    }

#ifndef QT_NO_DEBUG
	m_displayImage->save(FolderUtils::getUserDataStorePath("") + "/testDRCDisplay.png");
#endif

    emit wantBothVisible();
    emit setProgressValue(m_maxProgress);
    emit hideProgress();

	if (messages.count() == 0) {
        if (showOkMessage) {
		    QMessageBox::information(m_sketchWidget->window(), tr("Fritzing"), message);
        }
	}
	else {
		DRCResultsDialog * dialog = new DRCResultsDialog(message, messages, collidingThings, m_displayItem, m_displayImage, m_sketchWidget, m_sketchWidget->window());
        if (showOkMessage) {
            dialog->show();
        }
        else {
            dialog->exec();
        }
        m_displayItem = NULL;
        m_displayImage = NULL;
	}

    return result;
}

bool DRC::startAux(QString & message, QStringList & messages, QList<CollidingThing *> & collidingThings) {
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

    bool inches;
    double keepoutMils = getKeepoutSetting(inches);  // keepout result is in mils
    
    double dpi = (1000 / keepoutMils);
    QRectF boardRect = m_board->sceneBoundingRect();
    QRectF sourceRes(0, 0, 
					 boardRect.width() * dpi / GraphicsUtils::SVGDPI, 
                     boardRect.height() * dpi / GraphicsUtils::SVGDPI);

    QSize imgSize(qCeil(sourceRes.width()), qCeil(sourceRes.height()));

    m_plusImage = new QImage(imgSize, QImage::Format_Mono);
    m_plusImage->fill(0xffffffff);

    m_minusImage = new QImage(imgSize, QImage::Format_Mono);
    m_minusImage->fill(0);

    m_displayImage = new QImage(imgSize, QImage::Format_Indexed8);
    m_displayImage->setColor(0, 0);
    m_displayImage->setColor(1, 0x80ff0000);
    m_displayImage->setColor(2, 0xffffff00);
    m_displayImage->fill(0);

    if (!makeBoard(m_minusImage, sourceRes)) {
        message = tr("Fritzing error: unable to render board svg.");
        return false;
    }

    extendBorder(1, m_minusImage);   // since the resolution = keepout, extend by 1

    QList<ViewLayer::ViewLayerSpec> layerSpecs;
    layerSpecs << ViewLayer::Bottom;
    if (bothSidesNow) layerSpecs << ViewLayer::Top;

    bool collisions = false;
    int emptyMasterCount = 0;
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
	    if (!masterDoc->setContent(master, &errorStr, &errorLine, &errorColumn)) {
            message = tr("Unexpected SVG rendering failure--contact fritzing.org");
		    return false;
	    }

	    ProcessEventBlocker::processEvents();
        if (m_cancelled) {
            message = CancelledMessage;
            return false;
        }

	    ProcessEventBlocker::processEvents();
        if (m_cancelled) {
            message = CancelledMessage;
            return false;
        }

        QDomElement root = masterDoc->documentElement();
        SvgFileSplitter::forceStrokeWidth(root, 2 * keepoutMils, "#000000", true, false);

        renderOne(masterDoc, m_plusImage, sourceRes);
        #ifndef QT_NO_DEBUG
	        m_plusImage->save(FolderUtils::getUserDataStorePath("") + QString("/testDRCmaster%1.png").arg(viewLayerSpec));
        #endif

	    ProcessEventBlocker::processEvents();
        if (m_cancelled) {
            message = CancelledMessage;
            return false;
        }

        QList<QPointF> atPixels;
        if (pixelsCollide(m_plusImage, m_minusImage, m_displayImage, 0, 0, imgSize.width(), imgSize.height(), 1 /* 0x80ff0000 */, atPixels)) {
            CollidingThing * collidingThing = findItemsAt(atPixels, m_board, viewLayerIDs, keepoutMils, dpi, true, NULL);
            QString msg = tr("Too close to a border (%1 layer)")
                .arg(viewLayerSpec == ViewLayer::Top ? tr("top") : tr("bottom"))
                ;
            emit setProgressMessage(msg);
            messages << msg;
            collidingThings << collidingThing;
            collisions = true;
            updateDisplay();
        }

        emit setProgressValue(progress++);

	    ProcessEventBlocker::processEvents();
        if (m_cancelled) {
            message = CancelledMessage;
            return false;
        }

    }

    while (singletons.count() > 0) {
        QList<ConnectorItem *> combined;
        QList<ConnectorItem *> singleton = singletons.takeFirst();
        ItemBase * chief = singleton.at(0)->attachedTo()->layerKinChief();
        combined.append(singleton);
        for (int ix = singletons.count() - 1; ix >= 0; ix--) {
            QList<ConnectorItem *> candidate = singletons.at(ix);
            if (candidate.at(0)->attachedTo()->layerKinChief() == chief) {
                combined.append(candidate);
                singletons.removeAt(ix);
            }
        }

        equis.append(combined);
    }

    int index = 0;
    foreach (ViewLayer::ViewLayerSpec viewLayerSpec, layerSpecs) {    
        if (viewLayerSpec == ViewLayer::Top) emit wantTopVisible();
        else emit wantBottomVisible();

        QDomDocument * masterDoc = m_masterDocs.value(viewLayerSpec, NULL);
        if (masterDoc == NULL) continue;

	    LayerList viewLayerIDs = ViewLayer::copperLayers(viewLayerSpec);
        viewLayerIDs.removeOne(ViewLayer::GroundPlane0);
        viewLayerIDs.removeOne(ViewLayer::GroundPlane1);

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
            splitNet(masterDoc, equi, m_minusImage, m_plusImage, sourceRes, viewLayerSpec, keepoutMils, index++);
            
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
                message = CancelledMessage;
                return false;
            }

            foreach (ConnectorItem * equ, rects.keys()) {
                QRectF rect = rects.value(equ).intersected(boardRect);
                double l = (rect.left() - boardRect.left()) * dpi / GraphicsUtils::SVGDPI;
                double t = (rect.top() - boardRect.top()) * dpi / GraphicsUtils::SVGDPI;
                double r = (rect.right() - boardRect.left()) * dpi / GraphicsUtils::SVGDPI;
                double b = (rect.bottom() - boardRect.top()) * dpi / GraphicsUtils::SVGDPI;
                //DebugDialog::debug(QString("l:%1 t:%2 r:%3 b:%4").arg(l).arg(t).arg(r).arg(b));
                QList<QPointF> atPixels;
                if (pixelsCollide(m_plusImage, m_minusImage, m_displayImage, l, t, r, b, 1 /* 0x80ff0000 */, atPixels)) {
                    CollidingThing * collidingThing = findItemsAt(atPixels, m_board, viewLayerIDs, keepoutMils, dpi, false, equ);
                    QStringList names = getNames(collidingThing);
                    QString name0 = names.at(0);
                    QString msg = tr("%1 is overlapping (%2 layer)")
                        .arg(name0)
                        .arg(viewLayerSpec == ViewLayer::Top ? tr("top") : tr("bottom"))
                        ;
                    messages << msg;
                    collidingThings << collidingThing;
                    emit setProgressMessage(msg);
                    collisions = true;
                    updateDisplay();
                }
            }

            emit setProgressValue(progress++);

	        ProcessEventBlocker::processEvents();
            if (m_cancelled) {
                message = CancelledMessage;
                return false;
            }
        }
    }

    return true;
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
	renderer.render(&painter  /*, sourceRes */);
	painter.end();

    // board should be white, borders should be black

#ifndef QT_NO_DEBUG
	image->save(FolderUtils::getUserDataStorePath("") + "/testDRCBoard.png");
#endif

    return true;
}

void DRC::splitNet(QDomDocument * masterDoc, QList<ConnectorItem *> & equi, QImage * minusImage, QImage * plusImage, QRectF & sourceRes, ViewLayer::ViewLayerSpec viewLayerSpec, double keepoutMils, int index) {
    // TreatAsNet marks connectors on the same part even though they are not on the same net
    // in other words, make sure there are no overlaps of connectors on the same part
    QList<QDomElement> net;
    QList<QDomElement> alsoNet;
    QList<QDomElement> notNet;
    splitNetPrep(masterDoc, equi, keepoutMils, TreatAsNet, net, alsoNet, notNet, false, false);
    foreach (QDomElement element, notNet) element.setTagName("g");
    foreach (QDomElement element, alsoNet) element.setTagName("g");

    renderOne(masterDoc, plusImage, sourceRes);
    #ifndef QT_NO_DEBUG
	    plusImage->save(FolderUtils::getUserDataStorePath("") + QString("/testDRCNet%1_%2.png").arg(viewLayerSpec).arg(index));
    #else
        Q_UNUSED(viewLayerSpec);
        Q_UNUSED(index);
    #endif

    // restore doc without net
    foreach (QDomElement element, net) {
        SvgFileSplitter::forceStrokeWidth(element, 2 * keepoutMils, "#000000", false, false);
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

void DRC::splitNetPrep(QDomDocument * masterDoc, QList<ConnectorItem *> & equi, double keepoutMils, const QString & treatAs, QList<QDomElement> & net, QList<QDomElement> & alsoNet, QList<QDomElement> & notNet, bool fill, bool checkIntersection) 
{
    QMultiHash<QString, QString> partIDs;
    QMultiHash<QString, ItemBase *> itemBases;
    QSet<QString> wireIDs;
    foreach (ConnectorItem * equ, equi) {
        ItemBase * itemBase = equ->attachedTo();
        if (itemBase == NULL) continue;

        if (itemBase->itemType() == ModelPart::Wire) {
            wireIDs.insert(QString::number(itemBase->id()));
        }

        if (equ->connector() == NULL) {
            // this shouldn't happen
            itemBase->debugInfo("!!!!!!!!!!!!!!!!!!!!!!!!!!!!! missing connector !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            equ->debugInfo("missing connector");
            continue;
        }
        
        SvgIdLayer * svgIdLayer = equ->connector()->fullPinInfo(itemBase->viewIdentifier(), itemBase->viewLayerID());
        partIDs.insert(QString::number(itemBase->id()), svgIdLayer->m_svgId);
        itemBases.insert(QString::number(itemBase->id()), itemBase);
    }

    QList<QDomElement> todo;
    todo << masterDoc->documentElement();
    bool firstTime = true;
    while (!todo.isEmpty()) {
        QDomElement element = todo.takeFirst();
        //QString string;
        //QTextStream stream(&string);
        //element.save(stream, 0);

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
                markSubs(element, NotNet);
            }
            else if (wireIDs.contains(partID)) {
                markSubs(element, "net");
            }
            else {
                splitSubs(masterDoc, element, "net", treatAs, svgIDs, itemBases.values(partID), checkIntersection);
            }
        }

        if (element.tagName() == "g") {
            element.removeAttribute("net");
            continue;
        }

        element.setAttribute("former", element.tagName());
        if (element.attribute("net") == "net") {
            net.append(element);
            SvgFileSplitter::forceStrokeWidth(element, -2 * keepoutMils, "#000000", false, fill);
        }
        else if (element.attribute("net") == TreatAsNet) {
            alsoNet.append(element);
        }
        else {
            // assume unmarked element belongs to notNet
            notNet.append(element);
        }
    }
}

void DRC::renderOne(QDomDocument * masterDoc, QImage * image, const QRectF & sourceRes) {
    QByteArray byteArray = masterDoc->toByteArray();
	QSvgRenderer renderer(byteArray);
	QPainter painter;
	painter.begin(image);
	painter.setRenderHint(QPainter::Antialiasing, false);
	renderer.render(&painter /*, sourceRes */);
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

void DRC::splitSubs(QDomDocument * doc, QDomElement & root, const QString & mark1, const QString & mark2, const QStringList & svgIDs, const QList<ItemBase *> & itemBases, bool checkIntersection)
{
    //QString string;
    //QTextStream stream(&string);
    //root.save(stream, 0);

    QStringList notSvgIDs;    
    if (checkIntersection && itemBases.count() > 0) {
        ItemBase * itemBase = itemBases.at(0);
        foreach (ConnectorItem * connectorItem, itemBase->cachedConnectorItems()) {
            SvgIdLayer * svgIdLayer = connectorItem->connector()->fullPinInfo(itemBase->viewIdentifier(), itemBase->viewLayerID());
            if (!svgIDs.contains(svgIdLayer->m_svgId)) {
                notSvgIDs.append(svgIdLayer->m_svgId);
            }
        }
    }

    // split subelements of a part into separate nets
    QList<QDomElement> todo;
    QList<QDomElement> netElements;
    todo << root;
    while (!todo.isEmpty()) {
        QDomElement element = todo.takeFirst();
        QString svgID = element.attribute("id");
        if (!svgID.isEmpty()) {
            if (svgIDs.contains(svgID)) {
                netElements << element;
                markSubs(element, mark1);
                // all children are marked so don't add these to todo
                continue;
            }
            else if (notSvgIDs.contains(svgID)) {
                markSubs(element, mark2);
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

    QList<QDomElement> toCheck;

    todo << root;
    while (!todo.isEmpty()) {
        QDomElement element = todo.takeFirst();
        QString net = element.attribute("net");
        if (net == mark1) continue;
        else if (!checkIntersection) {
            element.setAttribute("net", mark2);
        }
        else if (net == mark2) continue;
        else if (element.tagName() != "g") {
            element.setAttribute("oldid", element.attribute("id"));
            element.setAttribute("id", QString("_____toCheck_____%1").arg(toCheck.count()));
            toCheck << element;
        }

        QDomElement child = element.firstChildElement();
        while (!child.isNull()) {
            todo << child;
            child = child.nextSiblingElement();
        }
    }

    if (toCheck.count() > 0) {
        // deal with elements that are effectively part of a connector, but aren't separately labeled
        // such as a rect which overlaps a circle
        QDomElement masterRoot = doc->documentElement();
        QString svg = QString("<svg width='%1' height='%2' viewBox='%3'>\n")
            .arg(masterRoot.attribute("width"))
            .arg(masterRoot.attribute("height"))
            .arg(masterRoot.attribute("viewBox"));
        QString string;
        QTextStream stream(&string);
        root.save(stream, 0);
        svg += string;
        svg += "</svg>";
        QSvgRenderer renderer;
        renderer.load(svg.toUtf8());
        QList<QRectF> netRects;
        foreach (QDomElement element, netElements) {
            QString id = element.attribute("id");
            QRectF r = renderer.matrixForElement(id).mapRect(renderer.boundsOnElement(id));
            netRects << r;
        }
        QList<QRectF> checkRects;
        foreach (QDomElement element, toCheck) {
            QString id = element.attribute("id");
            QRectF r = renderer.matrixForElement(id).mapRect(renderer.boundsOnElement(id));
            checkRects << r;
            element.setAttribute("id", element.attribute("oldid"));
        }
        for (int i = 0; i < checkRects.count(); i++) {
            QRectF checkr = checkRects.at(i);
            QDomElement checkElement = toCheck.at(i);
            bool gotOne = false;
            double carea = checkr.width() * checkr.height();
            foreach (QRectF netr, netRects) {
                QRectF sect = netr.intersected(checkr);
                if (sect.isEmpty()) continue;

                double area = sect.width() * sect.height();
                if ((area > netr.width() * netr.height() / 2) && (area > carea / 2)) {
                    checkElement.setAttribute("net", mark1);
                    gotOne = true;
                    break;
                }
            }
            if (!gotOne) {
                checkElement.setAttribute("net", mark2);
            }
        }
    }
}

void DRC::updateDisplay() {
    QPixmap pixmap = QPixmap::fromImage(*m_displayImage);
    if (m_displayItem == NULL) {
        m_displayItem = new QGraphicsPixmapItem(pixmap);
        m_displayItem->setPos(m_board->sceneBoundingRect().topLeft());
        m_sketchWidget->scene()->addItem(m_displayItem);
        m_displayItem->setZValue(5000);
        m_displayItem->setScale(m_board->sceneBoundingRect().width() / m_displayImage->width());   // GraphicsUtils::SVGDPI / dpi
        m_displayItem->setVisible(true);
    }
    else {
        m_displayItem->setPixmap(pixmap);
    }
    ProcessEventBlocker::processEvents();
}

void DRC::cancel() {
	m_cancelled = true;
}

CollidingThing * DRC::findItemsAt(QList<QPointF> & atPixels, ItemBase * board, const LayerList & viewLayerIDs, double keepoutMils, double dpi, bool skipHoles, ConnectorItem * already) {
    Q_UNUSED(board);
    Q_UNUSED(viewLayerIDs);
    Q_UNUSED(keepoutMils);
    Q_UNUSED(dpi);
    Q_UNUSED(skipHoles);
    
    CollidingThing * collidingThing = new CollidingThing;
    collidingThing->connectorItem = already;
    collidingThing->atPixels = atPixels;

    return collidingThing;
}

void DRC::extendBorder(double keepout, QImage * image) {
    // TODO: scanlines

    // keepout in terms of the board grid size

    QImage copy = image->copy();

    int h = image->height();
    int w = image->width();
    for (int x = 0; x < w; x++) {
        for (int k = 0; k < keepout; k++) {
            image->setPixel(x, k, 0);
            image->setPixel(x, h - k - 1, 0);
        }
    }
    int ikeepout = qCeil(keepout);
    for (int y = 0; y < h; y++) {
        for (int k = 0; k < keepout; k++) {
            image->setPixel(k, y, 0);
            image->setPixel(w - k - 1, y, 0);
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
                    image->setPixel(dx, dy, 0);
                }
            }
        }
    }
}
