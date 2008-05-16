/*
 * (c) Fachhochschule Potsdam
 */
package org.fritzing.fritzing.diagram.part;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;

import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Preferences;
import org.eclipse.core.runtime.Status;
import org.eclipse.emf.common.util.URI;
import org.eclipse.gmf.runtime.diagram.ui.parts.IDiagramWorkbenchPart;
import org.eclipse.jface.action.IAction;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.IWorkbenchWindowActionDelegate;
import org.fritzing.fritzing.diagram.export.Fritzing2Eagle;
import org.fritzing.fritzing.diagram.preferences.EaglePreferencePage;

/**
 * @generated NOT
 */
public class FritzingPCBExportAction implements IWorkbenchWindowActionDelegate {

	/**
	 * @generated NOT
	 */
	private IWorkbenchWindow window;

	/**
	 * @generated NOT
	 */
	public void init(IWorkbenchWindow window) {
		this.window = window;
	}

	/**
	 * @generated NOT
	 */
	public void dispose() {
		window = null;
	}

	/**
	 * @generated NOT
	 */
	public void selectionChanged(IAction action, ISelection selection) {
	}

	/**
	 * @generated NOT
	 */
	private Shell getShell() {
		return window.getShell();
	}

	/**
	 * @generated NOT
	 */
	public void run(IAction action) {
		// STEP 1: create Eagle script file from Fritzing files
		
		IDiagramWorkbenchPart editor = null;
		URI diagramUri = null;
		
		try {
			// use currently active diagram
			editor = FritzingDiagramEditorUtil.getActiveDiagramPart();
			diagramUri = FritzingDiagramEditorUtil.getActiveDiagramURI();
		} catch (NullPointerException npe) {
			ErrorDialog.openError(getShell(), "PCB Export Error",
					Messages.FritzingPCBExportAction_NoOpenSketchError,
					new Status(Status.ERROR, FritzingDiagramEditorPlugin.ID,
							"No sketch currently opened.", npe));
			return;
		}
		
		// Specify paths to scripts and ULPs needed by Eagle
		// these include fritzing_master.ulp, auto_place.ulp, fritzing_setup-arduino_shield.scr,
		// and fritzing_menu-placement.scr at the moment
		// (done here because the location of the Fritzing Eagle library must be passed
		// to the method that creates the schematic-generation script)
		String scriptsLocation = FritzingDiagramEditorUtil.getFritzingLocation()
									+ "eagle/";
		
		// transform into EAGLE script
		String script = Fritzing2Eagle.createEagleScript(editor.getDiagramGraphicalViewer());

		String fritzing2eagleSCR = diagramUri.trimFileExtension()
				.appendFileExtension("scr").toFileString();
		// Delete existing one so that EAGLE will create new ones
		File eagleScrFile = new File(fritzing2eagleSCR);
		if (eagleScrFile.exists()) {
			eagleScrFile.delete(); 
		}
		try {
			FileOutputStream fos = new FileOutputStream(fritzing2eagleSCR);
			OutputStreamWriter osw = new OutputStreamWriter(fos);
			osw.write(script);
			fos.flush();
			fos.getFD().sync();
			osw.close();
			// NOTE: FileWriter would be nice here, but it doesn't sync immediately
		} catch (IOException ioe) {	
			ErrorDialog.openError(getShell(), "PCB Export Error",
				"Could not create file " + fritzing2eagleSCR + " for writing the EAGLE script.\n"+
				"Please check if Fritzing has write access.",
				new Status(Status.ERROR, FritzingDiagramEditorPlugin.ID,
						"File could not be written.", ioe));
			return;
		} 

		// STEP 2: start Eagle ULP on the created script file

		// EAGLE folder
		Preferences preferences = FritzingDiagramEditorPlugin.getInstance()
				.getPluginPreferences();
		String eagleLocation = preferences
				.getString(EaglePreferencePage.EAGLE_LOCATION) + File.separator;
		
		// EAGLE executable
		String eagleExec = eagleLocation;
		if (Platform.getOS().equals(Platform.OS_WIN32)) {
			eagleExec = eagleExec + "bin\\eagle.exe";
		} else if (Platform.getOS().equals(Platform.OS_MACOSX)) {
			// the command-line executable for OSX is stored in the app bundle 
			// calling the app bundle directly does not support passing in command line statements 
			// from release 5.0 on,�Eagle's executable is called "EAGLE" inside the app bundle
			// we also support the earlier naming convention under the beta of "eagle"
			// TODO - should we just read in the version information from the info.plist file in Contents?  info contains version number, so we could additionally alert users if their eagle install is out-of-date.
			if (new File(eagleExec + "EAGLE.app/Contents/MacOS/EAGLE").exists()) {
				// appears to be a version after 5.0
				eagleExec = eagleExec + "EAGLE.app/Contents/MacOS/EAGLE";
			}
			if (new File(eagleExec + "EAGLE.app/Contents/MacOS/eagle").exists()) {
				// appears to be one of the 5.0 betas
				eagleExec = eagleExec + "EAGLE.app/Contents/MacOS/eagle";
			}				
		} else if (Platform.getOS().equals(Platform.OS_LINUX)) {
			eagleExec = eagleExec + "bin/eagle";
		}
		
		if (! new File(eagleExec).exists()) {
			ErrorDialog.openError(getShell(), "PCB Export Error",
				"Could not find EAGLE installation at " + eagleLocation + ".\n"+
				"Please check if you correctly set the EAGLE location in the preferences.",
				new Status(Status.ERROR, FritzingDiagramEditorPlugin.ID,
						"No EAGLE executable at "+eagleExec, null));
			return;
		}
		if (Platform.getOS().equals(Platform.OS_WIN32)) {
			eagleExec = "\"" + eagleExec + "\"";
		}
		// EAGLE PCB .ulp
		String eagleULP = scriptsLocation + "ulp/fritzing_master.ulp";
		if (! new File(eagleULP).exists()) {
			ErrorDialog.openError(getShell(), "PCB Export Error",
				"Could not find Fritzing ULP at " + eagleULP + ".\n"+
				"Please check if the Fritzing EAGLE files exist.",
				new Status(Status.ERROR, FritzingDiagramEditorPlugin.ID,
						"Fritzing ULP not found.", null));
			return;
		} 
		// EAGLE .brd file
		String eagleBRD = diagramUri.trimFileExtension()
				.appendFileExtension("brd").toFileString();
		// Delete existing one so that EAGLE will create new one
		File eagleBrdFile = new File(eagleBRD);
		if (eagleBrdFile.exists()) {
			eagleBrdFile.delete();
		}
		// EAGLE parameters
		String eagleParams = "RUN " 
				+ "'" + eagleULP + "' "
				+ "'" + fritzing2eagleSCR + "' "	
				+ "'" + scriptsLocation + "' " 
				+ "'" + eagleBRD + "'";
		if (Platform.getOS().equals(Platform.OS_WIN32)) {
			eagleBRD = "\"" + eagleBRD + "\"";
		}
		// Run!
		try {
			ProcessBuilder runEagle = new ProcessBuilder(
					eagleExec, "-C", eagleParams, eagleBRD);
			Process p = runEagle.start();
			//	p.waitFor(); // don't wait for Eagle to quit
		} catch (IOException ioe1) {
			ErrorDialog.openError(getShell(), "PCB Export Error",
					"Could not launch EAGLE PCB export.\n"+
					"Please check that all of the following files exist:\n\n"+
					String.format("%s, %s, %s", eagleExec, eagleULP, fritzing2eagleSCR),
					new Status(Status.ERROR, FritzingDiagramEditorPlugin.ID,
							"EAGLE PCB export failed.", ioe1));
				return;
		} 
	}
}
