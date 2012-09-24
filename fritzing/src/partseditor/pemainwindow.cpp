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

/* TO DO ******************************************


    ///////////////////////////////// first release ///////////////////////////////

        crash when swapping part during save
        
        crashed when saving the description

        changed terminal points sometimes not being saved

        connector locations are not updating properly when a part in the sketch is edited

		don't allow buses until all pins are assigned: check this works

        from partseditorview.cpp
	        bool fileHasChanged = (m_viewIdentifier == ViewLayer::IconView) ? false : TextUtils::fixPixelDimensionsIn(fileContent);
	        fileHasChanged |= TextUtils::fixViewboxOrigin(fileContent);
	
        for svg import check for flaws:
            multiple connector or terminal ids

        smd vs. tht
            disable pad for throughhole parts, disable m/f for smd parts
            for now don't allow mixing
			what happens if you open an smd placed on the bottom layer?

        multiple matching connector id--trash any other matching id

        keep originating file in fzp/svg and use it for naming
            needs updated when the part is first loaded or the svg is changed thereafter

        leaves should be on top of svg tree (partly done)

        lock should lock all fields

        check all MainWindow * casts

        first time help--trigger html page

	    clean up menus
			test everything visible works

		why isn't swapping available when a family has new parts with multiple variant values?

    ////////////////////////////// second release /////////////////////////////////

		restore editable pin labels functionality
			requires storing labels in the part rather than in the sketch

        allow blank new part?

		restore parts bin

        buses 
			secondary representation (list view)
				allow to delete bus, delete nodemember, add nodemember using right-click for now
				hide bus list if there are no buses

		use the actual svg shape instead of rectangles

		properties and tags entries allow duplicates

		delete temp files after crash

        show in OS button only shows folder and not file in linux

        smd vs. tht
            after it's all over remove copper0 if part is all smd
            split copper0 and copper1
            allow mixed tht/smd parts in fritzing but for now disable flipsmd
            allow smd with holes to flip?

        import
            kicad sch files?

        on svg import detect all connector IDs
            if any are invisible, tell user this is obsolete

        bury connectors behind other connectors
			can already be done

        force an export etchable svg to make sure the part is right?

        dump old parts editor resources

        sort connector list alphabetically or numerically?

        bendable legs

        hidden views (i.e. view-specific parts)

        set flippable
        
        connector duplicate op

        add layers:  put everything in silkscreen, then give copper1, copper0 checkbox
            what about breadboardbreadboard or other odd layers?
            if you click something as a connector, automatically move it into copper
                how to distinguish between both and top--default to both, let user set "pad"
			setting layer for top level group sets all children?

        swap connector metadata op

        delete op
    
        move connectors with arrow keys, or typed coordinates
	    drag and drop later

        import
            eagle lbr
            eagle brd

        for schematic view 
            offer lines-or-pins, rects, and a selection of standard schematic icons in the parts bin

        for breadboard view
            import 
            generate ICs, dips, sips, breakouts

        for pcb view
            pads, pins (circle, rect, oblong), holes
            lines and curves?
            import silkscreen

        hybrids

        flip and rotate images w/in the view?

        taxonomy entry like tag entry?

        new schematic layout specs

        give users a family popup with all family names
            ditto for other properties

        matrix problem with move and duplicate (i.e. if element inherits a matrix from far above)
            even a problem when inserting hole, pad, or pin
            eliminate internal transforms, then insert inverted matrix, then use untransformed coords based on viewbox
            
        kicad schematic does not match our schematic


***************************************************/

#include "pemainwindow.h"
#include "pemetadataview.h"
#include "peconnectorsview.h"
#include "pecommands.h"
#include "petoolview.h"
#include "pesvgview.h"
#include "pegraphicsitem.h"
#include "kicadmoduledialog.h"
#include "../debugdialog.h"
#include "../sketch/breadboardsketchwidget.h"
#include "../sketch/schematicsketchwidget.h"
#include "../sketch/pcbsketchwidget.h"
#include "../mainwindow/sketchareawidget.h"
#include "../referencemodel/referencemodel.h"
#include "../utils/graphicsutils.h"
#include "../utils/textutils.h"
#include "../utils/folderutils.h"
#include "../mainwindow/fdockwidget.h"
#include "../fsvgrenderer.h"
#include "../partsbinpalette/binmanager/binmanager.h"
#include "../svg/gedaelement2svg.h"
#include "../svg/kicadmodule2svg.h"
#include "../svg/kicadschematic2svg.h"
#include "../sketchtoolbutton.h"
#include "../items/virtualwire.h"
#include "../connectors/connectoritem.h"
#include "../connectors/bus.h"
#include "../installedfonts.h"

#include <QtDebug>
#include <QApplication>
#include <QSvgGenerator>
#include <QMenuBar>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <limits>

////////////////////////////////////////////////////

bool GotZeroConnector = false;

static const QString ReferenceFileString("referenceFile");

static const int IconViewIndex = 3;
static const int MetadataViewIndex = 4;
static const int ConnectorsViewIndex = 5;

static QHash<ViewLayer::ViewIdentifier, int> ZList;
const static int PegiZ = 5000;
const static int RatZ = 6000;

static long FakeGornSiblingNumber = 0;

bool byID(QDomElement & c1, QDomElement & c2)
{
    int c1id = -1;
    int c2id = -1;
	int ix = IntegerFinder.indexIn(c1.attribute("id"));
    if (ix >= 0) c1id = IntegerFinder.cap(0).toInt();
    ix = IntegerFinder.indexIn(c2.attribute("id"));
    if (ix >= 0) c2id = IntegerFinder.cap(0).toInt();

    if (c1id == 0 || c2id == 0) GotZeroConnector = true;
	
	return c1id <= c2id;
}

////////////////////////////////////////////////////

IconSketchWidget::IconSketchWidget(ViewLayer::ViewIdentifier viewIdentifier, QWidget *parent)
    : SketchWidget(viewIdentifier, parent)
{
	m_shortName = QObject::tr("ii");
	m_viewName = QObject::tr("Icon View");
	initBackgroundColor();
}

void IconSketchWidget::addViewLayers() {
	addIconViewLayers();
}

/////////////////////////////////////////////////////

PEMainWindow::PEMainWindow(ReferenceModel * referenceModel, QWidget * parent)
	: MainWindow(referenceModel, parent)
{
	m_autosaveTimer.stop();
    disconnect(&m_autosaveTimer, SIGNAL(timeout()), this, SLOT(backupSketch()));
    m_gaveSaveWarning = m_canSave = false;
    m_settingsPrefix = "pe/";
    m_guid = FolderUtils::getRandText();
    m_fileIndex = 0;
	m_userPartsFolderPath = FolderUtils::getUserDataStorePath("parts")+"/user/";
	m_userPartsFolderSvgPath = FolderUtils::getUserDataStorePath("parts")+"/svg/user/";
    m_peToolView = NULL;
    m_peSvgView = NULL;
    m_connectorsView = NULL;
}

PEMainWindow::~PEMainWindow()
{
    // PEGraphicsItems are still holding QDomElement so delete them before m_fzpDocument is deleted
    killPegi();

	// kill temp files
	foreach (QString string, m_toDelete) {
		QFile::remove(string);
	}
	QDir dir = QDir::temp();
    dir.rmdir(m_guid);
}

void PEMainWindow::closeEvent(QCloseEvent *event) 
{
	qDebug() << "close event";

	QString message;

	if (m_inFocusWidgets.count() > 0) {
		bool gotOne = false;
		// should only be one in-focus widget
		foreach (QWidget * widget, m_inFocusWidgets) {
			QLineEdit * lineEdit = qobject_cast<QLineEdit *>(widget);
			if (lineEdit) {
				if (lineEdit->isModified()) {
					lineEdit->clearFocus();
					gotOne = true;
				}
			}
			else {
				QTextEdit * textEdit = qobject_cast<QTextEdit *>(widget);
				if (textEdit) {
					if (textEdit->document()->isModified()) {
						textEdit->clearFocus();
						gotOne = true;
					}
				}
			}
		}
		if (gotOne) {
			message =  tr("There is one last edit still pending.");
		}
	}

	QString family = m_metadataView->family();
	QString variant = m_metadataView->variant();

	if (message.isEmpty()) {
		if (family.isEmpty()) {
			message = tr("'family'can not be blank.");
		}
	}

	if (message.isEmpty()) {
		if (variant.isEmpty()) {
			message = tr("'variant' can not be blank.");
		}
	}

	if (message.isEmpty()) {
		QStringList variants = m_referenceModel->propValues(family, "variant", true);
		if (variants.contains(variant, Qt::CaseInsensitive)) {
			message = tr("Variant '%1' is in use. A part's variant must be unique within a family.").arg(variant);
		}
	}

	QStringList keys = m_metadataView->properties().keys();

	if (message.isEmpty()) {
		if (keys.contains("family", Qt::CaseInsensitive)) {
			message = tr("Duplicate 'family' property not allowed");
		}
	}

	if (message.isEmpty()) {
		if (keys.contains("variant", Qt::CaseInsensitive)) {
			message = tr("Duplicate 'variant' property not allowed");
		}
	}

	if (!message.isEmpty()) {
	    QMessageBox messageBox(this);
	    messageBox.setWindowTitle(tr("Close without saving?"));

        message += tr("\n\nDo you want to keep working or close without saving?");

	    messageBox.setText(message);
	    messageBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	    messageBox.setDefaultButton(QMessageBox::Cancel);
	    messageBox.setIcon(QMessageBox::Warning);
	    messageBox.setWindowModality(Qt::WindowModal);
	    messageBox.setButtonText(QMessageBox::Ok, tr("Close without saving"));
	    messageBox.setButtonText(QMessageBox::Cancel, tr("Keep working"));
	    QMessageBox::StandardButton answer = (QMessageBox::StandardButton) messageBox.exec();

	    if (answer != QMessageBox::Ok) {
			event->ignore();
		    return;
	    }
	}
	else if (!beforeClosing(true)) {
        event->ignore();
        return;
    }

	QSettings settings;
	settings.setValue(m_settingsPrefix + "state",saveState());
	settings.setValue(m_settingsPrefix + "geometry",saveGeometry());

	QMainWindow::closeEvent(event);
}

void PEMainWindow::initLockedFiles(bool) {
}

void PEMainWindow::initSketchWidgets()
{
    MainWindow::initSketchWidgets();

	m_iconGraphicsView = new IconSketchWidget(ViewLayer::IconView, this);
	initSketchWidget(m_iconGraphicsView);
	m_iconWidget = new SketchAreaWidget(m_iconGraphicsView,this);
	addTab(m_iconWidget, tr("Icon"));
    initSketchWidget(m_iconGraphicsView);

    m_docs.insert(m_breadboardGraphicsView->viewIdentifier(), &m_breadboardDocument);
    m_docs.insert(m_schematicGraphicsView->viewIdentifier(), &m_schematicDocument);
    m_docs.insert(m_pcbGraphicsView->viewIdentifier(), &m_pcbDocument);
    m_docs.insert(m_iconGraphicsView->viewIdentifier(), &m_iconDocument);

    m_sketchWidgets.insert(m_breadboardGraphicsView->viewIdentifier(), m_breadboardGraphicsView);
    m_sketchWidgets.insert(m_schematicGraphicsView->viewIdentifier(), m_schematicGraphicsView);
    m_sketchWidgets.insert(m_pcbGraphicsView->viewIdentifier(), m_pcbGraphicsView);
    m_sketchWidgets.insert(m_iconGraphicsView->viewIdentifier(), m_iconGraphicsView);
	
    foreach (SketchWidget * sketchWidget, m_sketchWidgets) {
        sketchWidget->setAcceptWheelEvents(false);
		sketchWidget->setChainDrag(false);				// no bendpoints
        m_svgChangeCount.insert(sketchWidget->viewIdentifier(), 0);
        m_everZoomed.insert(sketchWidget->viewIdentifier(), true);
		connect(sketchWidget, SIGNAL(newWireSignal(Wire *)), this, SLOT(newWireSlot(Wire *)));
    }

    m_metadataView = new PEMetadataView(this);
	SketchAreaWidget * sketchAreaWidget = new SketchAreaWidget(m_metadataView, this);
	addTab(sketchAreaWidget, tr("Metadata"));
    connect(m_metadataView, SIGNAL(metadataChanged(const QString &, const QString &)), this, SLOT(metadataChanged(const QString &, const QString &)), Qt::DirectConnection);
    connect(m_metadataView, SIGNAL(tagsChanged(const QStringList &)), this, SLOT(tagsChanged(const QStringList &)), Qt::DirectConnection);
    connect(m_metadataView, SIGNAL(propertiesChanged(const QHash<QString, QString> &)), this, SLOT(propertiesChanged(const QHash<QString, QString> &)), Qt::DirectConnection);

    m_connectorsView = new PEConnectorsView(this);
	sketchAreaWidget = new SketchAreaWidget(m_connectorsView, this);
	addTab(sketchAreaWidget, tr("Connectors"));
    connect(m_connectorsView, SIGNAL(connectorMetadataChanged(ConnectorMetadata *)), this, SLOT(connectorMetadataChanged(ConnectorMetadata *)), Qt::DirectConnection);
    connect(m_connectorsView, SIGNAL(removedConnectors(QList<ConnectorMetadata *> &)), this, SLOT(removedConnectors(QList<ConnectorMetadata *> &)), Qt::DirectConnection);
    connect(m_connectorsView, SIGNAL(connectorCountChanged(int)), this, SLOT(connectorCountChanged(int)), Qt::DirectConnection);
}

