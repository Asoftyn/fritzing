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

	    clean up menus

        crash when swapping part during save
        
        crashed when saving the description
  
        show in OS button
            test on mac, linux

	    disable dragging wires    
            hide connectors
            need to show again during bus mode

	    change pin count
            when the count is smaller give user a choice to truncate or delete individually
            add individual delete button to petoolview

        don't allow parts editor window to open if editor is already open with a given module id

        on svg import detect all connector IDs
            if any are invisible, tell user this is obsolete

        from partseditorview.cpp
	        bool fileHasChanged = (m_viewIdentifier == ViewLayer::IconView) ? false : TextUtils::fixPixelDimensionsIn(fileContent);
	        fileHasChanged |= TextUtils::cleanSodipodi(fileContent);
	        fileHasChanged |= TextUtils::fixViewboxOrigin(fileContent);
	        fileHasChanged |= TextUtils::tspanRemove(fileContent);
	        fileHasChanged |= fixFonts(fileContent,filename,canceled);
	
        import
            kicad footprint
            gEDA footprint

        allow but discourage png imports

        for svg import check for flaws:
            internal coords
            internal transforms
            corel draw not saved for presentation
            inkscape not saved as plain
            inkscape scaling?
            illustrator px
            <gradient>, <pattern>, <marker>, <tspan>, etc.
            pcb view missing layers
            multiple connector or terminal ids

        smd vs. tht

        buses 
            connect bus by drawing a wire
            can this be modal? i.e. turn bus mode on and off

        bendable legs

        set flippable

        zoom slider is not correctly synchronized with actual zoom level

        after "save" need some kind of confirmation
            change to window title is enough?

        deal with customized svgs
            chip label
            * pin label
            * resistance
            * led color
            pin header stuff
            pin size

        keep family as is, but force users to put in a unique variant
            don't allow blank
            check db for already used
            fill in with guid

        delete all unused svg and fzp files when finished

        only allow appropriate file to be loaded for appropriate view (.mod, .fp, etc.)

        sort connector list alphabetically or numerically

        if pcb image has no layers complain directly

    ////////////////////////////// second release /////////////////////////////////

	    first time help?
            dialog box always comes up, click to say not next time

        connector duplicate op

        add layers:  put everything in silkscreen, then give copper1, copper0 checkbox
            what about breadboardbreadboard or other odd layers?
            if you click something as a connector, automatically move it into copper
                how to distinguish between both and top--default to both, let user set "pad"

        swap connector metadata op

        delete op
    
        move connectors with arrow keys, or typed coordinates
	    drag and drop later

        import
            eagle lbr
            eagle brd
            kicad mod?


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

        flip and rotate?


        undo/redo as xml file: use index + guid for uniqueness

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

#include <QApplication>
#include <QSvgGenerator>
#include <QMenuBar>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

////////////////////////////////////////////////////

bool GotZeroConnector = false;

static const int IconViewIndex = 3;
static const int MetadataViewIndex = 4;
static const int ConnectorsViewIndex = 5;

static QHash<ViewLayer::ViewIdentifier, int> ZList;

static long FakeGornSiblingNumber = 0;

bool byID(QDomElement & c1, QDomElement & c2)
{
    int c1id = -1;
    int c2id = -1;
	int ix = IntegerFinder.indexIn(c1.attribute("id"));
    if (ix > 0) c1id = IntegerFinder.cap(0).toInt();
    ix = IntegerFinder.indexIn(c2.attribute("id"));
    if (ix > 0) c2id = IntegerFinder.cap(0).toInt();

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
    m_connectorsView = NULL;
}

PEMainWindow::~PEMainWindow()
{
    // PEGraphicsItems are still holding QDomElement so delete them before m_fzpDocument is deleted
    killPegi();
}

void PEMainWindow::closeEvent(QCloseEvent *event) {
    if (!beforeClosing(true)) {
        event->ignore();
        return;
    }

	QSettings settings;
	settings.setValue(m_settingsPrefix + "state",saveState());
	settings.setValue(m_settingsPrefix + "geometry",saveGeometry());
}

void PEMainWindow::initLockedFiles(bool) {
}

