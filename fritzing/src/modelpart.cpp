/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2009 Fachhochschule Potsdam - http://fh-potsdam.de

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

#include "modelpart.h"
#include "debugdialog.h"
#include "connectorshared.h"
#include "busshared.h"
#include "bus.h"
#include "version/version.h"

#include <QDomElement>

QHash<ModelPart::ItemType, QString> ModelPart::itemTypeNames;
long ModelPart::m_nextIndex = 0;
const int ModelPart::indexMultiplier = 10;
QString ModelPart::customSizeTranslated;
QString ModelPart::customShapeTranslated;

ModelPart::ModelPart(ItemType type)
	: QObject()
{
	m_size = QSizeF(0,0);
	m_type = type;
	m_modelPartShared = NULL;
	m_index = m_nextIndex++;
	m_originalIndex = -1;
	m_core = false;
	m_alien = false;
	m_originalModelPartShared = false;
}

ModelPart::ModelPart(QDomDocument * domDocument, const QString & path, ItemType type)
	: QObject()
{
	m_size = QSizeF(0,0);
	m_type = type;
	m_modelPartShared = new ModelPartShared(domDocument, path);
	m_originalModelPartShared = true;
	m_core = false;
	m_alien = false;
	m_originalIndex = -1;

	//TODO Mariano: enough for now
	QDomElement viewsElems = domDocument->documentElement().firstChildElement("views");
	if(!viewsElems.isNull()) {
		m_valid = !viewsElems.firstChildElement(ViewIdentifierClass::viewIdentifierXmlName(ViewIdentifierClass::IconView)).isNull();
	} else {
		m_valid = false;
	}
}

ModelPart::~ModelPart() {
	//DebugDialog::debug(QString("deleting modelpart %1 %2").arg((long) this, 0, 16).arg(m_index));
	foreach (ItemBase * itemBase, m_viewItems) {
		itemBase->clearModelPart();
	}
	if (m_originalModelPartShared) {
		if (m_modelPartShared) {
			delete m_modelPartShared;
		}
	}

	foreach (Connector * connector, m_connectorHash.values()) {
		delete connector;
	}
	m_connectorHash.clear();

	foreach (Bus* bus, m_busHash.values()) {
		delete bus;
	}
	m_busHash.clear();
}

const QString & ModelPart::moduleID() {
	if (m_modelPartShared != NULL) return m_modelPartShared->moduleID();

	return ___emptyString___;
}


const QString & ModelPart::itemTypeName(ModelPart::ItemType itemType) {
	return itemTypeNames[itemType];
}

const QString & ModelPart::itemTypeName(int itemType) {
	return itemTypeNames[(ModelPart::ItemType) itemType];
}

void ModelPart::initNames() {
	if (itemTypeNames.count() == 0) {
		itemTypeNames.insert(ModelPart::Part, QObject::tr("part"));
		itemTypeNames.insert(ModelPart::Wire, QObject::tr("wire"));
		itemTypeNames.insert(ModelPart::Breadboard, QObject::tr("breadboard"));
		itemTypeNames.insert(ModelPart::Board, QObject::tr("board"));
		itemTypeNames.insert(ModelPart::Board, QObject::tr("resizable"));
		itemTypeNames.insert(ModelPart::Module, QObject::tr("module"));
	}
}

void ModelPart::setItemType(ItemType t) {
	m_type = t;
}


void ModelPart::copy(ModelPart * modelPart) {
	m_type = modelPart->itemType();
	m_modelPartShared = modelPart->modelPartShared();
	m_core = modelPart->isCore();
}

void ModelPart::copyNew(ModelPart * modelPart) {
	copy(modelPart);
}

void ModelPart::copyStuff(ModelPart * modelPart) {
	modelPartShared()->copy(modelPart->modelPartShared());
}

ModelPartShared * ModelPart::modelPartShared() {
	if(!m_modelPartShared) {
		m_modelPartShared = new ModelPartShared();
		m_originalModelPartShared = true;
	}
	return m_modelPartShared;
}
void ModelPart::setModelPartShared(ModelPartShared * modelPartShared) {
	m_modelPartShared = modelPartShared;
}

void ModelPart::addViewItem(ItemBase * item) {
	m_viewItems.append(item);
}

void ModelPart::removeViewItem(ItemBase * item) {
	m_viewItems.removeOne(item);
}

ItemBase * ModelPart::viewItem(QGraphicsScene * scene) {
	foreach (ItemBase * itemBase, m_viewItems) {
		if (itemBase->scene() == scene) return itemBase;
	}

	return NULL;
}