void PEMainWindow::initDock()
{
    //m_binManager = new BinManager(m_referenceModel, NULL, m_undoStack, this);
    //m_binManager->openBin(":/resources/bins/pe.fzb");
    //m_binManager->hideTabBar();
}

void PEMainWindow::moreInitDock()
{
    static int MinHeight = 75;
    static int DefaultHeight = 100;

    m_peSvgView = new PESvgView();
    makeDock(tr("SVG"), m_peSvgView, DockMinWidth, DockMinHeight);
    m_peSvgView->setMinimumSize(DockMinWidth, DockMinHeight);

    m_peToolView = new PEToolView();
    connect(m_peToolView, SIGNAL(getSpinAmount(double &)), this, SLOT(getSpinAmount(double &)), Qt::DirectConnection);
    connect(m_peToolView, SIGNAL(terminalPointChanged(const QString &)), this, SLOT(terminalPointChanged(const QString &)));
    connect(m_peToolView, SIGNAL(terminalPointChanged(const QString &, double)), this, SLOT(terminalPointChanged(const QString &, double)));
    connect(m_peToolView, SIGNAL(switchedConnector(const QDomElement &)), this, SLOT(switchedConnector(const QDomElement &)));
    connect(m_peToolView, SIGNAL(removedConnector(const QDomElement &)), this, SLOT(removedConnector(const QDomElement &)));
    connect(m_peToolView, SIGNAL(lockChanged(bool)), this, SLOT(lockChanged(bool)));
    connect(m_peToolView, SIGNAL(connectorMetadataChanged(ConnectorMetadata *)), this, SLOT(connectorMetadataChanged(ConnectorMetadata *)), Qt::DirectConnection);
    makeDock(tr("Connectors"), m_peToolView, DockMinWidth, DockMinHeight);
    m_peToolView->setMinimumSize(DockMinWidth, DockMinHeight);

	//QDockWidget * dockWidget = makeDock(BinManager::Title, m_binManager, MinHeight, DefaultHeight);
    //dockWidget->resize(0, 0);
}

void PEMainWindow::createActions()
{
    createFileMenuActions();
	m_openAct->setStatusTip(tr("Open a file to use as a part image."));
	disconnect(m_openAct, SIGNAL(triggered()), this, SLOT(mainLoad()));
	connect(m_openAct, SIGNAL(triggered()), this, SLOT(loadImage()));

	m_showInOSAct = new QAction(tr("Show in Folder"), this);
	m_showInOSAct->setStatusTip(tr("On the desktop, open the folder containing the current svg file."));
	connect(m_showInOSAct, SIGNAL(triggered()), this, SLOT(showInOS()));

    createEditMenuActions();

	m_deleteBusConnectionAct = new WireAction(tr("Remove Internal Connection"), this);
	connect(m_deleteBusConnectionAct, SIGNAL(triggered()), this, SLOT(deleteBusConnection()));

    createViewMenuActions();
    createHelpMenuActions();
    createWindowMenuActions();
}

void PEMainWindow::createMenus()
{
    createFileMenu();
    createEditMenu();
    createViewMenu();
    createWindowMenu();
    createHelpMenu();
}

QList<QWidget*> PEMainWindow::getButtonsForView(ViewLayer::ViewIdentifier) {
	QList<QWidget*> retval;
    return retval;
}

void PEMainWindow::connectPairs() {
    bool succeeded = true;
	succeeded =  succeeded && connect(qApp, SIGNAL(spaceBarIsPressedSignal(bool)), m_breadboardGraphicsView, SLOT(spaceBarIsPressedSlot(bool)));
	succeeded =  succeeded && connect(qApp, SIGNAL(spaceBarIsPressedSignal(bool)), m_schematicGraphicsView, SLOT(spaceBarIsPressedSlot(bool)));
	succeeded =  succeeded && connect(qApp, SIGNAL(spaceBarIsPressedSignal(bool)), m_pcbGraphicsView, SLOT(spaceBarIsPressedSlot(bool)));

	succeeded =  succeeded && connect(m_pcbGraphicsView, SIGNAL(cursorLocationSignal(double, double)), this, SLOT(cursorLocationSlot(double, double)));
	succeeded =  succeeded && connect(m_breadboardGraphicsView, SIGNAL(cursorLocationSignal(double, double)), this, SLOT(cursorLocationSlot(double, double)));
	succeeded =  succeeded && connect(m_schematicGraphicsView, SIGNAL(cursorLocationSignal(double, double)), this, SLOT(cursorLocationSlot(double, double)));

	connect(m_breadboardGraphicsView, SIGNAL(setActiveWireSignal(Wire *)), this, SLOT(setActiveWire(Wire *)));
	connect(m_schematicGraphicsView, SIGNAL(setActiveWireSignal(Wire *)), this, SLOT(setActiveWire(Wire *)));
	connect(m_pcbGraphicsView, SIGNAL(setActiveWireSignal(Wire *)), this, SLOT(setActiveWire(Wire *)));

}

QMenu *PEMainWindow::breadboardWireMenu() {
	QMenu *menu = new QMenu(QObject::tr("Internal Connections"), this);
	menu->addAction(m_deleteBusConnectionAct);
    connect( menu, SIGNAL(aboutToShow()), this, SLOT(updateWireMenu()));
	return menu;
}
    
QMenu *PEMainWindow::breadboardItemMenu() {
    return NULL;
}

QMenu *PEMainWindow::schematicWireMenu() {
    return breadboardWireMenu();
}
    
QMenu *PEMainWindow::schematicItemMenu() {
    return NULL;
}

QMenu *PEMainWindow::pcbWireMenu() {
    return breadboardWireMenu();
}
    
QMenu *PEMainWindow::pcbItemMenu() {
    return NULL;
}

void PEMainWindow::setInitialItem(PaletteItem * paletteItem) {

    ModelPart * originalModelPart = NULL;
    if (paletteItem == NULL) {
        // this shouldn't happen
        originalModelPart = m_referenceModel->retrieveModelPart("generic_ic_dip_8_300mil");
    }
    else {
        originalModelPart = paletteItem->modelPart();
    }

    m_originalFzpPath = originalModelPart->path();
    m_originalModuleID = originalModelPart->moduleID();

    QFileInfo info(originalModelPart->path());
    //DebugDialog::debug(QString("%1, %2").arg(info.absoluteFilePath()).arg(m_userPartsFolderPath));
    m_canSave = info.absoluteFilePath().contains(m_userPartsFolderPath);

    if (!loadFzp(originalModelPart->path())) return;

    QDomElement fzpRoot = m_fzpDocument.documentElement();
    QString referenceFile = fzpRoot.attribute(ReferenceFileString);
    if (referenceFile.isEmpty()) {
        fzpRoot.setAttribute(ReferenceFileString, info.fileName());
    }
    QDomElement views = fzpRoot.firstChildElement("views");
    QDomElement author = fzpRoot.firstChildElement("author");
    if (author.isNull()) {
        author = m_fzpDocument.createElement("author");
        fzpRoot.appendChild(author);
    }
    if (author.text().isEmpty()) {
        TextUtils::replaceChildText(m_fzpDocument, author, QString(getenvUser()));
    }
    QDomElement date = fzpRoot.firstChildElement("date");
    if (date.isNull()) {
        date = m_fzpDocument.createElement("date");
        fzpRoot.appendChild(date);
    }
    TextUtils::replaceChildText(m_fzpDocument, date, QDate::currentDate().toString());

    fzpRoot.setAttribute("moduleId", m_guid);

	QDomElement properties = fzpRoot.firstChildElement("properties");
	if (properties.isNull()) {
		properties = m_fzpDocument.createElement("properties");
		fzpRoot.appendChild(properties);
	}
	QHash<QString,QString> props = originalModelPart->properties();
	foreach (QString key, props.keys()) {
		replaceProperty(key, props.value(key), properties);
	}
	// record "local" properties
	foreach (QByteArray byteArray, originalModelPart->dynamicPropertyNames()) {
		replaceProperty(byteArray, originalModelPart->property(byteArray).toString(), properties);
	}

	// for now kill editable pin labels, otherwise the saved part will try to use the labels that are only found in the sketch
	QDomElement epl = TextUtils::findElementWithAttribute(properties, "name", "editable pin labels");
	if (!epl.isNull()) {
		TextUtils::replaceChildText(m_fzpDocument, epl, "false");
	}

	QDomElement family = TextUtils::findElementWithAttribute(properties, "name", "family");
	if (family.isNull()) {
		replaceProperty("family", m_guid, properties);
	}
	QDomElement variant = TextUtils::findElementWithAttribute(properties, "name", "variant");
	if (variant.isNull()) {
		QStringList variants = m_referenceModel->propValues(family.text(), "variant", true);
		int theMax = std::numeric_limits<int>::max(); 
		QString candidate;
		for (int i = 1; i < theMax; i++) {
			candidate = QString("variant %1").arg(i);
			if (!variants.contains(candidate, Qt::CaseInsensitive)) break;
		}
		replaceProperty("variant", candidate, properties);
	}

	// make sure local props are copied

    foreach (SketchWidget * sketchWidget, m_sketchWidgets) {
        ItemBase * itemBase = originalModelPart->viewItem(sketchWidget->viewIdentifier());
        if (itemBase == NULL) continue;
        if (!itemBase->hasCustomSVG()) continue;

        QHash<QString, QString> svgHash;
        QString svg = "";
		foreach (ViewLayer * vl, sketchWidget->viewLayers().values()) {
			svg += itemBase->retrieveSvg(vl->viewLayerID(), svgHash, false, GraphicsUtils::StandardFritzingDPI);
		}

        if (svg.isEmpty()) {
            DebugDialog::debug(QString("pe: missing custom svg %1").arg(originalModelPart->moduleID()));
            continue;
        }

        QString referenceFile = getSvgReferenceFile(itemBase->filename());
		QSizeF size = itemBase->size();
        QString header = TextUtils::makeSVGHeader(GraphicsUtils::SVGDPI, GraphicsUtils::StandardFritzingDPI, size.width(), size.height());
        header += makeDesc(referenceFile);
		svg = header + svg + "</svg>";
        QString svgPath = makeSvgPath(referenceFile, sketchWidget, true);
        bool result = writeXml(m_userPartsFolderSvgPath + svgPath, svg, true);
        if (!result) {
            QMessageBox::critical(NULL, tr("Parts Editor"), tr("Unable to write svg to  %1").arg(svgPath));
		    return;
        }

        QDomElement view = views.firstChildElement(ViewLayer::viewIdentifierXmlName(sketchWidget->viewIdentifier()));
        QDomElement layers = view.firstChildElement("layers");
        if (layers.isNull()) {
            QMessageBox::critical(NULL, tr("Parts Editor"), tr("Unable to parse fzp file  %1").arg(originalModelPart->path()));
		    return;
        }

        layers.setAttribute("image", svgPath);
    }

    reload(true);

    foreach (ItemBase * itemBase, m_items.values()) {
        m_originalSvgPaths.insert(itemBase->viewIdentifier(), itemBase->filename());
    }

    setTitle();
    createRaiseWindowActions();

}

void PEMainWindow::initHelper()
{
}

void PEMainWindow::initZoom() {
    if (m_currentGraphicsView) {
        if (!m_everZoomed.value(m_currentGraphicsView->viewIdentifier())) {
            m_everZoomed.insert(m_currentGraphicsView->viewIdentifier(), true);
            m_currentGraphicsView->fitInWindow();
        }
    }
}

void PEMainWindow::setTitle() {
    QString title = tr("Fritzing (New) Parts Editor");
    QString partTitle = getPartTitle();

    QString viewName;
    if (m_currentGraphicsView) viewName = m_currentGraphicsView->viewName();
    else if (currentTabIndex() == IconViewIndex) viewName = tr("Icon View");
    else if (currentTabIndex() == MetadataViewIndex) viewName = tr("Metadata View");
    else if (currentTabIndex() == ConnectorsViewIndex) viewName = tr("Connectors View");

	setWindowTitle(QString("%1: %2 [%3]%4").arg(title).arg(partTitle).arg(viewName).arg(QtFunkyPlaceholder));
}