void PEMainWindow::initSketchWidgets()
{
    MainWindow::initSketchWidgets();

	m_iconGraphicsView = new IconSketchWidget(ViewLayer::IconView, this);
	initSketchWidget(m_iconGraphicsView);
	m_iconWidget = new SketchAreaWidget(m_iconGraphicsView,this);
	m_tabWidget->addWidget(m_iconWidget);
    initSketchWidget(m_iconGraphicsView);

    m_docs.insert(m_breadboardGraphicsView->viewIdentifier(), &m_breadboardDocument);
    m_docs.insert(m_schematicGraphicsView->viewIdentifier(), &m_schematicDocument);
    m_docs.insert(m_pcbGraphicsView->viewIdentifier(), &m_pcbDocument);
    m_docs.insert(m_iconGraphicsView->viewIdentifier(), &m_iconDocument);

    m_breadboardGraphicsView->setAcceptWheelEvents(false);
    m_schematicGraphicsView->setAcceptWheelEvents(false);
    m_pcbGraphicsView->setAcceptWheelEvents(false);
    m_iconGraphicsView->setAcceptWheelEvents(false);

    m_metadataView = new PEMetadataView(this);
	SketchAreaWidget * sketchAreaWidget = new SketchAreaWidget(m_metadataView, this);
	m_tabWidget->addWidget(sketchAreaWidget);
    connect(m_metadataView, SIGNAL(metadataChanged(const QString &, const QString &)), this, SLOT(metadataChanged(const QString &, const QString &)));
    connect(m_metadataView, SIGNAL(tagsChanged(const QStringList &)), this, SLOT(tagsChanged(const QStringList &)));
    connect(m_metadataView, SIGNAL(propertiesChanged(const QHash<QString, QString> &)), this, SLOT(propertiesChanged(const QHash<QString, QString> &)));

    m_connectorsView = new PEConnectorsView(this);
	sketchAreaWidget = new SketchAreaWidget(m_connectorsView, this);
	m_tabWidget->addWidget(sketchAreaWidget);
    connect(m_connectorsView, SIGNAL(connectorMetadataChanged(const ConnectorMetadata *)), this, SLOT(connectorMetadataChanged(const ConnectorMetadata *)), Qt::DirectConnection);
    connect(m_connectorsView, SIGNAL(removedConnectors(QList<ConnectorMetadata *> &)), this, SLOT(removedConnectors(QList<ConnectorMetadata *> &)), Qt::DirectConnection);

    m_svgChangeCount.insert(m_breadboardGraphicsView->viewIdentifier(), 0);
    m_svgChangeCount.insert(m_schematicGraphicsView->viewIdentifier(), 0);
    m_svgChangeCount.insert(m_pcbGraphicsView->viewIdentifier(), 0);
    m_svgChangeCount.insert(m_iconGraphicsView->viewIdentifier(), 0);

}

void PEMainWindow::initDock()
{
    m_binManager = new BinManager(m_refModel, NULL, m_undoStack, this);
    m_binManager->openBin(":/resources/bins/pe.fzb");
    m_binManager->hideTabBar();
}

void PEMainWindow::moreInitDock()
{
    static int MinHeight = 75;
    static int DefaultHeight = 100;

    m_peToolView = new PEToolView();
    connect(m_peToolView, SIGNAL(getSpinAmount(double &)), this, SLOT(getSpinAmount(double &)), Qt::DirectConnection);
    connect(m_peToolView, SIGNAL(terminalPointChanged(const QString &)), this, SLOT(terminalPointChanged(const QString &)));
    connect(m_peToolView, SIGNAL(terminalPointChanged(const QString &, double)), this, SLOT(terminalPointChanged(const QString &, double)));
    connect(m_peToolView, SIGNAL(switchedConnector(const QDomElement &)), this, SLOT(switchedConnector(const QDomElement &)));
    connect(m_peToolView, SIGNAL(lockChanged(bool)), this, SLOT(lockChanged(bool)));
    makeDock(tr("Tools"), m_peToolView, DockMinWidth, DockMinHeight);
    m_peToolView->setMinimumSize(DockMinWidth, DockMinHeight);

	QDockWidget * dockWidget = makeDock(BinManager::Title, m_binManager, MinHeight, DefaultHeight);
    dockWidget->resize(0, 0);
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
    createViewMenuActions();
    createHelpMenuActions();
}