void ModelPart::saveInstances(QXmlStreamWriter & streamWriter, bool startDocument) {
	if (startDocument) {
		streamWriter.writeStartDocument();
    	streamWriter.writeStartElement("module");
		streamWriter.writeAttribute("fritzingVersion", Version::versionString());
		QString title = this->modelPartShared()->title();
		if(!title.isNull() && !title.isEmpty()) {
			streamWriter.writeTextElement("title",title);
		}
		streamWriter.writeStartElement("instances");
	}

	if (m_viewItems.size() > 0) {
		streamWriter.writeStartElement("instance");
		if (m_modelPartShared != NULL) {
			const QString & moduleIdRef = m_modelPartShared->moduleID();
			streamWriter.writeAttribute("moduleIdRef", moduleIdRef);
			streamWriter.writeAttribute("modelIndex", QString::number(m_index));
			streamWriter.writeAttribute("path", m_modelPartShared->path());
			if (m_size.width() != 0) {
				streamWriter.writeAttribute("width", QString::number(m_size.width()));
			}
			if (m_size.height() != 0) {
				streamWriter.writeAttribute("height", QString::number(m_size.height()));
			}
		}
		QString title = instanceTitle();
		if(!title.isNull() && !title.isEmpty()) {
			writeTag(streamWriter,"title",title);
		}

		QString text = instanceText();
		if(!text.isNull() && !text.isEmpty()) {
			streamWriter.writeStartElement("text");
			streamWriter.writeCharacters(text);
			streamWriter.writeEndElement();
		}

		// tell the views to write themselves out
		streamWriter.writeStartElement("views");
		foreach (ItemBase * itemBase, m_viewItems) {
			itemBase->saveInstance(streamWriter);
		}
		streamWriter.writeEndElement();		// views
		streamWriter.writeEndElement();		//instance
	}

	if (this->itemType() != ModelPart::Module) {

		QList<QObject *> children = this->children();
		if(m_orderedChildren.count() > 0) {
			children = m_orderedChildren;
		}

		QList<QObject *>::const_iterator i;
		for (i = children.constBegin(); i != children.constEnd(); ++i) {
			ModelPart* mp = qobject_cast<ModelPart *>(*i);
			if (mp == NULL) continue;

			mp->saveInstances(streamWriter, false);
		}
	}

	if (startDocument) {
		streamWriter.writeEndElement();	  //  instances
		streamWriter.writeEndElement();   //  module
		streamWriter.writeEndDocument();
	}
}

void ModelPart::writeTag(QXmlStreamWriter & streamWriter, QString tagName, QString tagValue) {
	if(!tagValue.isEmpty()) {
		streamWriter.writeTextElement(tagName,tagValue);
	}
}

void ModelPart::writeNestedTag(QXmlStreamWriter & streamWriter, QString tagName, const QStringList &values, QString childTag) {
	if(values.count() > 0) {
		streamWriter.writeStartElement(tagName);
		for(int i=0; i<values.count(); i++) {
			writeTag(streamWriter, childTag, values[i]);
		}
		streamWriter.writeEndElement();
	}
}

void ModelPart::writeNestedTag(QXmlStreamWriter & streamWriter, QString tagName, const QHash<QString,QString> &values, QString childTag, QString attrName) {
	streamWriter.writeStartElement(tagName);
	for(int i=0; i<values.keys().count(); i++) {
		streamWriter.writeStartElement(childTag);
		QString key = values.keys()[i];
		streamWriter.writeAttribute(attrName,key);
		streamWriter.writeCharacters(values[key]);
		streamWriter.writeEndElement();
	}
	streamWriter.writeEndElement();
}

void ModelPart::saveAsPart(QXmlStreamWriter & streamWriter, bool startDocument) {
	if (startDocument) {
		streamWriter.writeStartDocument();
    	streamWriter.writeStartElement("module");
		streamWriter.writeAttribute("fritzingVersion", Version::versionString());
		streamWriter.writeAttribute("moduleId", m_modelPartShared->moduleID());
    	writeTag(streamWriter,"version",m_modelPartShared->version());
    	writeTag(streamWriter,"author",m_modelPartShared->author());
    	writeTag(streamWriter,"title",m_modelPartShared->title());
    	writeTag(streamWriter,"label",m_modelPartShared->label());
    	writeTag(streamWriter,"date",m_modelPartShared->dateAsStr());

    	writeNestedTag(streamWriter,"tags",m_modelPartShared->tags(),"tag");
    	writeNestedTag(streamWriter,"properties",m_modelPartShared->properties(),"property","name");

    	writeTag(streamWriter,"taxonomy",m_modelPartShared->taxonomy());
    	writeTag(streamWriter,"description",m_modelPartShared->description());
	}

	if (m_viewItems.size() > 0) {
		if (startDocument) {
			streamWriter.writeStartElement("views");
		}
		for (int i = 0; i < m_viewItems.size(); i++) {
			ItemBase * item = m_viewItems[i];
			item->writeXml(streamWriter);
		}

		if(startDocument) {
			streamWriter.writeEndElement();
		}

		streamWriter.writeStartElement("connectors");
		const QList<ConnectorShared *> connectors = m_modelPartShared->connectors();
		for (int i = 0; i < connectors.count(); i++) {
			Connector * connector = new Connector(connectors[i], this);
			connector->saveAsPart(streamWriter);
			delete connector;
		}
		streamWriter.writeEndElement();
	}

	QList<QObject *>::const_iterator i;
    for (i = children().constBegin(); i != children().constEnd(); ++i) {
		ModelPart * mp = qobject_cast<ModelPart *>(*i);
		if (mp == NULL) continue;

		mp->saveAsPart(streamWriter, false);
	}

	if (startDocument) {
		streamWriter.writeEndElement();
		streamWriter.writeEndElement();
		streamWriter.writeEndDocument();
	}
}