void PEMainWindow::createViewMenuActions() {
    MainWindow::createViewMenuActions();

	m_showIconAct = new QAction(tr("Show Icon"), this);
	m_showIconAct->setShortcut(tr("Ctrl+4"));
	m_showIconAct->setStatusTip(tr("Show the icon view"));
	connect(m_showIconAct, SIGNAL(triggered()), this, SLOT(showIconView()));

	m_showMetadataViewAct = new QAction(tr("Show Metadata"), this);
	m_showMetadataViewAct->setShortcut(tr("Ctrl+5"));
	m_showMetadataViewAct->setStatusTip(tr("Show the metadata view"));
	connect(m_showMetadataViewAct, SIGNAL(triggered()), this, SLOT(showMetadataView()));

    m_showConnectorsViewAct = new QAction(tr("Show Connectors"), this);
	m_showConnectorsViewAct->setShortcut(tr("Ctrl+6"));
	m_showConnectorsViewAct->setStatusTip(tr("Show the connector metadata in a list view"));
	connect(m_showConnectorsViewAct, SIGNAL(triggered()), this, SLOT(showConnectorsView()));

}

void PEMainWindow::createViewMenu() {
    MainWindow::createViewMenu();

    bool afterNext = false;
    foreach (QAction * action, m_viewMenu->actions()) {
        if (action == m_showPCBAct) {
            afterNext = true;
        }
        else if (afterNext) {
            m_viewMenu->insertAction(action, m_showIconAct);
            m_viewMenu->insertAction(action, m_showMetadataViewAct);
            m_viewMenu->insertAction(action, m_showConnectorsViewAct);
            break;
        }
    }
}

void PEMainWindow::showMetadataView() {
    setCurrentTabIndex(MetadataViewIndex);
}

void PEMainWindow::showConnectorsView() {
    setCurrentTabIndex(ConnectorsViewIndex);
}

void PEMainWindow::showIconView() {
    setCurrentTabIndex(IconViewIndex);
}

void PEMainWindow::changeSpecialProperty(const QString & name, const QString & value)
{
    QHash<QString, QString> oldProperties = getOldProperties();

	if (value.isEmpty()) {
		QMessageBox::warning(NULL, tr("Blank not allowed"), tr("The value of '%1' can not be blank.").arg(name));
		m_metadataView->resetProperty(name, value);
		return;
	}

    QHash<QString, QString> newProperties(oldProperties);
    newProperties.insert(name, value);
    
    ChangePropertiesCommand * cpc = new ChangePropertiesCommand(this, oldProperties, newProperties, NULL);
    cpc->setText(tr("Change %1 to %2").arg(name).arg(value));
    cpc->setSkipFirstRedo();
    changeProperties(newProperties, false);
    m_undoStack->waitPush(cpc, SketchWidget::PropChangeDelay);
}

void PEMainWindow::metadataChanged(const QString & name, const QString & value)
{
	qDebug() << "metadata changed";

	if (name.compare("family") == 0) {
		changeSpecialProperty(name, value);
        return;
    }

    if (name.compare("variant") == 0) {
		QString family = m_metadataView->family();
		QStringList variants = m_referenceModel->propValues(family, "variant", true);
		if (variants.contains(value, Qt::CaseInsensitive)) {
			QMessageBox::warning(NULL, tr("Must be unique"), tr("Variant '%1' is in use. The variant name must be unique.").arg(value));
			return;
		}

		changeSpecialProperty(name, value);
        return;
    }

    QString menuText = (name.compare("description") == 0) ? tr("Change description") : tr("Change %1 to '%2'").arg(name).arg(value);

    // called from metadataView
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement element = root.firstChildElement(name);
    QString oldValue = element.text();
	if (oldValue == value) return;

    ChangeMetadataCommand * cmc = new ChangeMetadataCommand(this, name, oldValue, value, NULL);
    cmc->setText(menuText);
    cmc->setSkipFirstRedo();
    changeMetadata(name, value, false);
    m_undoStack->waitPush(cmc, SketchWidget::PropChangeDelay);
}

void PEMainWindow::changeMetadata(const QString & name, const QString & value, bool updateDisplay)
{
    // called from command object
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement element = root.firstChildElement(name);
    QString oldValue = element.text();
    TextUtils::replaceChildText(m_fzpDocument, element, value);

    if (updateDisplay) {
        m_metadataView->initMetadata(m_fzpDocument);
    }
}

void PEMainWindow::tagsChanged(const QStringList & newTags)
{

    // called from metadataView
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement tags = root.firstChildElement("tags");
    QDomElement tag = tags.firstChildElement("tag");
    QStringList oldTags;
    while (!tag.isNull()) {
        oldTags << tag.text();
        tag = tag.nextSiblingElement("tag");
    }

    ChangeTagsCommand * ctc = new ChangeTagsCommand(this, oldTags, newTags, NULL);
    ctc->setText(tr("Change tags"));
    ctc->setSkipFirstRedo();
    changeTags(newTags, false);
    m_undoStack->waitPush(ctc, SketchWidget::PropChangeDelay);
}

void PEMainWindow::changeTags(const QStringList & newTags, bool updateDisplay)
{
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement tags = root.firstChildElement("tags");
    QDomElement tag = tags.firstChildElement("tag");
    while (!tag.isNull()) {
        tags.removeChild(tag);
        tag = tags.firstChildElement("tag");
    }

    foreach (QString newTag, newTags) {
        QDomElement tag = m_fzpDocument.createElement("tag");
        tags.appendChild(tag);
        TextUtils::replaceChildText(m_fzpDocument, tag, newTag);
    }

    if (updateDisplay) {
        m_metadataView->initMetadata(m_fzpDocument);
    }
}

void PEMainWindow::propertiesChanged(const QHash<QString, QString> & newProperties)
{	
	qDebug() << "properties changed";

	QStringList keys = newProperties.keys();
	if (keys.contains("family", Qt::CaseInsensitive)) {
		QMessageBox::warning(NULL, tr("Duplicate problem"), tr("Duplicate 'family' property not allowed"));
		return;
	}

	if (keys.contains("variant", Qt::CaseInsensitive)) {
		QMessageBox::warning(NULL, tr("Duplicate problem"), tr("Duplicate 'variant' property not allowed"));
		return;
	}

    // called from metadataView
    QHash<QString, QString> oldProperties = getOldProperties();

    ChangePropertiesCommand * cpc = new ChangePropertiesCommand(this, oldProperties, newProperties, NULL);
    cpc->setText(tr("Change properties"));
    cpc->setSkipFirstRedo();
    changeProperties(newProperties, false);
    m_undoStack->waitPush(cpc, SketchWidget::PropChangeDelay);
}


void PEMainWindow::changeProperties(const QHash<QString, QString> & newProperties, bool updateDisplay)
{
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement properties = root.firstChildElement("properties");
    QDomElement prop = properties.firstChildElement("property");
    while (!prop.isNull()) {
        properties.removeChild(prop);
        prop = properties.firstChildElement("property");
    }

    foreach (QString name, newProperties.keys()) {
        QDomElement prop = m_fzpDocument.createElement("property");
        properties.appendChild(prop);
        prop.setAttribute("name", name);
        TextUtils::replaceChildText(m_fzpDocument, prop, newProperties.value(name));
    }

    if (updateDisplay) {
        m_metadataView->initMetadata(m_fzpDocument);
    }
}

QHash<QString, QString> PEMainWindow::getOldProperties() 
{
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement properties = root.firstChildElement("properties");
    QDomElement prop = properties.firstChildElement("property");
    QHash<QString, QString> oldProperties;
    while (!prop.isNull()) {
        QString name = prop.attribute("name");
        QString value = prop.text();
        oldProperties.insert(name, value);
        prop = prop.nextSiblingElement("property");
    }

    return oldProperties;
}

void PEMainWindow::connectorMetadataChanged(ConnectorMetadata * cmd)
{
    ConnectorMetadata oldcmd;

    int index;
    QDomElement connector = findConnector(cmd->connectorID, index);
    if (connector.isNull()) return;

    oldcmd.connectorID = connector.attribute("id");
    oldcmd.connectorType = Connector::Male;
    if (connector.attribute("type").compare("female", Qt::CaseInsensitive) == 0) oldcmd.connectorType = Connector::Female;
    else if (connector.attribute("type").compare("pad", Qt::CaseInsensitive) == 0) oldcmd.connectorType = Connector::Pad;
    oldcmd.connectorName = connector.attribute("name");
    QDomElement description = connector.firstChildElement("description");
    oldcmd.connectorDescription = description.text();

    ChangeConnectorMetadataCommand * ccmc = new ChangeConnectorMetadataCommand(this, &oldcmd, cmd, NULL);
    ccmc->setText(tr("Change connector %1").arg(cmd->connectorName));
    ccmc->setSkipFirstRedo();
    changeConnectorElement(connector, cmd);
    m_undoStack->waitPush(ccmc, SketchWidget::PropChangeDelay);
}

QDomElement PEMainWindow::findConnector(const QString & id, int & index) 
{
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement connectors = root.firstChildElement("connectors");
    QDomElement connector = connectors.firstChildElement("connector");
    index = 0;
    while (!connector.isNull()) {
        if (id.compare(connector.attribute("id")) == 0) {
            return connector;
        }
        connector = connector.nextSiblingElement("connector");
        index++;
    }

    return QDomElement();
}


void PEMainWindow::changeConnectorMetadata(ConnectorMetadata * cmd, bool updateDisplay) {
    int index;
    QDomElement connector = findConnector(cmd->connectorID, index);
    if (connector.isNull()) return;

    changeConnectorElement(connector, cmd);
    if (updateDisplay) {
        initConnectors();
    }
}

void PEMainWindow::changeConnectorElement(QDomElement & connector, ConnectorMetadata * cmd)
{
    connector.setAttribute("name", cmd->connectorName);
    connector.setAttribute("id", cmd->connectorID);
    QString type = "male";
    if (cmd->connectorType == Connector::Female) type = "female";
    else if (cmd->connectorType == Connector::Pad) type = "pad";
    connector.setAttribute("type", type);
    TextUtils::replaceElementChildText(m_fzpDocument, connector, "description", cmd->connectorDescription);

#ifndef QT_NO_DEBUG
    QDomElement description = connector.firstChildElement("description");
    QString text = description.text();
    DebugDialog::debug(QString("description %1").arg(text));
#endif

}

void PEMainWindow::initSvgTree(ItemBase * itemBase, QDomDocument & domDocument) 
{
    QString errorStr;
    int errorLine;
    int errorColumn;

    QDomDocument doc;
    QFile file(itemBase->filename());
    if (!doc.setContent(&file, true, &errorStr, &errorLine, &errorColumn)) {
		DebugDialog::debug(QString("unable to parse svg: %1 %2 %3").arg(errorStr).arg(errorLine).arg(errorColumn));
        return;
	}


    int z = PegiZ;


    FSvgRenderer tempRenderer;
    QByteArray rendered = tempRenderer.loadSvg(doc.toByteArray(), "", false);
    // cleans up the svg
    if (!domDocument.setContent(rendered, true, &errorStr, &errorLine, &errorColumn)) {
		DebugDialog::debug(QString("unable to parse svg (2): %1 %2 %3").arg(errorStr).arg(errorLine).arg(errorColumn));
        return;
	}

    TextUtils::gornTree(domDocument);  
    FSvgRenderer renderer;
    renderer.loadSvg(domDocument.toByteArray(), "", false);

    QList<QDomElement> traverse;
    traverse << domDocument.documentElement();
    while (traverse.count() > 0) {
        ZList.insert(itemBase->viewIdentifier(), z++);
        QList<QDomElement> next;
        foreach (QDomElement element, traverse) {
            bool isG = false;
            bool isSvg = false;
            QString tagName = element.tagName();
            if      (tagName.compare("rect") == 0);
            else if (tagName.compare("g") == 0) {
                isG = true;
            }
            else if (tagName.compare("svg") == 0) {
                isSvg = true;
            }
            else if (tagName.compare("circle") == 0);
            else if (tagName.compare("ellipse") == 0);
            else if (tagName.compare("path") == 0);
            else if (tagName.compare("line") == 0);
            else if (tagName.compare("polyline") == 0);
            else if (tagName.compare("polygon") == 0);
            else if (tagName.compare("text") == 0);
            else continue;

            QRectF bounds = getPixelBounds(renderer, element);
            if (isSvg) {                
                bounds.setWidth(0);             // skip top level element
            }
            else if (isG) {
                // skip if it's too high up in the hierarchy?
            }

            // known Qt bug: boundsOnElement returns zero width and height for text elements.
            if (bounds.width() > 0 && bounds.height() > 0) {
                makePegi(bounds.size(), bounds.topLeft(), itemBase, element);

                /* 
                QString string;
                QTextStream stream(&string);
                element.save(stream, 0);
                DebugDialog::debug("........");
                DebugDialog::debug(string);
                */
            }

            QDomElement child = element.firstChildElement();
            while (!child.isNull()) {
                next.append(child);
                child = child.nextSiblingElement();
            }
        }
        traverse.clear();
        foreach (QDomElement element, next) traverse.append(element);
        next.clear();
    }
}

