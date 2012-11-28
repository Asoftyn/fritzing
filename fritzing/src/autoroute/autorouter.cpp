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

#include "autorouter.h"
#include "../sketch/pcbsketchwidget.h"
#include "../debugdialog.h"
#include "../items/virtualwire.h"
#include "../items/tracewire.h"
#include "../items/jumperitem.h"
#include "../items/via.h"
#include "../utils/graphicsutils.h"
#include "../connectors/connectoritem.h"
#include "../items/moduleidnames.h"
#include "../processeventblocker.h"
#include "../referencemodel/referencemodel.h"

#include <qmath.h>
#include <QApplication>
#include <QSettings>

Autorouter::Autorouter(PCBSketchWidget * sketchWidget)
{
	m_sketchWidget = sketchWidget;
	m_stopTracing = m_cancelTrace = m_cancelled = false;
}

Autorouter::~Autorouter(void)
{
}

void Autorouter::cleanUpNets() {
	foreach (QList<ConnectorItem *> * connectorItems, m_allPartConnectorItems) {
		delete connectorItems;
	}
	m_allPartConnectorItems.clear();
}

void Autorouter::updateRoutingStatus() {
	RoutingStatus routingStatus;
	routingStatus.zero();
	m_sketchWidget->updateRoutingStatus(routingStatus, true);
}

TraceWire * Autorouter::drawOneTrace(QPointF fromPos, QPointF toPos, double width, ViewLayer::ViewLayerSpec viewLayerSpec)
{
    if (qAbs(fromPos.x() - toPos.x()) < 0.01 && qAbs(fromPos.y() - toPos.y()) < 0.01) {
        DebugDialog::debug("zero length trace", fromPos);
    }
    

	long newID = ItemBase::getNextID();
	ViewGeometry viewGeometry;
	viewGeometry.setWireFlags(m_sketchWidget->getTraceFlag());
	viewGeometry.setAutoroutable(true);
	viewGeometry.setLoc(fromPos);
	QLineF line(0, 0, toPos.x() - fromPos.x(), toPos.y() - fromPos.y());
	viewGeometry.setLine(line);

	ItemBase * trace = m_sketchWidget->addItem(m_sketchWidget->referenceModel()->retrieveModelPart(ModuleIDNames::WireModuleIDName), 
		                                 viewLayerSpec, BaseCommand::SingleView, viewGeometry, newID, -1, NULL);
	if (trace == NULL) {
		// we're in trouble
		DebugDialog::debug("autorouter unable to draw one trace");
		return NULL;
	}

	// addItem calls trace->setSelected(true) so unselect it (TODO: this may no longer be necessar)
	trace->setSelected(false);
	TraceWire * traceWire = dynamic_cast<TraceWire *>(trace);
	if (traceWire == NULL) {
		DebugDialog::debug("autorouter unable to draw one trace as trace");
		return NULL;
	}


	m_sketchWidget->setClipEnds(traceWire, false);
	traceWire->setColorString(m_sketchWidget->traceColor(viewLayerSpec), 1.0);
	traceWire->setWireWidth(width, m_sketchWidget, m_sketchWidget->getWireStrokeWidth(traceWire, width));

	return traceWire;
}

void Autorouter::cancel() {
	m_cancelled = true;
}

void Autorouter::cancelTrace() {
	m_cancelTrace = true;
}

void Autorouter::stopTracing() {
	m_stopTracing = true;
}