void ModelPart::initConnectors(bool force) {
	if(m_modelPartShared == NULL) return;

	if(force) {
		m_connectorHash.clear();						// TODO: not deleting old connectors here causes a memory leak; but deleting them here causes a crash
		foreach (Bus * bus, m_busHash.values()) {
			delete bus;
		}
		m_busHash.clear();
	}
	if(m_connectorHash.count() > 0) return;		// already done

	m_modelPartShared->initConnectors();
	foreach (ConnectorShared * connectorShared, m_modelPartShared->connectors()) {
		Connector * connector = new Connector(connectorShared, this);
		m_connectorHash.insert(connectorShared->id(), connector);
		BusShared * busShared = connectorShared->bus();
		if (busShared != NULL) {
			Bus * bus = m_busHash.value(busShared->id());
			if (bus == NULL) {
				bus = new Bus(busShared, this);
				m_busHash.insert(busShared->id(), bus);
			}
			connector->setBus(bus);
			bus->addConnector(connector);
		}
	}
}

const QHash<QString, Connector *> & ModelPart::connectors() {
	return m_connectorHash;
}

long ModelPart::modelIndex() {
	return m_index;
}

long ModelPart::originalModelIndex() {
	return m_originalIndex;
}

void ModelPart::setModelIndex(long index) {
	m_index = index;
	updateIndex(index);
}

void ModelPart::setModelIndexFromMultiplied(long multiplied) {
	setModelIndex(multiplied / ModelPart::indexMultiplier);
}

void ModelPart::updateIndex(long index)
{
	if (index >= m_nextIndex) {
		m_nextIndex = index + 1;
	}
}

void ModelPart::setOriginalModelIndex(long index) {
	if (m_originalIndex > 0) {
		return;
	}

	m_originalIndex = index;
}

long ModelPart::nextIndex() {
	return m_nextIndex++;
}

void ModelPart::setInstanceDomElement(const QDomElement & domElement) {
	//DebugDialog::debug(QString("model part instance %1").arg((long) this, 0, 16));
	m_instanceDomElement = domElement;
}

const QDomElement & ModelPart::instanceDomElement() {
	return m_instanceDomElement;
}

const QString & ModelPart::title() {
	if (m_modelPartShared != NULL) return m_modelPartShared->title();

	return ___emptyString___;
}

const QStringList & ModelPart::tags() {
	if (m_modelPartShared != NULL) return m_modelPartShared->tags();

	return ___emptyStringList___;
}

const QHash<QString,QString> & ModelPart::properties() const {
	if (m_modelPartShared != NULL) return m_modelPartShared->properties();

	return ___emptyStringHash___;
}

Connector * ModelPart::getConnector(const QString & id) {
	return m_connectorHash.value(id);
}

const QHash<QString, Bus *> & ModelPart::buses() {
	return  m_busHash;
}

Bus * ModelPart::bus(const QString & busID) {
	return m_busHash.value(busID);
}

bool ModelPart::ignoreTerminalPoints() {
	if (m_modelPartShared != NULL) return m_modelPartShared->ignoreTerminalPoints();

	return true;
}

bool ModelPart::isCore() {
	return m_core;
}

void ModelPart::setCore(bool core) {
	m_core = core;
}

bool ModelPart::isAlien() {
	return m_alien;
}

void ModelPart::setAlien(bool alien) {
	m_alien = alien;
}

bool ModelPart::isValid() {
	return m_valid;
}

QList<ModelPart*> ModelPart::getAllNonCoreParts() {
	QList<ModelPart*> retval;
	QList<QObject *>::const_iterator i;
	for (i = children().constBegin(); i != children().constEnd(); ++i) {
		ModelPart* mp = qobject_cast<ModelPart *>(*i);
		if (mp == NULL) continue;

		if(!mp->isCore()) {
			retval << mp;
		}
	}

	return retval;
}

