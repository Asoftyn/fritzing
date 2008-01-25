/*
 * (c) Fachhochschule Potsdam
 */
package org.fritzing.fritzing.diagram.edit.parts;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.Assert;
import org.eclipse.draw2d.AutomaticRouter;
import org.eclipse.draw2d.Connection;
import org.eclipse.draw2d.ConnectionAnchor;
import org.eclipse.draw2d.ConnectionRouter;
import org.eclipse.draw2d.Graphics;
import org.eclipse.draw2d.geometry.Point;
import org.eclipse.draw2d.geometry.PointList;
import org.eclipse.emf.common.notify.Notification;
import org.eclipse.emf.ecore.impl.EReferenceImpl;
import org.eclipse.gef.ConnectionEditPart;
import org.eclipse.gef.EditPart;
import org.eclipse.gef.EditPolicy;
import org.eclipse.gef.Request;
import org.eclipse.gef.commands.Command;
import org.eclipse.gef.commands.UnexecutableCommand;
import org.eclipse.gef.editparts.AbstractConnectionEditPart;
import org.eclipse.gef.editpolicies.ConnectionEndpointEditPolicy;
import org.eclipse.gef.handles.ConnectionEndHandle;
import org.eclipse.gef.requests.GroupRequest;
import org.eclipse.gmf.runtime.diagram.ui.editparts.GraphicalEditPart;
import org.eclipse.gmf.runtime.diagram.ui.editpolicies.EditPolicyRoles;
import org.eclipse.gmf.runtime.diagram.ui.internal.editpolicies.ConnectionEditPolicy;
import org.eclipse.gmf.runtime.diagram.ui.requests.RequestConstants;
import org.eclipse.gmf.runtime.draw2d.ui.internal.routers.FanRouter;
import org.eclipse.gmf.runtime.draw2d.ui.internal.routers.ObliqueRouter;
import org.eclipse.gmf.runtime.notation.NotationPackage;
import org.eclipse.gmf.runtime.notation.View;
import org.eclipse.swt.graphics.Color;
import org.fritzing.fritzing.diagram.edit.parts.Terminal2EditPart.TerminalAnchor;
import org.fritzing.fritzing.diagram.edit.policies.LegItemSemanticEditPolicy;
import org.fritzing.fritzing.diagram.edit.policies.NonDeleteComponentEditPolicy;

/**
 * @generated
 */
public class LegEditPart extends ConnectionFritzingEditPart {

	/**
	 * @generated
	 */
	public static final int VISUAL_ID = 4003;

	/**
	 * @generated
	 */
	public LegEditPart(View view) {
		super(view);
	}
	
		
	public void setParent(EditPart parent) {		
		// AbstractConnectionEditPart.setSource() sets the parent very strangely so just take it over

		EditPart s = getSource();
		if (s instanceof SketchEditPart) {
			super.setParent(s);
		}
		else if (s instanceof Terminal2EditPart) {
			super.setParent(s.getParent());
		}
		else {
			super.setParent(parent);
		}
	}

	/**
	 * @generated NOT
	 */
	protected void createDefaultEditPolicies() {
		super.createDefaultEditPolicies();
		installEditPolicy(EditPolicyRoles.SEMANTIC_ROLE,
				new LegItemSemanticEditPolicy());
		installEditPolicy(EditPolicy.CONNECTION_ROLE,
				new LegItemConnectionEditPolicy());
		installEditPolicy(EditPolicy.CONNECTION_ENDPOINTS_ROLE,
				new LegItemConnectionEndpointEditPolicy());
		// don't want delete
		installEditPolicy(EditPolicy.COMPONENT_ROLE,
				new NonDeleteComponentEditPolicy());
	}
	
	public class LegItemConnectionEditPolicy extends ConnectionEditPolicy {
		protected Command createDeleteViewCommand(GroupRequest deleteRequest) {
			// disable delete for legs
			return UnexecutableCommand.INSTANCE;
		}

		protected Command createDeleteSemanticCommand(GroupRequest deleteRequest) {
			// disable delete for legs
			return UnexecutableCommand.INSTANCE;
		}
		
	}

	public class LegItemConnectionEndpointEditPolicy extends
			ConnectionEndpointEditPolicy {
		protected List createSelectionHandles() {
			// only show target handle, not source handle
			List list = new ArrayList();
			list.add(new ConnectionEndHandle((ConnectionEditPart) getHost()));
			return list;
		}

	}

//	protected void initBend(LegFigure figure, Point p) {
		//			BendpointRequest request = new BendpointRequest();
		//			request.setType(RequestConstants.REQ_CREATE_BENDPOINT);
		//			request.setIndex(0);
		//			request.setSource(this);
		//			request.setLocation(p);
		//			Command cmd = super.getCommand(request);
		//			if (cmd != null && cmd.canExecute()) {
		//				DiagramCommandStack commandStack = this.getDiagramEditDomain()
		//				.getDiagramCommandStack();
		//				commandStack.execute(cmd);		
		//			}
//	}

	protected boolean connectedToSketch() {
		return (this.getTarget() instanceof SketchEditPart);
	}

	public Command getCommand(Request request) {
		if (request.getType() == RequestConstants.REQ_CREATE_BENDPOINT) {
			if (this.getTarget() instanceof SketchEditPart) {
				return UnexecutableCommand.INSTANCE;
			}
		}
		return super.getCommand(request);
	}

	/**
	 * Creates figure for this edit part.
	 * 
	 * Body of this method does not depend on settings in generation model
	 * so you may safely remove <i>generated</i> tag and modify it.
	 * 
	 * @generated NOT
	 */

	protected Connection createConnectionFigure() {
		return new LegFigure(this);
	}

	/**
	 * @generated
	 */
	public LegFigure getPrimaryShape() {
		return (LegFigure) getFigure();
	}

	/**
	 * @generated
	 */
	public class LegFigure extends ConnectionFritzingFigure {

		LegEditPart leg;
		
		/**
		 * @generated NOT
		 */
		public LegFigure() {
			// XXX: do not use this!
		}

		/**
		 * @generated NOT
		 */
		public LegFigure(LegEditPart leg) {
			setLineWidth(connectionWidth = 3);
			this.leg = leg;
			createContents();
		}
		
		// useful for debugging purposes
//		public void paintFigure(Graphics graphics) {
//			//setRoutingStyles(true, false);
//			//this.setSmoothness(0);  // for debugging only; delete this  
//			super.paintFigure(graphics);
//		}
		

		/**
		 * @generated NOT
		 */
		private void createContents() {

		}
		
		public Object getRoutingConstraint() {
			// this isn't a clean fix, but it will do for now
			// basically, returning a null at this point stops any new bendpoint being created
			// the root problem is that when a leg is connected to the sketch,
			// adding a bendpoint to the leg results in the leg getting badly screwed up or disappearing completely
			// this problem would probably fix itself if we were connecting to a terminal on the sketch and not the sketch itself

			if (leg.connectedToSketch()) {
				return null;
			}

			return super.getRoutingConstraint();
		}
	}
	
	// XXX: this is hard-coded in LegViewFactory.decorateView() because of a Mac bug
	public static final Color LEG_FIGURE_COLOR = new Color(null, 178, 178, 178);

}
