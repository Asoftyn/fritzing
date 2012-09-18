/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2011 Fachhochschule Potsdam - http://fh-potsdam.de

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


#ifndef PEMAINWINDOW_H_
#define PEMAINWINDOW_H_


#include "../mainwindow/mainwindow.h"
#include "../model/modelpartshared.h"
#include "../sketch/sketchwidget.h"
#include "peconnectorsview.h"

class IconSketchWidget : public SketchWidget
{
	Q_OBJECT

public:
    IconSketchWidget(ViewLayer::ViewIdentifier, QWidget *parent=0);

	void addViewLayers();
};


class PEMainWindow : public MainWindow
{
Q_OBJECT

public:
	PEMainWindow(class ReferenceModel * referenceModel, QWidget * parent);
	~PEMainWindow();

    void setInitialItem(class PaletteItem *);
    void changeTags(const QStringList &, bool updateDisplay);
    void changeProperties(const QHash<QString, QString> &, bool updateDisplay);
    void changeMetadata(const QString & name, const QString & value, bool updateDisplay);
    void changeConnectorMetadata(ConnectorMetadata *, bool updateDisplay);
    void changeSvg(SketchWidget *, const QString & filename, const QString & originalPath, int changeDirection);
    void relocateConnectorSvg(SketchWidget *, const QString & id, const QString & terminalID, const QString & oldGorn, const QString & oldGornTerminal, const QString & newGorn, const QString & newGornTerminal, int changeDirection);
    void moveTerminalPoint(SketchWidget *, const QString & id, QSizeF, QPointF, int changeDirection);
    void restoreFzp(const QString & filename);
    bool editsModuleID(const QString &);

signals:
    void addToMyPartsSignal(ModelPart *);

public slots:
    void metadataChanged(const QString & name, const QString & value);
    void propertiesChanged(const QHash<QString, QString> &);
    void tagsChanged(const QStringList &);
    void connectorMetadataChanged(struct ConnectorMetadata *);
    void removedConnectors(QList<ConnectorMetadata *> &);
    void highlightSlot(class PEGraphicsItem *);
    void pegiMouseReleased(class PEGraphicsItem *);
    void pegiTerminalPointMoved(class PEGraphicsItem *, QPointF);
    void pegiTerminalPointChanged(class PEGraphicsItem *, QPointF before, QPointF after);
    void switchedConnector(const QDomElement &);
    void removedConnector(const QDomElement &);
    void terminalPointChanged(const QString & how);
    void terminalPointChanged(const QString & coord, double value);
    void getSpinAmount(double & amount);
    void lockChanged(bool);
    void connectorCountChanged(int);

protected:
	void closeEvent(QCloseEvent * event);
    void initLockedFiles(bool lockFiles);
    void initSketchWidgets();
    void initDock();
    void initHelper();
    void moreInitDock();
    void createActions();
    void createMenus();
    QList<QWidget*> getButtonsForView(ViewLayer::ViewIdentifier);
    void connectPairs();
	QMenu *breadboardItemMenu();
	QMenu *schematicItemMenu();
	QMenu *pcbItemMenu();
	QMenu *pcbWireMenu();
	QMenu *schematicWireMenu();
	QMenu *breadboardWireMenu();
    bool eventFilter(QObject *obj, QEvent *event);
	void setTitle();
    void createViewMenuActions();
    void createViewMenu();
    QHash<QString, QString> getOldProperties();
    QDomElement findConnector(const QString & id, int & index);
    void changeConnectorElement(QDomElement & connector, ConnectorMetadata *);
    void initSvgTree(ItemBase *, QDomDocument &);
    void initConnectors();
    QString createSvgFromImage(const QString &origFilePath);
    QString makeSvgPath(const QString & referenceFile, SketchWidget * sketchWidget, bool useIndex);
    QString saveSvg(const QString & svg, const QString & newFilePath);
    QString saveFzp();
    void reload();
    void createFileMenu();
    bool getConnectorIDs(const QDomElement & element, SketchWidget * sketchWidget, QString & id, QString & terminalID);
    QDomElement getConnectorPElement(const QDomElement & element, SketchWidget * sketchWidget);
    void updateChangeCount(SketchWidget * sketchWidget, int changeDirection);
    class PEGraphicsItem * findConnectorItem();
    void terminalPointChangedAux(PEGraphicsItem * pegi, QPointF before, QPointF after);
    void showInOS(QWidget *parent, const QString &pathIn);
    void switchedConnector(const QDomElement &, SketchWidget *, bool lock);
    PEGraphicsItem * makePegi(QSizeF size, QPointF topLeft, ItemBase * itemBase, QDomElement & element);
    QRectF getPixelBounds(FSvgRenderer & renderer, QDomElement & element);
    bool canSave();
    bool saveAs(bool overWrite);
    void lockChangedAux(bool state, const QList<PEGraphicsItem *> & pegiList); 
    void setBeforeClosingText(const QString & filename, QMessageBox & messageBox);
    QString getPartTitle();
    void killPegi();
    bool loadFzp(const QString & path);
    void removedConnectorsAux(QList<QDomElement> & connectors);
    QString getFzpReferenceFile();
    QString getSvgReferenceFile(const QString & filename);
    QString makeDesc(const QString & referenceFile);

protected slots:
    void initZoom();
    void showMetadataView();
    void showConnectorsView();
    void showIconView();
    void loadImage();
    bool save();
    bool saveAs();
    void showInOS();
    void tabWidget_currentChanged(int index);
    void backupSketch();

protected:
    QDomDocument m_fzpDocument;
    QDomDocument m_iconDocument;
    QDomDocument m_breadboardDocument;
    QDomDocument m_schematicDocument;
    QDomDocument m_pcbDocument;

    QAction * m_showMetadataViewAct;
    QAction * m_showConnectorsViewAct;
    QAction * m_showIconAct;
    QAction * m_showInOSAct;

	QPointer<SketchAreaWidget> m_iconWidget;
	QPointer<class IconSketchWidget> m_iconGraphicsView;
    class PEMetadataView * m_metadataView;
    class PEConnectorsView * m_connectorsView;
    class PEToolView * m_peToolView;
    class PESvgView * m_peSvgView;
    QString m_guid;
    int m_fileIndex;
    QHash<ViewLayer::ViewIdentifier, ItemBase *> m_items;
    QHash<ViewLayer::ViewIdentifier, QDomDocument *> m_docs;
    QHash<ViewLayer::ViewIdentifier, int> m_svgChangeCount;
    QHash<ViewLayer::ViewIdentifier, bool> m_everZoomed;
    QList<SketchWidget *> m_sketchWidgets;
    QString m_userPartsFolderPath;
    QString m_userPartsFolderSvgPath;
    bool m_canSave;
    QString m_originalFzpPath;
    QString m_originalModuleID;
    QHash<ViewLayer::ViewIdentifier, QString> m_originalSvgPaths;
    bool m_gaveSaveWarning;
};

#endif /* PEMAINWINDOW_H_ */