void Autorouter::initUndo(QUndoCommand * parentCommand) 
{
	// autoroutable traces, jumpers and vias are saved on the undo command and deleted
	// non-autoroutable jumpers and via are not deleted
	// non-autoroutable traces are saved as a copy and deleted: they are restored at each run of the autorouter

	// what happens when a non-autoroutable trace is split?

	QList<JumperItem *> jumperItems;
	QList<Via *> vias;
	QList<TraceWire *> traceWires;
	QList<ItemBase *> doNotAutorouteList;
	if (m_sketchWidget->usesJumperItem()) {
        QList<QGraphicsItem *> collidingItems = m_sketchWidget->scene()->collidingItems(m_board);
		foreach (QGraphicsItem * item, collidingItems) {
			JumperItem * jumperItem = dynamic_cast<JumperItem *>(item);
			if (jumperItem == NULL) continue;

			if (jumperItem->getAutoroutable()) {
				addUndoConnection(false, jumperItem, parentCommand);
				jumperItems.append(jumperItem);
				continue;
			}

			// deal with the traces connecting the jumperitem to the part
			QList<ConnectorItem *> both;
			foreach (ConnectorItem * ci, jumperItem->connector0()->connectedToItems()) both.append(ci);
			foreach (ConnectorItem * ci, jumperItem->connector1()->connectedToItems()) both.append(ci);
			foreach (ConnectorItem * connectorItem, both) {
				TraceWire * w = qobject_cast<TraceWire *>(connectorItem->attachedTo());
				if (w == NULL) continue;
				if (!w->isTraceType(m_sketchWidget->getTraceFlag())) continue;

				QList<Wire *> wires;
				QList<ConnectorItem *> ends;
				w->collectChained(wires, ends);
				foreach (Wire * wire, wires) {
					// make sure the jumper item doesn't lose its wires
					wire->setAutoroutable(false);
				}
			}
		}
		foreach (QGraphicsItem * item, collidingItems) {
			Via * via = dynamic_cast<Via *>(item);
			if (via == NULL) continue;

			if (via->getAutoroutable()) {
				addUndoConnection(false, via, parentCommand);
				vias.append(via);
				continue;
			}

			// deal with the traces connecting the via to the part
			QList<ConnectorItem *> both;
			foreach (ConnectorItem * ci, via->connectorItem()->connectedToItems()) both.append(ci);
			foreach (ConnectorItem * ci, via->connectorItem()->getCrossLayerConnectorItem()->connectedToItems()) both.append(ci);
			foreach (ConnectorItem * connectorItem, both) {
				TraceWire * w = qobject_cast<TraceWire *>(connectorItem->attachedTo());
				if (w == NULL) continue;
				if (!w->isTraceType(m_sketchWidget->getTraceFlag())) continue;

				QList<Wire *> wires;
				QList<ConnectorItem *> ends;
				w->collectChained(wires, ends);
				foreach (Wire * wire, wires) {
					// make sure the via doesn't lose its wires
					wire->setAutoroutable(false);
				}
			}
		}
	}

	foreach (QGraphicsItem * item, (m_board == NULL) ? m_sketchWidget->scene()->items() : m_sketchWidget->scene()->collidingItems(m_board)) {
		TraceWire * traceWire = dynamic_cast<TraceWire *>(item);
		if (traceWire == NULL) continue;
		if (!traceWire->isTraceType(m_sketchWidget->getTraceFlag())) continue;
		if (!traceWire->getAutoroutable()) continue;

		traceWires.append(traceWire);
		addUndoConnection(false, traceWire, parentCommand);
	}

	foreach (TraceWire * traceWire, traceWires) {
		m_sketchWidget->makeDeleteItemCommand(traceWire, BaseCommand::CrossView, parentCommand);
	}
	foreach (JumperItem * jumperItem, jumperItems) {
		m_sketchWidget->makeDeleteItemCommand(jumperItem, BaseCommand::CrossView, parentCommand);
	}
	foreach (Via * via, vias) {
		m_sketchWidget->makeDeleteItemCommand(via, BaseCommand::CrossView, parentCommand);
	}
	
	foreach (TraceWire * traceWire, traceWires) {
		m_sketchWidget->deleteItem(traceWire, true, false, false);
	}
	foreach (JumperItem * jumperItem, jumperItems) {
		m_sketchWidget->deleteItem(jumperItem, true, true, false);
	}
	foreach (Via * via, vias) {
		m_sketchWidget->deleteItem(via, true, true, false);
	}
}

void Autorouter::addUndoConnection(bool connect, Via * via, QUndoCommand * parentCommand) {
	addUndoConnection(connect, via->connectorItem(), BaseCommand::CrossView, parentCommand);
	addUndoConnection(connect, via->connectorItem()->getCrossLayerConnectorItem(), BaseCommand::CrossView, parentCommand);
}

void Autorouter::addUndoConnection(bool connect, JumperItem * jumperItem, QUndoCommand * parentCommand) {
	addUndoConnection(connect, jumperItem->connector0(), BaseCommand::CrossView, parentCommand);
	addUndoConnection(connect, jumperItem->connector1(), BaseCommand::CrossView, parentCommand);
}

void Autorouter::addUndoConnection(bool connect, TraceWire * traceWire, QUndoCommand * parentCommand) {
	addUndoConnection(connect, traceWire->connector0(), BaseCommand::CrossView, parentCommand);
	addUndoConnection(connect, traceWire->connector1(), BaseCommand::CrossView, parentCommand);
}