QList<SvgAndPartFilePath> ModelPart::getAvailableViewFiles() {
	QDomElement viewsElems = modelPartShared()->domDocument()->documentElement().firstChildElement("views");
	QHash<ViewIdentifierClass::ViewIdentifier, SvgAndPartFilePath> viewImages;

	grabImagePath(viewImages, viewsElems, ViewIdentifierClass::IconView);
	grabImagePath(viewImages, viewsElems, ViewIdentifierClass::BreadboardView);
	grabImagePath(viewImages, viewsElems, ViewIdentifierClass::SchematicView);
	grabImagePath(viewImages, viewsElems, ViewIdentifierClass::PCBView);

	return viewImages.values();
}

void ModelPart::grabImagePath(QHash<ViewIdentifierClass::ViewIdentifier, SvgAndPartFilePath> &viewImages, QDomElement &viewsElems, ViewIdentifierClass::ViewIdentifier viewId) {
	QDomElement viewElem = viewsElems.firstChildElement(ViewIdentifierClass::viewIdentifierXmlName(viewId));
	if(!viewElem.isNull()) {
		QString partspath = getApplicationSubFolderPath("parts")+"/svg";
		QDomElement layerElem = viewElem.firstChildElement("layers");
		if (!layerElem.isNull()) {
			QString imagepath = layerElem.attribute("image");
			QString folderinparts = inWhichFolder(partspath, imagepath);
			if(folderinparts != ___emptyString___) {
				SvgAndPartFilePath st(partspath,folderinparts,imagepath);
				viewImages[viewId] = st;
			}
		}
	}
}

QString ModelPart::inWhichFolder(const QString &partspath, const QString &imagepath) {
	QStringList possibleFolders;
	possibleFolders << "core" << "contrib" << "user";
	for(int i=0; i < possibleFolders.size(); i++) {
		if (QFileInfo( partspath+"/"+possibleFolders[i]+"/"+imagepath ).exists()) {
			return possibleFolders[i];
		}
	}
	return ___emptyString___;
}

bool ModelPart::hasViewID(long id) {
	foreach (ItemBase * item, m_viewItems) {
		if (item->id() == id) return true;
	}

	return false;
}

const QString & ModelPart::instanceTitle() {
	return m_instanceTitle;
}

const QString & ModelPart::instanceText() {
	return m_instanceText;
}

void ModelPart::setInstanceText(QString text) {
	m_instanceText = text;
}

void ModelPart::setInstanceTitle(QString title) {
	m_instanceTitle = title;
}

void ModelPart::setOrderedChildren(QList<QObject*> children) {
	m_orderedChildren = children;
}

void ModelPart::collectExtraValues(const QString & prop, QString & value, QStringList & extraValues) {
	if (itemType() != ModelPart::ResizableBoard) return;

	if (prop.compare("size") == 0) {
		if (customSizeTranslated.isEmpty()) {
			customSizeTranslated = tr("Custom Size");
		}
		if (customShapeTranslated.isEmpty()) {
			customShapeTranslated = tr("Import Shape...");
		}
		extraValues.append(customSizeTranslated);
		extraValues.append(customShapeTranslated);

		if (m_size.width() != 0) {
			value = customSizeTranslated;
		}
	}
}

QString ModelPart::collectExtraHtml(const QString & prop, const QString & value) {
	if (itemType() != ModelPart::ResizableBoard) return  ___emptyString___;

	if (prop.compare("size") != 0) return ___emptyString___;

	if (value.compare(customSizeTranslated) == 0) {
		if (m_size.width() == 0) return ___emptyString___;

		qreal w = qRound(m_size.width() * 10) / 10.0;	// truncate to 1 decimal point
		qreal h = qRound(m_size.height() * 10) / 10.0;  // truncate to 1 decimal point
		return QString("&nbsp;width(mm):<input type='text' name='boardwidth' id='boardwidth' maxlength='5' value='%1' style='width:35px' onblur='resizeBoardWidth()' onkeypress='resizeBoardWidthEnter(event)' />"
					   "&nbsp;height(mm):<input type='text' name='boardheight' id='boardheight' maxlength='5' value='%2' style='width:35px' onblur='resizeBoardHeight()' onkeypress='resizeBoardHeightEnter(event)' />"
					   "<script language='JavaScript'>lastGoodWidth=%1;lastGoodHeight=%2;</script>"
					   ).arg(w).arg(h);
	}
	else if (value.compare(customShapeTranslated) == 0) {
		return "<input type='button' value='image...' name='image...' id='image...' style='width:60px' onclick='loadBoardImage()'/>";
	}

	return ___emptyString___;
}

void ModelPart::setSize(QSizeF sz) {
	m_size = sz;
}

QSizeF ModelPart::size() {
	return m_size;
}