void PEMainWindow::highlightSlot(PEGraphicsItem * pegi) {
    if (m_peToolView) {
        m_peToolView->highlightElement(pegi);
    }
    if (m_peSvgView) {
        m_peSvgView->highlightElement(pegi);
    }
}

void PEMainWindow::initConnectors() {
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement connectors = root.firstChildElement("connectors");
    QDomElement connector = connectors.firstChildElement("connector");
    QList<QDomElement> connectorList;
    while (!connector.isNull()) {
        connectorList.append(connector);
        connector = connector.nextSiblingElement("connector");
    }

    qSort(connectorList.begin(), connectorList.end(), byID);

    m_connectorsView->initConnectors(connectorList);
    m_peToolView->initConnectors(connectorList);
}

void PEMainWindow::switchedConnector(const QDomElement & element)
{
    if (m_currentGraphicsView == NULL) return;
    if (element.isNull()) return;

    switchedConnector(element, m_currentGraphicsView, true);
}

void PEMainWindow::switchedConnector(const QDomElement & element, SketchWidget * sketchWidget, bool lock)
{
    QString id, terminalID;
    if (!getConnectorIDs(element, sketchWidget, id, terminalID)) return;

    QList<PEGraphicsItem *> pegiList;
    foreach (QGraphicsItem * item, sketchWidget->scene()->items()) {
        PEGraphicsItem * pegi = dynamic_cast<PEGraphicsItem *>(item);
        if (pegi) pegiList.append(pegi);
    }

    PEGraphicsItem * terminalItem = NULL;
    if (!terminalID.isEmpty()) {
        foreach (PEGraphicsItem * pegi, pegiList) {
            QDomElement pegiElement = pegi->element();
            if (pegiElement.attribute("id").compare(terminalID) == 0) {
                terminalItem = pegi;
                break;
            }
        }
    }

    bool gotOne = false;
    foreach (PEGraphicsItem * pegi, pegiList) {
        QDomElement pegiElement = pegi->element();
        if (pegiElement.attribute("id").compare(id) == 0) {
            QPointF terminalPoint = pegi->rect().center();
            if (terminalItem) {
                terminalPoint = terminalItem->pos() - pegi->pos() + terminalItem->rect().center();
            }
            pegi->setTerminalPoint(terminalPoint);
            m_peToolView->setTerminalPointCoords(terminalPoint);
            m_peToolView->setTerminalPointLimits(pegi->rect().size());
            gotOne = true;
            pegi->showTerminalPoint(true);
            pegi->setHighlighted(true);
            break;
        }
    }

    if (lock) {
        m_peToolView->setLock(gotOne);
        lockChangedAux(gotOne, pegiList);
    }
    if (!gotOne) {
        foreach (PEGraphicsItem * pegi, pegiList) {
            pegi->showTerminalPoint(false);
            pegi->setHighlighted(false);        
        }
    }
}

void PEMainWindow::loadImage() 
{
    if (m_currentGraphicsView == NULL) return;

	QStringList extras;
	extras.append("");
	extras.append("");
	QString imageFiles;
	if (m_currentGraphicsView->viewIdentifier() == ViewLayer::PCBView) {
		imageFiles = tr("Image & Footprint Files (%1 %2 %3 %4 %5);;SVG Files (%1);;JPEG Files (%2);;PNG Files (%3);;gEDA Footprint Files (%4);;Kicad Module Files (%5)");   // 
		extras[0] = "*.fp";
		extras[1] = "*.mod";
	}
	else {
		imageFiles = tr("Image Files (%1 %2 %3);;SVG Files (%1);;JPEG Files (%2);;PNG Files (%3)%4%5");
	}

    /*
	if (m_currentGraphicsView->viewIdentifier() == ViewLayer::SchematicView) {
		extras[0] = "*.lib";
		imageFiles = tr("Image & Footprint Files (%1 %2 %3 %4);;SVG Files (%1);;JPEG Files (%2);;PNG Files (%3);;Kicad Schematic Files (%4)%5");   // 
	}
    */

    QString initialPath = FolderUtils::openSaveFolder();
    ItemBase * itemBase = m_items.value(m_currentGraphicsView->viewIdentifier(), NULL);
    if (itemBase) {
        initialPath = itemBase->filename();
    }
	QString origPath = FolderUtils::getOpenFileName(this,
		tr("Open Image"),
		initialPath,
		imageFiles.arg("*.svg").arg("*.jpg *.jpeg").arg("*.png").arg(extras[0]).arg(extras[1])
	);

	if (origPath.isEmpty()) {
		return; // Cancel pressed
	} 

    bool saveReferenceFile = true;
    QString newPath = origPath;
    QString referenceFile = QFileInfo(origPath).fileName();
	if (newPath.endsWith(".svg")) {
		QFile origFile(origPath);
		if (!origFile.open(QFile::ReadOnly)) {
    		QMessageBox::warning(NULL, tr("Conversion problem"), tr("Unable to load '%1'").arg(origPath));
			return;
		}

		QString svg = origFile.readAll();
		origFile.close();
		if (svg.contains("coreldraw", Qt::CaseInsensitive) && svg.contains("cdata", Qt::CaseInsensitive)) {
    		QMessageBox::warning(NULL, tr("Conversion problem"), 
				tr("The SVG file '%1' appears to have been exported from CorelDRAW without the 'presentation attributes' setting. ").arg(origPath) +
				tr("Please re-export the SVG file using that setting, and try loading again.")
			);
			return;
		}

		if (newPath.contains(m_userPartsFolderSvgPath)) saveReferenceFile = false;
        else if (newPath.contains(FolderUtils::getApplicationSubFolderPath("parts"))) saveReferenceFile = false;

		QStringList availFonts = InstalledFonts::InstalledFontsList.toList();
		if (availFonts.count() > 0) {
			QString destFont = availFonts.at(0);
			foreach (QString f, availFonts) {
				if (f.contains("droid", Qt::CaseInsensitive)) {
					destFont = f;
					break;
				}
			}
			if (TextUtils::fixFonts(svg, destFont)) {
    			QMessageBox::information(NULL, tr("Fonts"), 
					tr("Fritzing currently only supports OCRA and Droid fonts--these have been substituted in for the fonts in '%1'").arg(origPath));
				saveReferenceFile = true;
			}
		}
		if (saveReferenceFile) {
            // from outside the Fritzing ecosystem or we needed to fix the fonts
            newPath = m_userPartsFolderSvgPath + makeSvgPath(referenceFile, m_currentGraphicsView, true);
            bool success = TextUtils::writeUtf8(newPath, svg);
            if (!success) {
    		    QMessageBox::warning(NULL, tr("Copy problem"), tr("Unable to make a local copy of: '%1'").arg(origPath));
				return;
            }
        }
    }
    else {
        if (origPath.endsWith("png") || origPath.endsWith("jpg") || origPath.endsWith("jpeg")) {
                QString message = tr("You may use a PNG or JPG image to construct your part, but it is better to use an SVG. ") +
                                tr("PNG and JPG images retain their nature as bitmaps and do not look good when scaled--") +                        
                                tr("so for Fritzing parts it is best to use PNG and JPG only as placeholders.")                       
                 ;
                
    		    QMessageBox::information(NULL, tr("Use of PNG and JPG discouraged"), message);

        }

		try {
			newPath = createSvgFromImage(origPath);
		}
		catch (const QString & msg) {
    		QMessageBox::warning(NULL, tr("Conversion problem"), tr("Unable to load image file '%1': \n\n%2").arg(newPath).arg(msg));
			return;
		}
	}

	if (!newPath.isEmpty()) {
        QFile file(newPath);
	    QString errorStr;
	    int errorLine;
	    int errorColumn;
        QDomDocument doc;
	    bool result = doc.setContent(&file, &errorStr, &errorLine, &errorColumn);
        if (!result) {
    		QMessageBox::warning(NULL, tr("SVG problem"), tr("Unable to parse '%1': %2 line:%3 column:%4").arg(origPath).arg(errorStr).arg(errorLine).arg(errorColumn));
            return;
        }

        if (m_currentGraphicsView == m_pcbGraphicsView) {
            QDomElement root = doc.documentElement();
            QDomElement check = TextUtils::findElementWithAttribute(root, "id", "copper1");
            if (check.isNull()) {
                check = TextUtils::findElementWithAttribute(root, "id", "copper0");
            }
            if (check.isNull()) {
                QString message = tr("There are no copper layers defined in: %1. ").arg(origPath) +
                                tr("See <a href=\"http://fritzing.org/learning/tutorials/creating-custom-parts/providing-part-graphics/\">this explanation</a>.") +
                                tr("<br/><br/>This will not be a problem in the next release of the Parts Editor, ") +
                                tr("but for now please modify the file according to the instructions in the link.")                         
                 ;
                
    		    QMessageBox::warning(NULL, tr("SVG problem"), message);
                return;
            }
        }

        if (saveReferenceFile) {
            QDomElement root = doc.documentElement();
            QDomElement desc = root.firstChildElement("desc");
            if (desc.isNull()) {
                desc = doc.createElement("desc");
                root.appendChild(desc);
            }
            QDomElement referenceFile = desc.firstChildElement(ReferenceFileString);
            if (referenceFile.isNull()) {
                referenceFile = doc.createElement(ReferenceFileString);
                desc.appendChild(referenceFile);
            }
            TextUtils::replaceChildText(doc, desc, QFileInfo(origPath).fileName());
            writeXml(newPath, doc.toString(), true);
        }

		ChangeSvgCommand * csc = new ChangeSvgCommand(this, m_currentGraphicsView, itemBase->filename(), newPath, m_originalSvgPaths.value(itemBase->viewIdentifier()), newPath, NULL);
        QFileInfo info(origPath);
        csc->setText(QString("Load '%1'").arg(info.fileName()));
        m_undoStack->waitPush(csc, SketchWidget::PropChangeDelay);
	}
}

QString PEMainWindow::createSvgFromImage(const QString &origFilePath) {
    QString referenceFile = QFileInfo(origFilePath).fileName();
	QString newFilePath = m_userPartsFolderSvgPath + makeSvgPath(referenceFile, m_currentGraphicsView, true);
	if (origFilePath.endsWith(".fp")) {
		// this is a geda footprint file
		GedaElement2Svg geda;
		QString svg = geda.convert(origFilePath, false);
		return saveSvg(svg, newFilePath);
	}

	if (origFilePath.endsWith(".lib")) {
		// Kicad schematic library file
		QStringList defs = KicadSchematic2Svg::listDefs(origFilePath);
		if (defs.count() == 0) {
			throw tr("no schematics found in %1").arg(origFilePath);
		}

		QString def;
		if (defs.count() > 1) {
			KicadModuleDialog kmd(tr("schematic part"), origFilePath, defs, this);
			int result = kmd.exec();
			if (result != QDialog::Accepted) {
				return "";
			}

			def = kmd.selectedModule();
		}
		else {
			def = defs.at(0);
		}

		KicadSchematic2Svg kicad;
		QString svg = kicad.convert(origFilePath, def);
		return saveSvg(svg, newFilePath);
	}

	if (origFilePath.endsWith(".mod")) {
		// Kicad footprint (Module) library file
		QStringList modules = KicadModule2Svg::listModules(origFilePath);
		if (modules.count() == 0) {
			throw tr("no footprints found in %1").arg(origFilePath);
		}

		QString module;
		if (modules.count() > 1) {
			KicadModuleDialog kmd("footprint", origFilePath, modules, this);
			int result = kmd.exec();
			if (result != QDialog::Accepted) {
				return "";
			}

			module = kmd.selectedModule();
		}
		else {
			module = modules.at(0);
		}

		KicadModule2Svg kicad;
		QString svg = kicad.convert(origFilePath, module, false);
		return saveSvg(svg, newFilePath);
	}

	// deal with png, jpg:
	QImage img(origFilePath);
	QSvgGenerator svgGenerator;
	svgGenerator.setResolution(90);
	svgGenerator.setFileName(newFilePath);
	QSize sz = img.size();
    svgGenerator.setSize(sz);
	svgGenerator.setViewBox(QRect(0, 0, sz.width(), sz.height()));
	QPainter svgPainter(&svgGenerator);
	svgPainter.drawImage(QPoint(0,0), img);
	svgPainter.end();

	return newFilePath;
}

QString PEMainWindow::makeSvgPath(const QString & referenceFile, SketchWidget * sketchWidget, bool useIndex)
{
    QString rf = referenceFile;
    if (!rf.isEmpty()) rf += "_";
    QString viewName = ViewLayer::viewIdentifierNaturalName(sketchWidget->viewIdentifier());
    QString indexString;
    if (useIndex) indexString = QString("_%3").arg(m_fileIndex++);
    return QString("%1/%2%3_%1%4.svg").arg(viewName).arg(rf).arg(m_guid).arg(indexString);
}

