/*******************************************************************************
 * Copyright (c) 2000, 2005 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *     IBM Corporation - initial API and implementation
 *******************************************************************************/
package org.fritzing.edit;

import java.util.List;

import org.eclipse.draw2d.geometry.Dimension;
import org.eclipse.draw2d.geometry.Rectangle;
import org.eclipse.gef.EditPart;
import org.eclipse.gef.commands.Command;
import org.eclipse.gef.commands.CompoundCommand;
import org.eclipse.gef.commands.UnexecutableCommand;
import org.eclipse.gef.editpolicies.TreeContainerEditPolicy;
import org.eclipse.gef.requests.ChangeBoundsRequest;
import org.eclipse.gef.requests.CreateRequest;
import org.fritzing.model.FritzingDiagram;
import org.fritzing.model.FritzingSubpart;
import org.fritzing.model.commands.CreateCommand;
import org.fritzing.model.commands.ReorderPartCommand;

public class FritzingTreeContainerEditPolicy 
	extends TreeContainerEditPolicy 
{
	
protected Command createCreateCommand(FritzingSubpart child, Rectangle r, 
						 int index, String label){
	CreateCommand cmd = new CreateCommand();
	Rectangle rect;
	if(r == null){
		rect = new Rectangle();
		rect.setSize(new Dimension(-1,-1));
	} else {
		rect = r;
	}
	cmd.setLocation(rect);
	cmd.setParent((FritzingDiagram)getHost().getModel());
	cmd.setChild(child);
	cmd.setLabel(label);
	if(index >= 0)
		cmd.setIndex(index);
	return cmd;
}

protected Command getAddCommand(ChangeBoundsRequest request){
	CompoundCommand command = new CompoundCommand();
	command.setDebugLabel("Add in FritzingTreeContainerEditPolicy");//$NON-NLS-1$
	List editparts = request.getEditParts();
	int index = findIndexOfTreeItemAt(request.getLocation());
	
	for(int i = 0; i < editparts.size(); i++){
		EditPart child = (EditPart)editparts.get(i);
		if(isAncestor(child,getHost()))
			command.add(UnexecutableCommand.INSTANCE);
		else {
			FritzingSubpart childModel = (FritzingSubpart)child.getModel();
			command.add(createCreateCommand(
						childModel,
						new Rectangle(new org.eclipse.draw2d.geometry.Point(),
							childModel.getSize()),
						index, "Reparent FritzingSubpart"));//$NON-NLS-1$
		}
	}
	return command;
}

protected Command getCreateCommand(CreateRequest request){
	FritzingSubpart child = (FritzingSubpart)request.getNewObject();
	int index = findIndexOfTreeItemAt(request.getLocation());
	return createCreateCommand(child, null, index, "Create FritzingSubpart");//$NON-NLS-1$
}

protected Command getMoveChildrenCommand(ChangeBoundsRequest request){
	CompoundCommand command = new CompoundCommand();
	List editparts = request.getEditParts();
	List children = getHost().getChildren();
	int newIndex = findIndexOfTreeItemAt(request.getLocation());
		
	for(int i = 0; i < editparts.size(); i++){
		EditPart child = (EditPart)editparts.get(i);
		int tempIndex = newIndex;
		int oldIndex = children.indexOf(child);
		if(oldIndex == tempIndex || oldIndex + 1 == tempIndex){
			command.add(UnexecutableCommand.INSTANCE);
			return command;
		} else if(oldIndex <= tempIndex){
			tempIndex--;
		}
		command.add(new ReorderPartCommand(
					(FritzingSubpart)child.getModel(), 
					(FritzingDiagram)getHost().getModel(), 
					tempIndex));
	}
	return command;
}

protected boolean isAncestor(EditPart source, EditPart target){
	if(source == target)
		return true;
	if(target.getParent() != null)
		return isAncestor(source, target.getParent());
	return false;
}

}