void Autorouter::addUndoConnection(bool connect, ConnectorItem * connectorItem, BaseCommand::CrossViewType crossView, QUndoCommand * parentCommand) 
{
	foreach (ConnectorItem * toConnectorItem, connectorItem->connectedToItems()) {
		VirtualWire * vw = qobject_cast<VirtualWire *>(toConnectorItem->attachedTo());
		if (vw != NULL) continue;

		ChangeConnectionCommand * ccc = new ChangeConnectionCommand(m_sketchWidget, crossView, 
												toConnectorItem->attachedToID(), toConnectorItem->connectorSharedID(),
												connectorItem->attachedToID(), connectorItem->connectorSharedID(),
												ViewLayer::specFromID(toConnectorItem->attachedToViewLayerID()),
												connect, parentCommand);
		ccc->setUpdateConnections(false);
	}
}

void Autorouter::restoreOriginalState(QUndoCommand * parentCommand) {
	QUndoStack undoStack;
	undoStack.push(parentCommand);
	undoStack.undo();
}

void Autorouter::clearTracesAndJumpers() {
	QList<Via *> vias;
	QList<JumperItem *> jumperItems;
	QList<TraceWire *> traceWires;

	foreach (QGraphicsItem * item, (m_board == NULL) ? m_sketchWidget->scene()->items() : m_sketchWidget->scene()->collidingItems(m_board)) {
		if (m_sketchWidget->usesJumperItem()) {
			JumperItem * jumperItem = dynamic_cast<JumperItem *>(item);
			if (jumperItem != NULL) {
				if (jumperItem->getAutoroutable()) {
					jumperItems.append(jumperItem);
				}
				continue;
			}
			Via * via = dynamic_cast<Via *>(item);
			if (via != NULL) {
				if (via->getAutoroutable()) {
					vias.append(via);
				}
				continue;
			}
		}

		TraceWire * traceWire = dynamic_cast<TraceWire *>(item);
		if (traceWire != NULL) {
			if (traceWire->isTraceType(m_sketchWidget->getTraceFlag()) && traceWire->getAutoroutable()) {
				traceWires.append(traceWire);
			}
			continue;
		}
	}

	foreach (Wire * traceWire, traceWires) {
		m_sketchWidget->deleteItem(traceWire, true, false, false);
	}
	foreach (JumperItem * jumperItem, jumperItems) {
		m_sketchWidget->deleteItem(jumperItem, true, true, false);
	}
	foreach (Via * via, vias) {
		m_sketchWidget->deleteItem(via, true, true, false);
	}
}

void Autorouter::doCancel(QUndoCommand * parentCommand) {
	restoreOriginalState(parentCommand);
	cleanUpNets();
}

void Autorouter::addToUndo(QMultiHash<TraceWire *, long> & splitDNA, QUndoCommand * parentCommand) 
{
	foreach (TraceWire * traceWire, splitDNA.uniqueKeys()) {
		// original doNotAutoroute wire has been split so delete it here because it has been replaced
		addUndoConnection(false, traceWire, parentCommand);
		m_sketchWidget->makeDeleteItemCommand(traceWire, BaseCommand::CrossView, parentCommand);
	}

	QList<long> newDNA = splitDNA.values();
	QList<TraceWire *> wires;
	QList<JumperItem *> jumperItems;	
	QList<Via *> vias;
	foreach (QGraphicsItem * item, (m_board  == NULL) ? m_sketchWidget->scene()->items() : m_sketchWidget->scene()->collidingItems(m_board)) {
		TraceWire * wire = dynamic_cast<TraceWire *>(item);
		if (wire != NULL) {
			if (!wire->getAutoroutable()) continue;
			if (!wire->isTraceType(m_sketchWidget->getTraceFlag())) continue;

			m_sketchWidget->setClipEnds(wire, true);
			wire->update();
			bool ra = false;
			if (newDNA.contains(wire->id())) {
				wire->setAutoroutable(false);
				ra = true;
			}
			addWireToUndo(wire, parentCommand);
			if (ra) {
				wire->setAutoroutable(true);
			}
			wires.append(wire);
			continue;
		}
		if (m_sketchWidget->usesJumperItem()) {
			JumperItem * jumperItem = dynamic_cast<JumperItem *>(item);	
			if (jumperItem != NULL) {
				jumperItems.append(jumperItem);
				if (!jumperItem->getAutoroutable()) {
					continue;
				}

				jumperItem->saveParams();
				QPointF pos, c0, c1;
				jumperItem->getParams(pos, c0, c1);

				new AddItemCommand(m_sketchWidget, BaseCommand::CrossView, ModuleIDNames::JumperModuleIDName, jumperItem->viewLayerSpec(), jumperItem->getViewGeometry(), jumperItem->id(), false, -1, parentCommand);
				new ResizeJumperItemCommand(m_sketchWidget, jumperItem->id(), pos, c0, c1, pos, c0, c1, parentCommand);
				new CheckStickyCommand(m_sketchWidget, BaseCommand::SingleView, jumperItem->id(), false, CheckStickyCommand::RemoveOnly, parentCommand);

				continue;
			}
			Via * via = dynamic_cast<Via *>(item);	
			if (via != NULL) {
				vias.append(via);
				if (!via->getAutoroutable()) {
					continue;
				}

				new AddItemCommand(m_sketchWidget, BaseCommand::CrossView, ModuleIDNames::ViaModuleIDName, via->viewLayerSpec(), via->getViewGeometry(), via->id(), false, -1, parentCommand);
				new CheckStickyCommand(m_sketchWidget, BaseCommand::SingleView, via->id(), false, CheckStickyCommand::RemoveOnly, parentCommand);
				new SetPropCommand(m_sketchWidget, via->id(), "hole size", via->holeSize(), via->holeSize(), true, parentCommand);

				continue;
			}
		}
	}

	foreach (TraceWire * traceWire, wires) {
		//traceWire->debugInfo("trace");
		addUndoConnection(true, traceWire, parentCommand);
	}
	foreach (JumperItem * jumperItem, jumperItems) {
		addUndoConnection(true, jumperItem, parentCommand);
	}
	foreach (Via * via, vias) {
		addUndoConnection(true, via, parentCommand);
	}
}