QString PEMainWindow::saveSvg(const QString & svg, const QString & newFilePath) {
    if (!writeXml(newFilePath, svg, true)) {
        throw tr("unable to open temp file %1").arg(newFilePath);
    }
	return newFilePath;
}

void PEMainWindow::changeSvg(SketchWidget * sketchWidget, const QString & filename, const QString & originalPath, int changeDirection) {
    QDomElement fzpRoot = m_fzpDocument.documentElement();
    QDomElement views = fzpRoot.firstChildElement("views");
    QDomElement view = views.firstChildElement(ViewLayer::viewIdentifierXmlName(sketchWidget->viewIdentifier()));
    QDomElement layers = view.firstChildElement("layers");
    QFileInfo info(filename);
    QDir dir = info.absoluteDir();
    QString shortName = dir.dirName() + "/" + info.fileName();
    layers.setAttribute("image", shortName);

    foreach (QGraphicsItem * item, sketchWidget->scene()->items()) {
        PEGraphicsItem * pegi = dynamic_cast<PEGraphicsItem *>(item);
        if (pegi) delete pegi;
    }

    foreach (ItemBase * itemBase, m_items.values()) {
        foreach(ItemBase * lk, itemBase->layerKin()) {
            delete lk;
        }
        delete itemBase;
    }
    m_items.clear();

    updateChangeCount(sketchWidget, changeDirection);

    reload(false);

    this->m_originalSvgPaths.insert(sketchWidget->viewIdentifier(), originalPath);
}

QString PEMainWindow::saveFzp() {
    QDir dir = QDir::temp();
    dir.mkdir(m_guid);
    dir.cd(m_guid);
    QString fzpPath = dir.absoluteFilePath(QString("%1%2_%3.fzp").arg(getFzpReferenceFile()).arg(m_guid).arg(m_fileIndex++));   
    DebugDialog::debug("temp path " + fzpPath);
    writeXml(fzpPath, m_fzpDocument.toString(), true);
    return fzpPath;
}

void PEMainWindow::reload(bool firstTime) {
	Q_UNUSED(firstTime);

    QString fzpPath = saveFzp();   // needs a document somewhere to set up connectors--not part of the undo stack
    ModelPart * modelPart = new ModelPart(m_fzpDocument, fzpPath, ModelPart::Part);

    long newID = ItemBase::getNextID();
	ViewGeometry viewGeometry;
	viewGeometry.setLoc(QPointF(0, 0));

    ItemBase * iconItem = m_iconGraphicsView->addItem(modelPart, m_iconGraphicsView->defaultViewLayerSpec(), BaseCommand::SingleView, viewGeometry, newID, -1, NULL);
    ItemBase * breadboardItem = m_breadboardGraphicsView->addItem(modelPart, m_breadboardGraphicsView->defaultViewLayerSpec(), BaseCommand::SingleView, viewGeometry, newID, -1, NULL);
    ItemBase * schematicItem = m_schematicGraphicsView->addItem(modelPart, m_schematicGraphicsView->defaultViewLayerSpec(), BaseCommand::SingleView, viewGeometry, newID, -1, NULL);
    ItemBase * pcbItem = m_pcbGraphicsView->addItem(modelPart, m_pcbGraphicsView->defaultViewLayerSpec(), BaseCommand::SingleView, viewGeometry, newID, -1, NULL);
    
	// make sure m_fzpDocument has a variant property
	
	m_metadataView->initMetadata(m_fzpDocument);

    initConnectors();

	QList<QWidget *> widgets;
	widgets << m_metadataView << m_peToolView << m_connectorsView;
	foreach (QWidget * widget, widgets) {
		QList<QLineEdit *> lineEdits = widget->findChildren<QLineEdit *>();
		foreach (QLineEdit * lineEdit, lineEdits) {
			lineEdit->installEventFilter(this);
		}
		QList<QTextEdit *> textEdits = widget->findChildren<QTextEdit *>();
		foreach (QTextEdit * textEdit, textEdits) {
			textEdit->installEventFilter(this);
		}
	}

    initSvgTree(breadboardItem, m_breadboardDocument);
    initSvgTree(schematicItem, m_schematicDocument);
    initSvgTree(pcbItem, m_pcbDocument);
    initSvgTree(iconItem, m_iconDocument);  // TODO: full svg tree not necessary here, but need the icon document

    m_items.insert(m_iconGraphicsView->viewIdentifier(), iconItem);
    m_items.insert(m_breadboardGraphicsView->viewIdentifier(), breadboardItem);
    m_items.insert(m_schematicGraphicsView->viewIdentifier(), schematicItem);
    m_items.insert(m_pcbGraphicsView->viewIdentifier(), pcbItem);

    foreach (ItemBase * itemBase, m_items.values()) {
        // TODO: may have to revisit this and move all pegi items
        itemBase->setMoveLock(true);
		itemBase->setAcceptsMousePressLegEvent(false);
    }

    foreach (SketchWidget * sketchWidget, m_sketchWidgets.values()) {
        sketchWidget->hideConnectors(true);
        m_everZoomed.insert(sketchWidget->viewIdentifier(), false);
    }

    switchedConnector(m_peToolView->currentConnector());

	displayBuses();

    // processeventblocker might be enough
    QTimer::singleShot(10, this, SLOT(initZoom()));
}

void PEMainWindow::lockChanged(bool state) {
    QList<PEGraphicsItem *> pegiList;
    foreach (QGraphicsItem * item, m_currentGraphicsView->scene()->items()) {
        PEGraphicsItem * pegi = dynamic_cast<PEGraphicsItem *>(item);
        if (pegi) pegiList.append(pegi);
    }
    lockChangedAux(state, pegiList);
}

void PEMainWindow::lockChangedAux(bool state, const QList<PEGraphicsItem *> & pegiList) 
{
	if (state) {
		// make sure all connectors are assigned before allowing bus drawing
		QDomElement root = m_fzpDocument.documentElement();
		QDomElement connectors = root.firstChildElement("connectors");
		QDomElement connector = connectors.firstChildElement("connector");
		while (!connector.isNull()) {
			QDomElement p = this->getConnectorPElement(connector, m_currentGraphicsView);
			QString id = p.attribute("svgId");
			bool gotOne = false;
			foreach (PEGraphicsItem * pegi, pegiList) {
				QDomElement pegiElement = pegi->element();
				if (pegiElement.attribute("id").compare(id) == 0) {
					gotOne = true;
					break;
				}
			}
			if (!gotOne) {
				state = false;
				break;
			}
			connector = connector.nextSiblingElement("connector");
		}	
	}

	m_currentGraphicsView->hideConnectors(!state);
}

void PEMainWindow::pegiTerminalPointMoved(PEGraphicsItem * pegi, QPointF p)
{
    // called while terminal point is being dragged, no need for an undo operation

    Q_UNUSED(pegi);
    m_peToolView->setTerminalPointCoords(p);
}

void PEMainWindow::pegiTerminalPointChanged(PEGraphicsItem * pegi, QPointF before, QPointF after)
{
    terminalPointChangedAux(pegi, before, after);
}

void PEMainWindow::pegiMousePressed(PEGraphicsItem * pegi, bool & locked)
{
	locked = m_peToolView->locked();
	if (!locked) return;

	QString id = pegi->element().attribute("id");
	if (id.isEmpty()) return;

	if (m_peToolView->currentConnector().attribute("id").compare(id) == 0) {
		// already there
		return;
	}

	// if a connector has been clicked, make it the current connector
	QDomElement root = m_fzpDocument.documentElement();
	QDomElement connectors = root.firstChildElement("connectors");
	QDomElement connector = connectors.firstChildElement("connector");
	while (!connector.isNull()) {
		QDomElement p = this->getConnectorPElement(connector, m_currentGraphicsView);
		if (p.attribute("svgId") == id || p.attribute("terminalId") == id) {
			m_peToolView->setCurrentConnector(connector);
			return;
		}
		connector = connector.nextSiblingElement("connector");
	}
}

void PEMainWindow::pegiMouseReleased(PEGraphicsItem * pegi)
{
	if (m_peToolView->locked()) return;

    QString newGorn = pegi->element().attribute("gorn");
    QDomDocument * doc = m_docs.value(m_currentGraphicsView->viewIdentifier());
    QDomElement root = doc->documentElement();
    QDomElement newGornElement = TextUtils::findElementWithAttribute(root, "gorn", newGorn);
    if (newGornElement.isNull()) {
        return;
    }

    QDomElement currentConnectorElement = m_peToolView->currentConnector();
    QString id, terminalID;
    if (!getConnectorIDs(currentConnectorElement, m_currentGraphicsView, id, terminalID)) {
        return;
    }

    QDomElement oldGornElement = TextUtils::findElementWithAttribute(root, "id", id);
    QString oldGorn = oldGornElement.attribute("gorn");
    QString oldGornTerminal;
    if (!terminalID.isEmpty()) {
        QDomElement element = TextUtils::findElementWithAttribute(root, "id", terminalID);
        oldGornTerminal = element.attribute("gorn");
    }

    if (newGornElement.attribute("gorn").compare(oldGornElement.attribute("gorn")) == 0) {
        // no change
        return;
    }

    RelocateConnectorSvgCommand * rcsc = new RelocateConnectorSvgCommand(this, m_currentGraphicsView, id, terminalID, oldGorn, oldGornTerminal, newGorn, "", NULL);
    rcsc->setText(tr("Relocate connector %1").arg(currentConnectorElement.attribute("name")));
    m_undoStack->waitPush(rcsc, SketchWidget::PropChangeDelay);
}

void PEMainWindow::createFileMenu() {
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileMenu->addAction(m_openAct);
    m_fileMenu->addAction(m_revertAct);

    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_closeAct);
    m_fileMenu->addAction(m_saveAct);
    m_fileMenu->addAction(m_saveAsAct);

    m_fileMenu->addSeparator();
    //m_fileMenu->addAction(m_pageSetupAct);
    m_fileMenu->addAction(m_printAct);
    m_fileMenu->addAction(m_showInOSAct);

	m_fileMenu->addSeparator();
	m_fileMenu->addAction(m_quitAct);

    connect(m_fileMenu, SIGNAL(aboutToShow()), this, SLOT(updateFileMenu()));
}

QDomElement PEMainWindow::getConnectorPElement(const QDomElement & element, SketchWidget * sketchWidget)
{
    QString viewName = ViewLayer::viewIdentifierXmlName(sketchWidget->viewIdentifier());
    QDomElement views = element.firstChildElement("views");
    QDomElement view = views.firstChildElement(viewName);
    return view.firstChildElement("p");
}

bool PEMainWindow::getConnectorIDs(const QDomElement & element, SketchWidget * sketchWidget, QString & id, QString & terminalID) {
    QDomElement p = getConnectorPElement(element, sketchWidget);
    if (p.isNull()) return false;

    id = p.attribute("svgId");
    if (id.isEmpty()) return false;

    terminalID = p.attribute("terminalId");
    return true;
}

void PEMainWindow::relocateConnectorSvg(SketchWidget * sketchWidget, const QString & id, const QString & terminalID,
                const QString & oldGorn, const QString & oldGornTerminal, const QString & newGorn, const QString & newGornTerminal, 
                int changeDirection)
{
    QDomDocument * doc = m_docs.value(sketchWidget->viewIdentifier());
    QDomElement root = doc->documentElement();

    QDomElement oldGornElement = TextUtils::findElementWithAttribute(root, "gorn", oldGorn);
    QDomElement oldGornTerminalElement;
    if (!oldGornTerminal.isEmpty()) {
        oldGornTerminalElement = TextUtils::findElementWithAttribute(root, "gorn", oldGornTerminal);
    }
    QDomElement newGornElement = TextUtils::findElementWithAttribute(root, "gorn", newGorn);
    QDomElement newGornTerminalElement;
    if (!newGornTerminal.isEmpty()) {
        newGornTerminalElement = TextUtils::findElementWithAttribute(root, "gorn", newGornTerminal);
    }

    if (!oldGornElement.isNull()) oldGornElement.setAttribute("id", "");
    if (!oldGornTerminalElement.isNull()) oldGornTerminalElement.setAttribute("id", "");
    if (!newGornElement.isNull()) newGornElement.setAttribute("id", id);
    if (!newGornTerminalElement.isNull()) newGornTerminalElement.setAttribute("id", terminalID);

    foreach (QGraphicsItem * item, sketchWidget->scene()->items()) {
        PEGraphicsItem * pegi = dynamic_cast<PEGraphicsItem *>(item);
        if (pegi == NULL) continue;

        QDomElement element = pegi->element();
        if (element.attribute("gorn").compare(newGorn) == 0) {
            pegi->setHighlighted(true);
            switchedConnector(m_peToolView->currentConnector(), sketchWidget, false);

        }
        else if (element.attribute("gorn").compare(oldGorn) == 0) {
            pegi->showTerminalPoint(false);         // if newGorn is empty as a redo when the original relocate started with no connector in the svg file
        }
    }

    updateChangeCount(sketchWidget, changeDirection);
}