void PEMainWindow::createMenus()
{
    createFileMenu();
    createEditMenu();
    createViewMenu();
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
}

QMenu *PEMainWindow::breadboardWireMenu() {
    return NULL;
}
    
QMenu *PEMainWindow::breadboardItemMenu() {
    return NULL;
}

QMenu *PEMainWindow::schematicWireMenu() {
    return NULL;
}
    
QMenu *PEMainWindow::schematicItemMenu() {
    return NULL;
}

QMenu *PEMainWindow::pcbWireMenu() {
    return NULL;
}
    
QMenu *PEMainWindow::pcbItemMenu() {
    return NULL;
}

void PEMainWindow::setInitialItem(PaletteItem * paletteItem) {
    ModelPart * originalModelPart = NULL;
    if (paletteItem == NULL) {
        originalModelPart = m_refModel->retrieveModelPart("generic_ic_dip_8_300mil");
    }
    else {
        originalModelPart = paletteItem->modelPart();
    }

    m_originalFzpPath = originalModelPart->path();
    m_originalModuleID = originalModelPart->moduleID();

    QFileInfo info(originalModelPart->path());
    //DebugDialog::debug(QString("%1, %2").arg(info.absoluteFilePath()).arg(m_userPartsFolderPath));
    m_canSave = info.absoluteFilePath().contains(m_userPartsFolderPath);

    QFile file(originalModelPart->path());
	QString errorStr;
	int errorLine;
	int errorColumn;
	bool result = m_fzpDocument.setContent(&file, &errorStr, &errorLine, &errorColumn);
	if (!result) {
        QMessageBox::critical(NULL, tr("Parts Editor"), QString("Unable to load fzp from %1").arg(originalModelPart->path()));
		return;
	}

    QDomElement fzpRoot = m_fzpDocument.documentElement();
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
    QString family = originalModelPart->family();
    if (family.isEmpty()) {
        family = m_guid;
    }
    else if (!family.startsWith("custom_")) {
        family = "custom_" + family;
    }
    QDomElement properties = fzpRoot.firstChildElement("properties");
    QDomElement prop = properties.firstChildElement("property");
    bool gotProp = false;
    while (!prop.isNull()) {
        QString name = prop.attribute("name");
        if (name.compare("hole size", Qt::CaseInsensitive) == 0) {
            gotProp = true;
            TextUtils::replaceChildText(m_fzpDocument, prop, family);
            break;
        }

        prop = prop.nextSiblingElement("property");
    }
    if (!gotProp) {
        QDomElement prop = m_fzpDocument.createElement("property");
        properties.appendChild(prop);
        prop.setAttribute("name", "family");
        TextUtils::replaceChildText(m_fzpDocument, prop, family);
    }

    QList<SketchWidget *> sketchWidgets;
    sketchWidgets << m_breadboardGraphicsView << m_schematicGraphicsView << m_pcbGraphicsView << m_iconGraphicsView;
    foreach (SketchWidget * sketchWidget, sketchWidgets) {
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

		QSizeF size = itemBase->size();
		svg = TextUtils::makeSVGHeader(GraphicsUtils::SVGDPI, GraphicsUtils::StandardFritzingDPI, size.width(), size.height()) + svg + "</svg>";
        QString svgPath = makeSvgPath(sketchWidget, true);
        result = TextUtils::writeUtf8(m_userPartsFolderSvgPath + svgPath, TextUtils::svgNSOnly(svg));
        if (!result) {
            QMessageBox::critical(NULL, tr("Parts Editor"), QString("Unable to write svg to  %1").arg(svgPath));
		    return;
        }

        QDomElement view = views.firstChildElement(ViewLayer::viewIdentifierXmlName(sketchWidget->viewIdentifier()));
        QDomElement layers = view.firstChildElement("layers");
        if (layers.isNull()) {
            QMessageBox::critical(NULL, tr("Parts Editor"), QString("Unable to parse fzp file  %1").arg(originalModelPart->path()));
		    return;
        }

        layers.setAttribute("image", svgPath);
    }

    reload();

    foreach (ItemBase * itemBase, m_items.values()) {
        m_originalSvgPaths.insert(itemBase->viewIdentifier(), itemBase->filename());
    }

    setTitle();
}