void Autorouter::addWireToUndo(Wire * wire, QUndoCommand * parentCommand) 
{  
	new AddItemCommand(m_sketchWidget, BaseCommand::CrossView, ModuleIDNames::WireModuleIDName, wire->viewLayerSpec(), wire->getViewGeometry(), wire->id(), false, -1, parentCommand);
	new CheckStickyCommand(m_sketchWidget, BaseCommand::SingleView, wire->id(), false, CheckStickyCommand::RemoveOnly, parentCommand);
	
	new WireWidthChangeCommand(m_sketchWidget, wire->id(), wire->width(), wire->width(), parentCommand);
	new WireColorChangeCommand(m_sketchWidget, wire->id(), wire->colorString(), wire->colorString(), wire->opacity(), wire->opacity(), parentCommand);
}

void Autorouter::setMaxCycles(int maxCycles) 
{
	m_maxCycles = maxCycles;
	QSettings settings;
	settings.setValue("cmrouter/maxcycles", maxCycles);
}

void Autorouter::removeOffBoard(bool isPCBType, bool removeSingletons) {
    QRectF boardRect;
    if (m_board) boardRect = m_board->sceneBoundingRect();
	// remove any vias or jumperItems that will be deleted, also remove off-board items
	for (int i = m_allPartConnectorItems.count() - 1; i >= 0; i--) {
		QList<ConnectorItem *> * connectorItems = m_allPartConnectorItems.at(i);
        if (removeSingletons) {
            if (connectorItems->count() < 2) {
                connectorItems->clear();
            }
            else if (connectorItems->count() == 2) {
                if (connectorItems->at(0) == connectorItems->at(1)->getCrossLayerConnectorItem()) {
                    connectorItems->clear();
                }
            }
        }
		for (int j = connectorItems->count() - 1; j >= 0; j--) {
			ConnectorItem * connectorItem = connectorItems->at(j);
			//connectorItem->debugInfo("pci");
			bool doRemove = false;
			if (connectorItem->attachedToItemType() == ModelPart::Via) {
				Via * via = qobject_cast<Via *>(connectorItem->attachedTo()->layerKinChief());
				doRemove = via->getAutoroutable();
			}
			else if (connectorItem->attachedToItemType() == ModelPart::Jumper) {
				JumperItem * jumperItem = qobject_cast<JumperItem *>(connectorItem->attachedTo()->layerKinChief());
				doRemove = jumperItem->getAutoroutable();
			}
            if (!doRemove && isPCBType) {
                if (!connectorItem->sceneBoundingRect().intersects(boardRect)) {
                    doRemove = true;
                }
            }
			if (doRemove) {
				connectorItems->removeAt(j);
			}
		}
		if (connectorItems->count() == 0) {
			m_allPartConnectorItems.removeAt(i);
			delete connectorItems;
		}
	}
}