bool PEMainWindow::save() {
    if (!canSave()) {
        return saveAs(false);
    }

    return saveAs(true);
}

bool PEMainWindow::saveAs() {
    return saveAs(false);
}

bool PEMainWindow::saveAs(bool overWrite)
{
    QDomElement fzpRoot = m_fzpDocument.documentElement();
    QString workingModuleID = fzpRoot.attribute("moduleId");

    QList<MainWindow *> affectedWindows;
    if (overWrite) {
        fzpRoot.setAttribute("moduleId", m_originalModuleID);
        foreach (QWidget *widget, QApplication::topLevelWidgets()) {
            MainWindow *mainWindow = qobject_cast<MainWindow *>(widget);
		    if (mainWindow == NULL) continue;
			    
		    if (qobject_cast<PEMainWindow *>(mainWindow) != NULL) continue;

            if (mainWindow->usesPart(m_originalModuleID)) {
                affectedWindows.append(mainWindow);
            }
	    }

        if (affectedWindows.count() > 0 && !m_gaveSaveWarning) {
	        QMessageBox messageBox(this);
	        messageBox.setWindowTitle(tr("Sketch Change Warning"));
            QString message;
            if (affectedWindows.count() == 1) {
                message = tr("The open sketch '%1' uses the part you are editing. ").arg(affectedWindows.first()->windowTitle());
                message += tr("Saving this part will make a change to the sketch that cannot be undone.");
            }
            else {
                message =  tr("The open sketches ");
                for (int i = 0; i < affectedWindows.count() - 1; i++) {
                    message += tr("'%1', ").arg(affectedWindows.at(i)->windowTitle());
                }
                message += tr("and '%1' ").arg(affectedWindows.last()->windowTitle());
                message += tr("Saving this part will make a change to these sketches that cannot be undone.");

            }
            message += tr("\n\nGo ahead and save?");

	        messageBox.setText(message);
	        messageBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	        messageBox.setDefaultButton(QMessageBox::Cancel);
	        messageBox.setIcon(QMessageBox::Warning);
	        messageBox.setWindowModality(Qt::WindowModal);
	        messageBox.setButtonText(QMessageBox::Ok, tr("Save"));
	        messageBox.setButtonText(QMessageBox::Cancel, tr("Cancel"));
	        QMessageBox::StandardButton answer = (QMessageBox::StandardButton) messageBox.exec();

	        if (answer != QMessageBox::Ok) {
		        return false;
	        }

            m_gaveSaveWarning = true;
        }
    }

    QDomElement views = fzpRoot.firstChildElement("views");

    QHash<ViewLayer::ViewIdentifier, QString> svgPaths;

    foreach (SketchWidget * sketchWidget, m_sketchWidgets.values()) {
        QDomElement view = views.firstChildElement(ViewLayer::viewIdentifierXmlName(sketchWidget->viewIdentifier()));
        QDomElement layers = view.firstChildElement("layers");

        QString currentSvgPath = layers.attribute("image");
        if (!currentSvgPath.contains(m_guid) && m_svgChangeCount.value(sketchWidget->viewIdentifier()) == 0) {
            // unaltered svg
            continue;
        }

        QString referenceFile;
        QString svg = m_docs.value(sketchWidget->viewIdentifier())->toString();
        if (!svg.contains(ReferenceFileString)) {
            referenceFile = getSvgReferenceFile(m_originalSvgPaths.value(sketchWidget->viewIdentifier()));
            int ix = svg.indexOf("<svg");
            if (ix >= 0) {
                int jx = svg.indexOf(">", ix);
                if (jx > ix) {
                    svg.insert(jx + 1, makeDesc(referenceFile));
                }
            }
        }

        svgPaths.insert(sketchWidget->viewIdentifier(), currentSvgPath);
        QString svgPath = makeSvgPath(referenceFile, sketchWidget, false);

        bool svgOverWrite = false;
        if (overWrite) {
            QFileInfo info(m_originalSvgPaths.value(sketchWidget->viewIdentifier()));
            svgOverWrite = info.absolutePath().contains(m_userPartsFolderSvgPath);
            if (svgOverWrite) {
                QDir dir = info.absoluteDir();
                svgPath = dir.dirName() + "/" + info.fileName();
            }
        }

        layers.setAttribute("image", svgPath);

        QString actualPath = svgOverWrite ? m_originalSvgPaths.value(sketchWidget->viewIdentifier()) : m_userPartsFolderSvgPath + svgPath; 
        bool result = writeXml(actualPath, svg, false);
        if (!result) {
            // TODO: warn user
        }
    }

    QDir dir(m_userPartsFolderPath);
    QString fzpPath = dir.absoluteFilePath(QString("%1%2.fzp").arg(getFzpReferenceFile()).arg(m_guid));   
    if (overWrite) {
        fzpPath = m_originalFzpPath;
    }
    bool result = writeXml(fzpPath, m_fzpDocument.toString(), false);

    if (!overWrite) {
        m_originalFzpPath = fzpPath;
        m_originalModuleID = fzpRoot.attribute("moduleId");
    }

    QString savedModuleID = fzpRoot.attribute("moduleId");

    // restore the set of working svg files
    foreach (SketchWidget * sketchWidget, m_sketchWidgets.values()) {
        QString svgPath = svgPaths.value(sketchWidget->viewIdentifier());
        if (svgPath.isEmpty()) continue;

        QDomElement view = views.firstChildElement(ViewLayer::viewIdentifierXmlName(sketchWidget->viewIdentifier()));
        QDomElement layers = view.firstChildElement("layers");
        layers.setAttribute("image", svgPath);
    }

    // restores the temporary fzp state
    fzpRoot.setAttribute("moduleId", workingModuleID);

    ModelPart * modelPart = m_referenceModel->retrieveModelPart(savedModuleID);
    if (modelPart == NULL) {
	    modelPart = m_referenceModel->loadPart(fzpPath, true);
        emit addToMyPartsSignal(modelPart);
	}
    else {
        m_referenceModel->reloadPart(fzpPath, m_originalModuleID);
        QUndoStack undoStack;
        QUndoCommand * parentCommand = new QUndoCommand;
        foreach (MainWindow * mainWindow, affectedWindows) {
            mainWindow->updateParts(m_originalModuleID, parentCommand);
        }
        undoStack.push(parentCommand);
    }


	m_autosaveNeeded = false;
    m_undoStack->setClean();

    return result;
}


void PEMainWindow::updateChangeCount(SketchWidget * sketchWidget, int changeDirection) {
    m_svgChangeCount.insert(sketchWidget->viewIdentifier(), m_svgChangeCount.value(sketchWidget->viewIdentifier()) + changeDirection);
}

PEGraphicsItem * PEMainWindow::findConnectorItem()
{
    foreach (QGraphicsItem * item, m_currentGraphicsView->scene()->items()) {
        PEGraphicsItem * pegi = dynamic_cast<PEGraphicsItem *>(item);
        if (pegi && pegi->showingTerminalPoint()) return pegi;
    }

    return NULL;
}

void PEMainWindow::terminalPointChanged(const QString & how) {
    PEGraphicsItem * pegi = findConnectorItem();
    if (pegi == NULL) return;

    QRectF r = pegi->rect();
    QPointF p = r.center();
    if (how == "center") {
    }
    else if (how == "N") {
        p.setY(0);
    }
    else if (how == "E") {
        p.setX(r.width());
    }
    else if (how == "S") {
        p.setY(r.height());
    }
    else if (how == "W") {
        p.setX(0);
    }
    terminalPointChangedAux(pegi, pegi->terminalPoint(), p);

    // TODO: UndoCommand which changes fzp xml and svg xml
}

void PEMainWindow::terminalPointChanged(const QString & coord, double value)
{
    PEGraphicsItem * pegi = findConnectorItem();
    if (pegi == NULL) return;

    QPointF p = pegi->terminalPoint();
    if (coord == "x") {
        p.setX(qMax(0.0, qMin(value, pegi->rect().width())));
    }
    else {
        p.setY(qMax(0.0, qMin(value, pegi->rect().height())));
    }
    
    terminalPointChangedAux(pegi, pegi->terminalPoint(), p);
    // TODO: UndoCommand which changes fzp xml and svg xml
}

void PEMainWindow::terminalPointChangedAux(PEGraphicsItem * pegi, QPointF before, QPointF after)
{
    if (pegi->pendingTerminalPoint() == after) {
        return;
    }

    pegi->setPendingTerminalPoint(after);

    QDomElement currentConnectorElement = m_peToolView->currentConnector();

    MoveTerminalPointCommand * mtpc = new MoveTerminalPointCommand(this, this->m_currentGraphicsView, currentConnectorElement.attribute("id"), pegi->rect().size(), before, after, NULL);
    mtpc->setText(tr("Move terminal point"));
    m_undoStack->waitPush(mtpc, SketchWidget::PropChangeDelay);
}

void PEMainWindow::getSpinAmount(double & amount) {
    double zoom = m_currentGraphicsView->currentZoom() / 100;
    if (zoom == 0) {
        amount = 1;
        return;
    }

    amount = qMin(1.0, 1.0 / zoom);
    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        amount *= 10;
    }
}

void PEMainWindow::moveTerminalPoint(SketchWidget * sketchWidget, const QString & connectorID, QSizeF size, QPointF p, int changeDirection)
{
    // TODO: assumes these IDs are unique--need to check if they are not 

    QPointF center(size.width() / 2, size.height() / 2);
    bool centered = qAbs(center.x() - p.x()) < .05 && qAbs(center.y() - p.y()) < .05;

    QDomElement fzpRoot = m_fzpDocument.documentElement();
    QDomElement connectorElement = TextUtils::findElementWithAttribute(fzpRoot, "id", connectorID);
    if (connectorElement.isNull()) {
        DebugDialog::debug(QString("missing connector %1").arg(connectorID));
        return;
    }

    QDomElement pElement = getConnectorPElement(connectorElement, sketchWidget);
    QString svgID = pElement.attribute("svgId");
    if (svgID.isEmpty()) {
        DebugDialog::debug(QString("Can't find svgId for connector %1").arg(connectorID));
        return;
    }

    PEGraphicsItem * connectorPegi = NULL;
    QList<PEGraphicsItem *> pegiList;
    foreach (QGraphicsItem * item, sketchWidget->scene()->items()) {
        PEGraphicsItem * pegi = dynamic_cast<PEGraphicsItem *>(item);
        if (pegi == NULL) continue;

        pegiList.append(pegi);
        QDomElement pegiElement = pegi->element();
        if (pegiElement.attribute("id").compare(svgID) == 0) {
            connectorPegi = pegi;        
        }
    }
    if (connectorPegi == NULL) return;

    if (centered) {
        pElement.removeAttribute("terminalId");
        // no need to change SVG in this case
    }
    else {
        QDomDocument * svgDoc = m_docs.value(sketchWidget->viewIdentifier());
        QDomElement svgRoot = svgDoc->documentElement();
        QDomElement svgConnectorElement = TextUtils::findElementWithAttribute(svgRoot, "id", svgID);
        if (svgConnectorElement.isNull()) {
            DebugDialog::debug(QString("Unable to find svg connector element %1").arg(svgID));
            return;
        }

        QString terminalID = pElement.attribute("terminalId");
        if (terminalID.isEmpty()) {
            terminalID = svgID;
            if (terminalID.endsWith("pin") || terminalID.endsWith("pad")) {
                terminalID.chop(3);
            }
            terminalID += "terminal";
            pElement.setAttribute("terminalId", terminalID);
        }

        FSvgRenderer renderer;
        renderer.loadSvg(svgDoc->toByteArray(), "", false);
        QRectF svgBounds = renderer.boundsOnElement(svgID);
        double cx = p.x () * svgBounds.width() / size.width();
        double cy = p.y() * svgBounds.height() / size.height();
        double dx = svgBounds.width() / 1000;
        double dy = svgBounds.height() / 1000;

        QDomElement terminalElement = TextUtils::findElementWithAttribute(svgRoot, "id", terminalID);
        if (terminalElement.isNull()) {
            terminalElement = svgDoc->createElement("rect");
        }
        else if (terminalElement.tagName() != "rect" || terminalElement.attribute("fill") != "none" || terminalElement.attribute("stroke") != "none") {
            terminalElement.setAttribute("id", "");
            terminalElement = svgDoc->createElement("rect");
        }
        terminalElement.setAttribute("id", terminalID);
        terminalElement.setAttribute("stroke", "none");
        terminalElement.setAttribute("fill", "none");
        terminalElement.setAttribute("stroke-width", "0");
        terminalElement.setAttribute("x", svgBounds.left() + cx - dx);
        terminalElement.setAttribute("y", svgBounds.top() + cy - dy);
        terminalElement.setAttribute("width", dx * 2);
        terminalElement.setAttribute("height", dy * 2);
        if (terminalElement.attribute("gorn").isEmpty()) {
            QString gorn = svgConnectorElement.attribute("gorn");
            int ix = gorn.lastIndexOf(".");
            if (ix > 0) {
                gorn.truncate(ix);
            }
            gorn = QString("%1.gen%2").arg(gorn).arg(FakeGornSiblingNumber++);
            terminalElement.setAttribute("gorn", gorn);
        }

        svgConnectorElement.parentNode().insertAfter(terminalElement, svgConnectorElement);

        double oldZ = connectorPegi->zValue() + 1;
        foreach (PEGraphicsItem * pegi, pegiList) {
            QDomElement pegiElement = pegi->element();
            if (pegiElement.attribute("id").compare(terminalID) == 0) {
                DebugDialog::debug("old pegi location", pegi->pos());
                oldZ = pegi->zValue();
                pegiList.removeOne(pegi);
                delete pegi;
                break;
            }
        }

        double invdx = dx * size.width() / svgBounds.width();
        double invdy = dy * size.height() / svgBounds.height();
        QPointF topLeft = connectorPegi->offset() + p - QPointF(invdx, invdy);
        PEGraphicsItem * pegi = makePegi(QSizeF(invdx * 2, invdy * 2), topLeft, m_items.value(sketchWidget->viewIdentifier()), terminalElement);
        pegi->setZValue(oldZ);
        DebugDialog::debug("new pegi location", pegi->pos());
        updateChangeCount(sketchWidget, changeDirection);
    }

    connectorPegi->setTerminalPoint(p);
    connectorPegi->update();
    m_peToolView->setTerminalPointCoords(p);
}

