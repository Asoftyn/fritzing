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
package org.fritzing.model;

import org.eclipse.swt.graphics.Image;
import org.fritzing.FritzingMessages;

public class OrGate
	extends Gate 
{

static private Image OR_ICON = createImage(OrGate.class, "icons/or16.gif");  //$NON-NLS-1$
static final long serialVersionUID = 1;

public Image getIconImage() {
	return OR_ICON;
}

public boolean getResult() {
	return getInput(TERMINAL_A) | getInput(TERMINAL_B);
}

public String toString() {
	return FritzingMessages.OrGate_LabelText + " #" + getID();  //$NON-NLS-1$
}

}