bool PEMainWindow::eventFilter(QObject *object, QEvent *event) 
{
	return QMainWindow::eventFilter(object, event);
}

void PEMainWindow::initHelper()
{
}

void PEMainWindow::initZoom() {
    m_breadboardGraphicsView->fitInWindow();
    m_schematicGraphicsView->fitInWindow();
    m_pcbGraphicsView->fitInWindow();
    m_iconGraphicsView->fitInWindow();
}

void PEMainWindow::setTitle() {
    QString title = tr("Friting (New) Parts Editor");
    QString partTitle = getPartTitle();

    QString viewName;
    if (m_currentGraphicsView) viewName = m_currentGraphicsView->viewName();
    else if (m_tabWidget->currentIndex() == IconViewIndex) viewName = tr("Icon View");
    else if (m_tabWidget->currentIndex() == MetadataViewIndex) viewName = tr("Metadata View");
    else if (m_tabWidget->currentIndex() == ConnectorsViewIndex) viewName = tr("Connectors View");

	setWindowTitle(QString("%1: %2 [%3]%4").arg(title).arg(partTitle).arg(viewName).arg(QtFunkyPlaceholder));
}

void PEMainWindow::createViewMenuActions() {
    MainWindow::createViewMenuActions();

	m_showIconAct = new QAction(tr("Show Icon"), this);
	m_showIconAct->setShortcut(tr("Ctrl+4"));
	m_showIconAct->setStatusTip(tr("Show the icon view"));
	connect(m_showIconAct, SIGNAL(triggered()), this, SLOT(showIconView()));

	m_showMetadataViewAct = new QAction(tr("Show Metatdata"), this);
	m_showMetadataViewAct->setShortcut(tr("Ctrl+5"));
	m_showMetadataViewAct->setStatusTip(tr("Show the metadata view"));
	connect(m_showMetadataViewAct, SIGNAL(triggered()), this, SLOT(showMetadataView()));

    m_showConnectorsViewAct = new QAction(tr("Show Connectors"), this);
	m_showConnectorsViewAct->setShortcut(tr("Ctrl+6"));
	m_showConnectorsViewAct->setStatusTip(tr("Show the connector metatdata in a list view"));
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
    this->m_tabWidget->setCurrentIndex(MetadataViewIndex);
}

void PEMainWindow::showConnectorsView() {
    this->m_tabWidget->setCurrentIndex(ConnectorsViewIndex);
}

void PEMainWindow::showIconView() {
    this->m_tabWidget->setCurrentIndex(IconViewIndex);
}

void PEMainWindow::metadataChanged(const QString & name, const QString & value)
{
    if (name.compare("family") == 0) {
        QHash<QString, QString> oldProperties = getOldProperties();
        QHash<QString, QString> newProperties(oldProperties);
        newProperties.insert("family", value);
    
        ChangePropertiesCommand * cpc = new ChangePropertiesCommand(this, oldProperties, newProperties, NULL);
        cpc->setText(tr("Change family to %1").arg(value));
        cpc->setSkipFirstRedo();
        changeProperties(newProperties, false);
        m_undoStack->waitPush(cpc, SketchWidget::PropChangeDelay);

        return;
    }

    QString menuText = (name.compare("description") == 0) ? tr("Change description") : tr("Change %1 to '%2'").arg(name).arg(value);

    // called from metadataView
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement element = root.firstChildElement(name);
    QString oldValue = element.text();
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

    RemoveConnectorsCommand * rcc = new RemoveConnectorsCommand(this, cmdList, NULL);
    QString message;
    if (cmdList.count() == 1) {
        message = tr("Remove connector");
    }
    else {
        message = tr("Remove %1 connectors").arg(cmdList.count());
    }
    rcc->setText(message);
    m_undoStack->waitPush(rcc, SketchWidget::PropChangeDelay);
}


void PEMainWindow::connectorMetadataChanged(const ConnectorMetadata * cmd)
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

    ChangeConnectorMetadataCommand * ccmc = new ChangeConnectorMetadataCommand(this, oldcmd, *cmd, NULL);
    ccmc->setText(tr("Change connector %1").arg(cmd->connectorName));
    ccmc->setSkipFirstRedo();
    changeConnectorElement(connector, *cmd);
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