// http://stackoverflow.com/questions/3490336/how-to-reveal-in-finder-or-show-in-explorer-with-qt
// http://stackoverflow.com/questions/9581330/change-selection-in-explorer-window
void PEMainWindow::showInOS(QWidget *parent, const QString &pathIn)
{
    // Mac, Windows support folder or file.
#if defined(Q_OS_WIN)
    Q_UNUSED(parent)
    const QString explorer = "explorer.exe";
    QString param = QLatin1String("/e,/select,");
    param += QDir::toNativeSeparators(pathIn);
    QProcess::startDetached(explorer, QStringList(param));
#elif defined(Q_OS_MAC)
    Q_UNUSED(parent)
    QStringList scriptArgs;
    scriptArgs << QLatin1String("-e")
               << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
                                     .arg(pathIn);
    QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
    scriptArgs.clear();
    scriptArgs << QLatin1String("-e")
               << QLatin1String("tell application \"Finder\" to activate");
    QProcess::execute("/usr/bin/osascript", scriptArgs);
#else
    QDesktopServices::openUrl( QUrl::fromLocalFile( QFileInfo(pathIn).absolutePath() ) );   
#endif


}

void PEMainWindow::showInOS() {
    if (m_currentGraphicsView == NULL) return;

    ItemBase * itemBase = m_items.value(m_currentGraphicsView->viewIdentifier());
    showInOS(this, itemBase->filename());
}

PEGraphicsItem * PEMainWindow::makePegi(QSizeF size, QPointF topLeft, ItemBase * itemBase, QDomElement & element) 
{
    PEGraphicsItem * pegiItem = new PEGraphicsItem(0, 0, size.width(), size.height());
    pegiItem->setPos(itemBase->pos() + topLeft);
    int z = ZList.value(itemBase->viewIdentifier());
    pegiItem->setZValue(z);
    itemBase->scene()->addItem(pegiItem);
    pegiItem->setElement(element);
    pegiItem->setOffset(topLeft);
    connect(pegiItem, SIGNAL(highlightSignal(PEGraphicsItem *)), this, SLOT(highlightSlot(PEGraphicsItem *)));
    connect(pegiItem, SIGNAL(mouseReleased(PEGraphicsItem *)), this, SLOT(pegiMouseReleased(PEGraphicsItem *)));
    connect(pegiItem, SIGNAL(mousePressed(PEGraphicsItem *, bool &)), this, SLOT(pegiMousePressed(PEGraphicsItem *, bool &)), Qt::DirectConnection);
    connect(pegiItem, SIGNAL(terminalPointMoved(PEGraphicsItem *, QPointF)), this, SLOT(pegiTerminalPointMoved(PEGraphicsItem *, QPointF)));
    connect(pegiItem, SIGNAL(terminalPointChanged(PEGraphicsItem *, QPointF, QPointF)), this, SLOT(pegiTerminalPointChanged(PEGraphicsItem *, QPointF, QPointF)));
    return pegiItem;
}

QRectF PEMainWindow::getPixelBounds(FSvgRenderer & renderer, QDomElement & element)
{
	QSizeF defaultSizeF = renderer.defaultSizeF();
	QRectF viewBox = renderer.viewBoxF();

    QString id = element.attribute("id");
    QRectF r = renderer.boundsOnElement(id);
    QMatrix matrix = renderer.matrixForElement(id);
    QString oldid = element.attribute("oldid");
    if (!oldid.isEmpty()) {
        element.setAttribute("id", oldid);
        element.removeAttribute("oldid");
    }
    QRectF bounds = matrix.mapRect(r);
	bounds.setRect(bounds.x() * defaultSizeF.width() / viewBox.width(), 
						bounds.y() * defaultSizeF.height() / viewBox.height(), 
						bounds.width() * defaultSizeF.width() / viewBox.width(), 
						bounds.height() * defaultSizeF.height() / viewBox.height());
    return bounds;
}

bool PEMainWindow::canSave() {

    return m_canSave;
}

void PEMainWindow::tabWidget_currentChanged(int index) {
    MainWindow::tabWidget_currentChanged(index);

    if (m_peToolView == NULL) return;

    switchedConnector(m_peToolView->currentConnector());

    bool enabled = index < IconViewIndex;
	m_peSvgView->setChildrenVisible(enabled);
	m_peToolView->setChildrenVisible(enabled);

    if (m_currentGraphicsView == NULL) {
        // update title when switching to connector and metadata view
        setTitle();
    }
    else {
        // processeventblocker might be enough
	
		ItemBase * itemBase = m_items.value(m_currentGraphicsView->viewIdentifier(), NULL);
		if (itemBase != NULL) {
			QString referenceFile = getSvgReferenceFile(itemBase->filename());
			if (referenceFile.isEmpty()) referenceFile = itemBase->filename();
			m_peSvgView->setFilename(referenceFile);
			QTimer::singleShot(10, this, SLOT(initZoom()));
		}
    }
}

void PEMainWindow::backupSketch()
{
}

void PEMainWindow::removedConnector(const QDomElement & element)
{
    QList<QDomElement> connectors;
    connectors.append(element);
    removedConnectorsAux(connectors);
}

void PEMainWindow::removedConnectors(QList<ConnectorMetadata *> & cmdList)
{
    QList<QDomElement> connectors;

    foreach (ConnectorMetadata * cmd, cmdList) {
        int index;
        QDomElement connector = findConnector(cmd->connectorID, index);
        if (connector.isNull()) return;

        cmd->index = index;
        connectors.append(connector);
    }

    removedConnectorsAux(connectors);
}

void PEMainWindow::removedConnectorsAux(QList<QDomElement> & connectors)
{
    QString originalPath = saveFzp();

    foreach (QDomElement connector, connectors) {
        connector.parentNode().removeChild(connector);
    }

    QString newPath = saveFzp();

    ChangeFzpCommand * cfc = new ChangeFzpCommand(this, originalPath, newPath, NULL);
    QString message;
    if (connectors.count() == 1) {
        message = tr("Remove connector");
    }
    else {
        message = tr("Remove %1 connectors").arg(connectors.count());
    }
    cfc->setText(message);
    m_undoStack->waitPush(cfc, SketchWidget::PropChangeDelay);
}

void PEMainWindow::restoreFzp(const QString & fzpPath) 
{
    if (!loadFzp(fzpPath)) return;

    killPegi();
    reload(false);
}

void PEMainWindow::setBeforeClosingText(const QString & filename, QMessageBox & messageBox)
{
    Q_UNUSED(filename);

    QString partTitle = getPartTitle();
    messageBox.setWindowTitle(tr("Save \"%1\"").arg(partTitle));
    messageBox.setText(tr("Do you want to save the changes you made in the part \"%1\"?").arg(partTitle));
    messageBox.setInformativeText(tr("Your changes will be lost if you don't save them."));
}

QString PEMainWindow::getPartTitle() {
    QString partTitle = tr("untitled part");
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement title = root.firstChildElement("title");
    QString candidate = title.text();
    if (!candidate.isEmpty()) return candidate;

    if (m_items.count() > 0) {
        candidate = m_items.values().at(0)->title();
        if (!candidate.isEmpty()) return candidate;
    }

    return partTitle;
}

void PEMainWindow::killPegi() {
    foreach (SketchWidget * sketchWidget, m_sketchWidgets.values()) {
        foreach (QGraphicsItem * item, sketchWidget->scene()->items()) {
            PEGraphicsItem * pegi = dynamic_cast<PEGraphicsItem *>(item);
            if (pegi) delete pegi;
        }
    }   
}

bool PEMainWindow::loadFzp(const QString & path) {
    QFile file(path);
	QString errorStr;
	int errorLine;
	int errorColumn;
	bool result = m_fzpDocument.setContent(&file, &errorStr, &errorLine, &errorColumn);
	if (!result) {
        QMessageBox::critical(NULL, tr("Parts Editor"), QString("Unable to load fzp from %1").arg(path));
		return false;
	}

    return true;
}

void PEMainWindow::connectorCountChanged(int newCount) {
    QList<QDomElement> connectorList;
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement connectors = root.firstChildElement("connectors");
    QDomElement connector = connectors.firstChildElement("connector");
    while (!connector.isNull()) {
        connectorList.append(connector);
        connector = connector.nextSiblingElement();
    }

    if (newCount == connectorList.count()) return;

    if (newCount < connectorList.count()) {
        qSort(connectorList.begin(), connectorList.end(), byID);
        QList<QDomElement> toDelete;
        for (int i = newCount; i < connectorList.count(); i++) {
            toDelete.append(connectorList.at(i));
        }

        removedConnectorsAux(toDelete);
        return;
    }

    // add connectors 
    int id = 0;
    foreach (QDomElement connector, connectorList) {
    	int ix = IntegerFinder.indexIn(connector.attribute("id"));
        if (ix >= 0) {
            int candidate = IntegerFinder.cap(0).toInt();
            if (candidate > id) id = candidate;
        }
    }

    QString originalPath = saveFzp();

    for (int i = connectorList.count(); i < newCount; i++) {
        id++;
        QDomElement element = m_fzpDocument.createElement("connector");
        connectors.appendChild(element);
        element.setAttribute("type", "male");
        element.setAttribute("name", QString("pin %1").arg(id));
        element.setAttribute("id", QString("connector%1").arg(id));
    }

    QString newPath = saveFzp();

    ChangeFzpCommand * cfc = new ChangeFzpCommand(this, originalPath, newPath, NULL);
    QString message;
    if (newCount - connectorList.count() == 1) {
        message = tr("Add connector");
    }
    else {
        message = tr("Add %1 connectors").arg(newCount - connectorList.count());
    }
    cfc->setText(message);
    m_undoStack->waitPush(cfc, SketchWidget::PropChangeDelay);
}

bool PEMainWindow::editsModuleID(const QString & moduleID) {
    // only to detect whether a user tries to open the parts editor on the same part twice
    return (m_originalModuleID.compare(moduleID) == 0);
}

QString PEMainWindow::getFzpReferenceFile() {
    QString referenceFile = m_fzpDocument.documentElement().attribute(ReferenceFileString);
    if (!referenceFile.isEmpty()) referenceFile += "_";
    return referenceFile;
}

QString PEMainWindow::getSvgReferenceFile(const QString & filename) {
    QFileInfo info(filename);
    QString referenceFile = info.fileName();

    QFile file(filename);
    if (file.open(QFile::ReadOnly)) {
        QString svg = file.readAll();
        int ix = svg.indexOf("<" + ReferenceFileString + ">");
        if (ix >= 0) {
            int jx = svg.indexOf(">", ix);
            if (jx > ix) {
                int kx = svg.indexOf("<");
                if (kx > jx) {
                    referenceFile = svg.mid(jx + 1, kx - jx - 1);
                }
            }
        }
    }

    return referenceFile;
}

QString PEMainWindow::makeDesc(const QString & referenceFile) 
{
    return QString("\n<desc><%2>%1</%2></desc>\n").arg(referenceFile).arg(ReferenceFileString);
}

void PEMainWindow::updateWindowMenu() {
}

void PEMainWindow::updateRaiseWindowAction() {
    QString title = tr("Fritzing (New) Parts Editor");
    QString partTitle = getPartTitle();
	QString actionText = QString("%1: %2").arg(title).arg(partTitle);
	m_raiseWindowAct->setText(actionText);
	m_raiseWindowAct->setToolTip(actionText);
	m_raiseWindowAct->setStatusTip("raise \""+actionText+"\" window");
}

bool PEMainWindow::writeXml(const QString & path, const QString & xml, bool temp)
{
	bool result = TextUtils::writeUtf8(path, TextUtils::svgNSOnly(xml));
	if (result) {
		if (temp) m_toDelete.append(path);
		else m_toDelete.removeAll(path);
	}

	return result;
}

void PEMainWindow::displayBuses() {
	QList<Wire *> toDelete;
	foreach (SketchWidget * sketchWidget, m_sketchWidgets.values()) {
		foreach (QGraphicsItem * item, sketchWidget->scene()->items()) {
			Wire * wire = dynamic_cast<Wire *>(item);
			if (wire == NULL) continue;

			toDelete << wire;
		}
	}

	foreach (Wire * wire, toDelete) {
		delete wire;
	}

	QDomElement root = m_fzpDocument.documentElement();
	QDomElement buses = root.firstChildElement("buses");
	QDomElement bus = buses.firstChildElement("bus");
	while (!bus.isNull()) {
		QDomElement nodeMember = bus.firstChildElement("nodeMember");
		QSet<QString> connectorIDs;
		while (!nodeMember.isNull()) {
			QString connectorID = nodeMember.attribute("connectorId");
			if (!connectorID.isEmpty()) {
				connectorIDs.insert(connectorID);
			}
			nodeMember = nodeMember.nextSiblingElement("nodeMember");
		}

		foreach (ViewLayer::ViewIdentifier viewIdentifier, m_sketchWidgets.keys()) {
			SketchWidget * sketchWidget = m_sketchWidgets.value(viewIdentifier);
			ItemBase * itemBase = m_items.value(viewIdentifier);
			QList<ConnectorItem *> connectorItems;
			foreach (QString connectorID, connectorIDs) {
				ConnectorItem * connectorItem = itemBase->findConnectorItemWithSharedID(connectorID, itemBase->viewLayerSpec());
				if (connectorItem) connectorItems.append(connectorItem);
			}
			for (int i = 0; i < connectorItems.count() - 1; i++) {
				ConnectorItem * c1 = connectorItems.at(i);
				ConnectorItem * c2 = connectorItems.at(i + 1);
				Wire * wire = sketchWidget->makeOneRatsnestWire(c1, c2, false, QColor(0, 255, 0), true);
				wire->setZValue(RatZ);
			}
		}

		bus = bus.nextSiblingElement("bus");
	}
}

void PEMainWindow::updateWireMenu() {
	// assumes update wire menu is only called when right-clicking a wire
	// and that wire is cached by the menu in Wire::mousePressEvent

	Wire * wire = m_activeWire;
	m_activeWire = NULL;

	m_deleteBusConnectionAct->setWire(wire);
	m_deleteBusConnectionAct->setEnabled(true);
}

void PEMainWindow::deleteBusConnection() {
	WireAction * wireAction = qobject_cast<WireAction *>(sender());
	if (wireAction == NULL) return;

	Wire * wire = wireAction->wire();
	if (wire == NULL) return;

	QList<Wire *> wires;
	QList<ConnectorItem *> ends;
	wire->collectChained(wires, ends);
	if (ends.count() != 2) return;

	Bus * bus = ends.at(0)->bus();
	if (bus == NULL) return;
	
	QUndoCommand * parentCommand = new QUndoCommand();
	QStringList names;
	names << ends.at(0)->connectorSharedName() << ends.at(1)->connectorSharedName() ;
	new RemoveBusConnectorCommand(this, bus->id(), ends.at(0)->connectorSharedID(), false, parentCommand);
	new RemoveBusConnectorCommand(this, bus->id(), ends.at(1)->connectorSharedID(), false, parentCommand);
	if (ends.at(0)->connectedToItems().count() > 1) {
		// restore it
		names.removeAt(0);
		new RemoveBusConnectorCommand(this, bus->id(), ends.at(0)->connectorSharedID(), true, parentCommand);
	}
	if (ends.at(1)->connectedToItems().count() > 1) {
		// restore it
		new RemoveBusConnectorCommand(this, bus->id(), ends.at(1)->connectorSharedID(), true, parentCommand);
	}

	parentCommand->setText(tr("Remove internal connection from '%1'").arg(names.at(0)));
	m_undoStack->waitPush(parentCommand, SketchWidget::PropChangeDelay);
}

void PEMainWindow::newWireSlot(Wire * wire) {
	wire->setDisplayBendpointCursor(false);
	disconnect(wire, 0, m_sketchWidgets.value(wire->viewIdentifier()), 0);
	connect(wire, SIGNAL(wireChangedSignal(Wire*, const QLineF & , const QLineF & , QPointF, QPointF, ConnectorItem *, ConnectorItem *)	),
			this, SLOT(wireChangedSlot(Wire*, const QLineF & , const QLineF & , QPointF, QPointF, ConnectorItem *, ConnectorItem *)),
			Qt::DirectConnection);		// DirectConnection means call the slot directly like a subroutine, without waiting for a thread or queue
}

void PEMainWindow::wireChangedSlot(Wire* wire, const QLineF &, const QLineF &, QPointF, QPointF, ConnectorItem * fromOnWire, ConnectorItem * to) {
	wire->deleteLater();

	if (to == NULL) return;

	ConnectorItem * from = wire->otherConnector(fromOnWire)->firstConnectedToIsh();
	if (from == NULL) return;

	QDomElement root = m_fzpDocument.documentElement();
	QDomElement buses = root.firstChildElement("buses");
	if (buses.isNull()) {
		buses = m_fzpDocument.createElement("buses");
		root.appendChild(buses);
	}

	QString busID;

	Bus * bus = from->bus();
	if (bus == NULL) {
		bus = to->bus();
	}
	if (bus == NULL) {
		int theMax = std::numeric_limits<int>::max(); 
		for (int ix = 1; ix < theMax; ix++) {
			QString candidate = QString("internal%1").arg(ix);
			QDomElement busElement = findBus(buses, candidate);
			if (busElement.isNull()) {
				busID = candidate;
				break;
			}
		}
	}
	else {
		QDomElement busElement = findBus(buses, bus->id());
		if (busElement.isNull()) {
			QMessageBox::critical(NULL, tr("Parts Editor"), tr("Internal connections are very messed up."));
			return;
		}
	}

	QString fromBusID = findNodeMember(buses, from->connectorSharedID());
	QString toBusID = findNodeMember(buses, to->connectorSharedID());
	QUndoCommand * parentCommand = new QUndoCommand(tr("Add internal connection from '%1' to '%2'").arg(from->connectorSharedName()).arg(to->connectorSharedName()));
	if (!fromBusID.isEmpty()) {
		// changing the bus for this nodeMember
		new RemoveBusConnectorCommand(this, fromBusID, from->connectorSharedID(), false, parentCommand);
	}
	if (!toBusID.isEmpty()) {
		// changing the bus for this nodeMember
		new RemoveBusConnectorCommand(this, toBusID, to->connectorSharedID(), false, parentCommand);
	}
	new RemoveBusConnectorCommand(this, busID, from->connectorSharedID(), true, parentCommand);
	new RemoveBusConnectorCommand(this, busID, to->connectorSharedID(), true, parentCommand);
	m_undoStack->waitPush(parentCommand, SketchWidget::PropChangeDelay);
}

QDomElement PEMainWindow::findBus(const QDomElement & buses, const QString & id)
{
	QDomElement busElement = buses.firstChildElement("bus");
	while (!busElement.isNull()) {
		if (busElement.attribute("id").compare(id) == 0) {
			return busElement;
		}
		busElement = busElement.nextSiblingElement("bus");
	}

	return QDomElement();
}

QString PEMainWindow::findNodeMember(const QDomElement & buses, const QString & connectorID)
{
	QDomElement bus = buses.firstChildElement("bus");
	while (!bus.isNull()) {
		QDomElement nodeMember = bus.firstChildElement("nodeMember");
		while (!nodeMember.isNull()) {
			if (nodeMember.attribute("connectorId").compare(connectorID) == 0) {
				return bus.attribute("id");
			}
			nodeMember = nodeMember.nextSiblingElement("nodeMember");
		}
		bus = bus.nextSiblingElement("bus");
	}

	return "";
}

void PEMainWindow::addBusConnector(const QString & busID, const QString & connectorID)
{
	// called from command object
	removeBusConnector(busID, connectorID);			// keep the dom very clean

	QDomElement root = m_fzpDocument.documentElement();
	QDomElement buses = root.firstChildElement("buses");
	if (buses.isNull()) {
		m_fzpDocument.createElement("buses");
		root.appendChild(buses);
	}

	QDomElement theBusElement = findBus(buses, busID);
	if (theBusElement.isNull()) {
		theBusElement = m_fzpDocument.createElement("bus");
		theBusElement.setAttribute("id", busID);
		buses.appendChild(theBusElement);
	}

	QDomElement nodeMember = m_fzpDocument.createElement("nodeMember");
	nodeMember.setAttribute("connectorId", connectorID);
	theBusElement.appendChild(nodeMember);
	displayBuses();
}

void PEMainWindow::removeBusConnector(const QString & busID, const QString & connectorID)
{
	// called from command object
	// for the sake of cleaning, deletes all matching nodeMembers so be careful about the order of deletion and addition within the same parentCommand
	Q_UNUSED(busID);

	QDomElement root = m_fzpDocument.documentElement();
	QDomElement buses = root.firstChildElement("buses");
	QDomElement bus = buses.firstChildElement("bus");
	QList<QDomElement> toDelete;
	while (!bus.isNull()) {
		QDomElement nodeMember = bus.firstChildElement("nodeMember");
		while (!nodeMember.isNull()) {
			if (nodeMember.attribute("connectorId").compare(connectorID) == 0) {
				toDelete.append(nodeMember);
			}
			nodeMember = nodeMember.nextSiblingElement("nodeMember");
		}
		bus = bus.nextSiblingElement("bus");
	}

	foreach (QDomElement element, toDelete) {
		element.parentNode().removeChild(element);
	}

	displayBuses();
}

void PEMainWindow::replaceProperty(const QString & key, const QString & value, QDomElement & properties)
{
    QDomElement prop = properties.firstChildElement("property");
    while (!prop.isNull()) {
        QString name = prop.attribute("name");
        if (name.compare(key, Qt::CaseInsensitive) == 0) {
            TextUtils::replaceChildText(m_fzpDocument, prop, value);
            return;
        }

        prop = prop.nextSiblingElement("property");
    }
    
	prop = m_fzpDocument.createElement("property");
    properties.appendChild(prop);
    prop.setAttribute("name", key);
    TextUtils::replaceChildText(m_fzpDocument, prop, value);
}

QWidget * PEMainWindow::createTabWidget() {
	return new QTabWidget(this);
}

void PEMainWindow::addTab(QWidget * widget, const QString & label) {
	qobject_cast<QTabWidget *>(m_tabWidget)->addTab(widget, label);
}

int PEMainWindow::currentTabIndex() {
	return qobject_cast<QTabWidget *>(m_tabWidget)->currentIndex();
}

void PEMainWindow::setCurrentTabIndex(int index) {
	qobject_cast<QTabWidget *>(m_tabWidget)->setCurrentIndex(index);
}

QWidget * PEMainWindow::currentTabWidget() {
	return qobject_cast<QTabWidget *>(m_tabWidget)->currentWidget();
}

bool PEMainWindow::event(QEvent * e) {
	if (e->type() == QEvent::Close) {
		//qDebug() << "event close";
		//if (m_inFocusWidgets.count() > 0) {
		//	e->ignore();
		//	qDebug() << "bail in focus";
		//	return true;
		//}
	}

	return MainWindow::event(e);
}

bool PEMainWindow::eventFilter(QObject *object, QEvent *event)
{
	//qDebug() << "event" << event->type();
    if (event->type() == QEvent::FocusIn) {
        QLineEdit * lineEdit = qobject_cast<QLineEdit *>(object);
		if (lineEdit != NULL) {
			if (lineEdit->window() == this) {
				qDebug() << "inc focus";
				m_inFocusWidgets << lineEdit;
			}
		}
		else {
			QTextEdit * textEdit = qobject_cast<QTextEdit *>(object);
			if (textEdit != NULL && textEdit->window() == this) {
				qDebug() << "inc focus";
				m_inFocusWidgets << textEdit;
			}
		}
    }
    if (event->type() == QEvent::FocusOut) {
        QLineEdit * lineEdit = qobject_cast<QLineEdit *>(object);
		if (lineEdit != NULL) {
			if (lineEdit->window() == this) {
				qDebug() << "dec focus";
				m_inFocusWidgets.removeOne(lineEdit);
			}
		}
		else {
			QTextEdit * textEdit = qobject_cast<QTextEdit *>(object);
			if (textEdit != NULL && textEdit->window() == this) {
				qDebug() << "inc focus";
				m_inFocusWidgets.removeOne(textEdit);
			}
		}
    }
    return false;
}

void PEMainWindow::closeLater()
{
	close();
}