void PEMainWindow::changeConnectorMetadata(const ConnectorMetadata & cmd, bool updateDisplay) {
    int index;
    QDomElement connector = findConnector(cmd.connectorID, index);
    if (connector.isNull()) return;

    changeConnectorElement(connector, cmd);
    if (updateDisplay) {
        initConnectors();
    }
}

void PEMainWindow::changeConnectorElement(QDomElement & connector, const ConnectorMetadata & cmd)
{
    connector.setAttribute("name", cmd.connectorName);
    QString type = "male";
    if (cmd.connectorType == Connector::Female) type = "female";
    else if (cmd.connectorType == Connector::Pad) type = "pad";
    connector.setAttribute("type", type);
    QDomElement description = connector.firstChildElement("description");
    TextUtils::replaceElementChildText(m_fzpDocument, connector, "description", cmd.connectorDescription);
}

void PEMainWindow::initSvgTree(ItemBase * itemBase, QDomDocument & domDocument) 
{
    QString errorStr;
    int errorLine;
    int errorColumn;

    QFile file(itemBase->filename());
    if (!domDocument.setContent(&file, true, &errorStr, &errorLine, &errorColumn)) {
		DebugDialog::debug(QString("unable to parse svg: %1 %2 %3").arg(errorStr).arg(errorLine).arg(errorColumn));
        return;
	}

    ZList.insert(itemBase->viewIdentifier(), 5000);

    QDomElement root = domDocument.documentElement();
    TextUtils::elevateTransform(root);
    TextUtils::gornTree(domDocument);   

    FSvgRenderer renderer;
    renderer.loadSvg(domDocument.toByteArray(), "", false);

    QList<QDomElement> traverse;
    traverse << domDocument.documentElement();
    while (traverse.count() > 0) {
        QList<QDomElement> next;
        foreach (QDomElement element, traverse) {
            QString tagName = element.tagName();
            if      (tagName.compare("rect") == 0);
            else if (tagName.compare("g") == 0);
            else if (tagName.compare("circle") == 0);
            else if (tagName.compare("ellipse") == 0);
            else if (tagName.compare("path") == 0);
            else if (tagName.compare("svg") == 0);
            else if (tagName.compare("line") == 0);
            else if (tagName.compare("polyline") == 0);
            else if (tagName.compare("polygon") == 0);
            else if (tagName.compare("use") == 0);
            else if (tagName.compare("text") == 0);
            else continue;

            QRectF bounds = getPixelBounds(renderer, element);

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

    m_connectorsView->initConnectors(connectorList, GotZeroConnector);
    m_peToolView->initConnectors(connectorList, GotZeroConnector);
}

void PEMainWindow::switchedConnector(const QDomElement & element)
{
    if (m_currentGraphicsView == NULL) return;
    if (element.isNull()) return;

    switchedConnector(element, m_currentGraphicsView);
}

void PEMainWindow::switchedConnector(const QDomElement & element, SketchWidget * sketchWidget)
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
    m_peToolView->setLock(gotOne);
    lockChangedAux(gotOne, pegiList);
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

	if (m_currentGraphicsView->viewIdentifier() == ViewLayer::SchematicView) {
		extras[0] = "*.lib";
		imageFiles = tr("Image & Footprint Files (%1 %2 %3 %4);;SVG Files (%1);;JPEG Files (%2);;PNG Files (%3);;Kicad Schematic Files (%4)%5");   // 
	}

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

    QString newPath = origPath;
	if (newPath.endsWith(".svg")) {
        if (newPath.contains(m_userPartsFolderSvgPath)) ;
        else if (newPath.contains(FolderUtils::getApplicationSubFolderPath("parts"))) ;
        else {
            newPath = m_userPartsFolderSvgPath + makeSvgPath(m_currentGraphicsView, true);
            bool success = QFile::copy(origPath, newPath);
            if (!success) {
    		    QMessageBox::warning(NULL, tr("Copy problem"), tr("Unable to make a local copy of: '%1'").arg(origPath));
            }
        }
    }
    else {
		try {
			newPath = createSvgFromImage(newPath);
		}
		catch (const QString & msg) {
    		QMessageBox::warning(NULL, tr("Conversion problem"), tr("Unable to load image file: \n%1").arg(msg));
			return;
		}
	}

	if (!newPath.isEmpty()) {
		ChangeSvgCommand * csc = new ChangeSvgCommand(this, m_currentGraphicsView, itemBase->filename(), newPath, m_originalSvgPaths.value(itemBase->viewIdentifier()), newPath, NULL);
        QFileInfo info(origPath);
        csc->setText(QString("Load '%1'").arg(info.fileName()));
        m_undoStack->waitPush(csc, SketchWidget::PropChangeDelay);
	}
}

QString PEMainWindow::createSvgFromImage(const QString &origFilePath) {

	QString newFilePath = m_userPartsFolderSvgPath + makeSvgPath(m_currentGraphicsView, true);
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

	// deal with png, jpg, etc.:


/* %1=witdh in mm
 * %2=height in mm
 * %3=width in local coords
 * %4=height in local coords
 * %5=binary data
 */
/*	QString svgTemplate =
"<?xml version='1.0' encoding='UTF-8' standalone='no'?>\n"
"	<svg width='%1mm' height='%2mm' viewBox='0 0 %3 %4' xmlns='http://www.w3.org/2000/svg'\n"
"		xmlns:xlink='http://www.w3.org/1999/xlink' version='1.2' baseProfile='tiny'>\n"
"		<g fill='none' stroke='black' vector-effect='non-scaling-stroke' stroke-width='1'\n"
"			fill-rule='evenodd' stroke-linecap='square' stroke-linejoin='bevel' >\n"
"			<image x='0' y='0' width='%3' height='%4'\n"
"				xlink:href='data:image/png;base64,%5' />\n"
"		</g>\n"
"	</svg>";

	QPixmap pixmap(origFilePath);
	QByteArray bytes;
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::WriteOnly);
	pixmap.save(&buffer,"png"); // writes pixmap into bytes in PNG format

	QString svgDom = svgTemplate
		.arg(pixmap.widthMM()).arg(pixmap.heightMM())
		.arg(pixmap.width()).arg(pixmap.height())
		.arg(QString("data:image/png;base64,%2").arg(QString(bytes.toBase64())));

	QFile destFile(newFilePath);
	if(!destFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QMessageBox::information(NULL, "", "file not created");
		if(!destFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
				QMessageBox::information(NULL, "", "file not created 2");
			}
	}
	QTextStream out(&destFile);
	out << svgDom;
	destFile.close();
	qDebug() << newFilePath;
	bool existsResult = QFileInfo(newFilePath).exists();
	Q_ASSERT(existsResult);
*/

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

QString PEMainWindow::makeSvgPath(SketchWidget * sketchWidget, bool useIndex)
{
    QString viewName = ViewLayer::viewIdentifierNaturalName(sketchWidget->viewIdentifier());
    QString indexString;
    if (useIndex) indexString = QString("_%3").arg(m_fileIndex++);
    return QString("%1/%2_%1%3.svg").arg(viewName).arg(m_guid).arg(indexString);
}

QString PEMainWindow::saveSvg(const QString & svg, const QString & newFilePath) {
    if (!TextUtils::writeUtf8(newFilePath, TextUtils::svgNSOnly(svg))) {
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

    reload();

    this->m_originalSvgPaths.insert(sketchWidget->viewIdentifier(), originalPath);
}

QString PEMainWindow::saveFzp() {
    QDir dir = QDir::temp();
    dir.mkdir(m_guid);
    dir.cd(m_guid);
    QString fzpPath = dir.absoluteFilePath(QString("%1_%2.fzp").arg(m_guid).arg(m_fileIndex++));   
    DebugDialog::debug("temp path " + fzpPath);
    TextUtils::writeUtf8(fzpPath, TextUtils::svgNSOnly(m_fzpDocument.toString()));
    return fzpPath;
}

void PEMainWindow::reload() {
    QString fzpPath = saveFzp();
    ModelPart * modelPart = new ModelPart(m_fzpDocument, fzpPath, ModelPart::Part);

    long newID = ItemBase::getNextID();
	ViewGeometry viewGeometry;
	viewGeometry.setLoc(QPointF(0, 0));

    ItemBase * iconItem = m_iconGraphicsView->addItem(modelPart, m_iconGraphicsView->defaultViewLayerSpec(), BaseCommand::SingleView, viewGeometry, newID, -1, NULL);
    ItemBase * breadboardItem = m_breadboardGraphicsView->addItem(modelPart, m_breadboardGraphicsView->defaultViewLayerSpec(), BaseCommand::SingleView, viewGeometry, newID, -1, NULL);
    ItemBase * schematicItem = m_schematicGraphicsView->addItem(modelPart, m_schematicGraphicsView->defaultViewLayerSpec(), BaseCommand::SingleView, viewGeometry, newID, -1, NULL);
    ItemBase * pcbItem = m_pcbGraphicsView->addItem(modelPart, m_pcbGraphicsView->defaultViewLayerSpec(), BaseCommand::SingleView, viewGeometry, newID, -1, NULL);
    m_metadataView->initMetadata(m_fzpDocument);

    initConnectors();

    initSvgTree(breadboardItem, m_breadboardDocument);
    initSvgTree(schematicItem, m_schematicDocument);
    initSvgTree(pcbItem, m_pcbDocument);
    initSvgTree(iconItem, m_iconDocument);  // TODO: full svg tree not necessary here, but need the icon document

    m_items.insert(m_iconGraphicsView->viewIdentifier(), iconItem);
    m_items.insert(m_breadboardGraphicsView->viewIdentifier(), breadboardItem);
    m_items.insert(m_schematicGraphicsView->viewIdentifier(), schematicItem);
    m_items.insert(m_pcbGraphicsView->viewIdentifier(), pcbItem);

    foreach (ItemBase * item, m_items.values()) {
        // TODO: may have to revisit this and move all pegi items
        item->setMoveLock(true);
    }

    m_breadboardGraphicsView->hideConnectors(true);
    m_schematicGraphicsView->hideConnectors(true);
    m_pcbGraphicsView->hideConnectors(true);

    switchedConnector(m_peToolView->currentConnector());

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
    foreach (PEGraphicsItem * pegi, pegiList) {
        pegi->setAcceptedMouseButtons(state ? Qt::NoButton : Qt::LeftButton);
    }
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

void PEMainWindow::pegiMouseReleased(PEGraphicsItem * pegi)
{
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
            switchedConnector(m_peToolView->currentConnector(), sketchWidget);
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
    QString savedModuleID = fzpRoot.attribute("moduleId");

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
	        QMessageBox messageBox(NULL);
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

    QList<SketchWidget *> sketchWidgets;
    sketchWidgets << m_breadboardGraphicsView << m_schematicGraphicsView << m_pcbGraphicsView << m_iconGraphicsView;
    foreach (SketchWidget * sketchWidget, sketchWidgets) {
        QDomElement view = views.firstChildElement(ViewLayer::viewIdentifierXmlName(sketchWidget->viewIdentifier()));
        QDomElement layers = view.firstChildElement("layers");

        QString currentSvgPath = layers.attribute("image");
        if (!currentSvgPath.contains(m_guid) && m_svgChangeCount.value(sketchWidget->viewIdentifier()) == 0) {
            // unaltered svg
            continue;
        }

        svgPaths.insert(sketchWidget->viewIdentifier(), currentSvgPath);
        QString svgPath = makeSvgPath(sketchWidget, false);

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
        QString svg = m_docs.value(sketchWidget->viewIdentifier())->toString();
        QString actualPath = svgOverWrite ? m_originalSvgPaths.value(sketchWidget->viewIdentifier()) : m_userPartsFolderSvgPath + svgPath; 
        bool result = TextUtils::writeUtf8(actualPath, TextUtils::svgNSOnly(svg));
        if (!result) {
            // TODO: warn user
        }
    }

    QDir dir(m_userPartsFolderPath);
    QString fzpPath = dir.absoluteFilePath(QString("%1.fzp").arg(m_guid));   
    if (overWrite) {
        fzpPath = m_originalFzpPath;
    }
    bool result = TextUtils::writeUtf8(fzpPath, TextUtils::svgNSOnly(m_fzpDocument.toString()));


    // restore the set of working svg files
    foreach (SketchWidget * sketchWidget, sketchWidgets) {
        QString svgPath = svgPaths.value(sketchWidget->viewIdentifier());
        if (svgPath.isEmpty()) continue;

        QDomElement view = views.firstChildElement(ViewLayer::viewIdentifierXmlName(sketchWidget->viewIdentifier()));
        QDomElement layers = view.firstChildElement("layers");
        layers.setAttribute("image", svgPath);
    }

	QString moduleID = fzpRoot.attribute("moduleId");
    fzpRoot.setAttribute("moduleId", savedModuleID);

    ModelPart * modelPart = m_refModel->retrieveModelPart(moduleID);
    if (modelPart == NULL) {
	    modelPart = m_refModel->loadPart(fzpPath, true);
        emit addToMyPartsSignal(modelPart);
	}
    else {
        m_refModel->reloadPart(fzpPath, m_originalModuleID);
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

        foreach (PEGraphicsItem * pegi, pegiList) {
            QDomElement pegiElement = pegi->element();
            if (pegiElement.attribute("id").compare(terminalID) == 0) {
                DebugDialog::debug("old pegi location", pegi->pos());
                pegiList.removeOne(pegi);
                delete pegi;
                break;
            }
        }

        double invdx = dx * size.width() / svgBounds.width();
        double invdy = dy * size.height() / svgBounds.height();
        QPointF topLeft = connectorPegi->offset() + p - QPointF(invdx, invdy);
        PEGraphicsItem * pegi = makePegi(QSizeF(invdx * 2, invdy * 2), topLeft, m_items.value(sketchWidget->viewIdentifier()), terminalElement);
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
    ZList.insert(itemBase->viewIdentifier(), z + 1);
    pegiItem->setZValue(z);
    itemBase->scene()->addItem(pegiItem);
    pegiItem->setElement(element);
    pegiItem->setOffset(topLeft);
    connect(pegiItem, SIGNAL(highlightSignal(PEGraphicsItem *)), this, SLOT(highlightSlot(PEGraphicsItem *)));
    connect(pegiItem, SIGNAL(mouseReleased(PEGraphicsItem *)), this, SLOT(pegiMouseReleased(PEGraphicsItem *)));
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
    m_peToolView->setEnabled(enabled);
    if (!enabled) m_peToolView->clearTexts();

    if (m_currentGraphicsView == NULL) setTitle();
}

void PEMainWindow::backupSketch()
{
}

void PEMainWindow::removeConnectors(QList<ConnectorMetadata *> & cmdList) 
{
    foreach (ConnectorMetadata * cmd, cmdList) {
        int index;
        QDomElement connector = findConnector(cmd->connectorID, index);
        if (connector.isNull()) continue;

        connector.parentNode().removeChild(connector);

    }

    killPegi();
    reload();
}

void PEMainWindow::addConnectors(QList<ConnectorMetadata *> & cmdList) 
{
    QDomElement root = m_fzpDocument.documentElement();
    QDomElement connectors = root.firstChildElement("connectors");
    if (connectors.isNull()) {
        // we are in big trouble: tell the user
        return;
    }

    foreach (ConnectorMetadata * cmd, cmdList) {
        QDomElement newConnector = m_fzpDocument.createElement("connector");

        int ix = 0;
        bool gotOne = false;
        QDomElement connector = connectors.firstChildElement("connector");
        while (!connector.isNull()) {
            if (ix++ == cmd->index) {
                // insert before
                connectors.insertBefore(newConnector, connector);
                gotOne = true;
                break;
            }

            connector = connector.nextSiblingElement("connector");
        }
        if (!gotOne) {
            // goes at the end
            connectors.appendChild(newConnector);
        }
        changeConnectorElement(newConnector, *cmd);
    }

    killPegi();
    reload();
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
    QList<SketchWidget *> sketchWidgets;
    sketchWidgets << m_breadboardGraphicsView << m_schematicGraphicsView << m_pcbGraphicsView << m_iconGraphicsView;
    foreach (SketchWidget * sketchWidget, sketchWidgets) {
        foreach (QGraphicsItem * item, sketchWidget->scene()->items()) {
            PEGraphicsItem * pegi = dynamic_cast<PEGraphicsItem *>(item);
            if (pegi) delete pegi;
        }
    }   
}