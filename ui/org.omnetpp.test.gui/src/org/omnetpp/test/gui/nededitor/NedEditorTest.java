package org.omnetpp.test.gui.nededitor;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.swt.SWT;
import org.omnetpp.test.gui.access.Access;
import org.omnetpp.test.gui.access.EditorPartAccess;
import org.omnetpp.test.gui.access.ShellAccess;
import org.omnetpp.test.gui.access.StyledTextAccess;
import org.omnetpp.test.gui.access.ViewPartAccess;
import org.omnetpp.test.gui.access.WorkbenchWindowAccess;
import org.omnetpp.test.gui.access.WorkspaceAccess;
import org.omnetpp.test.gui.core.GUITestCase;
import org.omnetpp.test.gui.util.WorkbenchUtils;

public class NedEditorTest extends GUITestCase
{
	protected String projectName = "test-project";

	protected String fileName = "test.ned";

	protected void prepareForTest() throws CoreException {
		// Test setup: close all editors, delete the file left over from previous runs
		WorkbenchWindowAccess workbenchWindow = Access.getWorkbenchWindowAccess();
		workbenchWindow.assertIsActiveShell();
		workbenchWindow.closeAllEditorPartsWithHotKey();
		WorkbenchUtils.ensureProjectFileDeleted(projectName, fileName);
	}

	public void testCreateNedFile() throws Throwable {
		prepareForTest();

		createNewNEDFileByWizard(projectName, fileName);
		
		WorkbenchWindowAccess workbenchWindow = Access.getWorkbenchWindowAccess();
		workbenchWindow.findEditorPartByTitle(fileName).activatePage("Text");
	}
	
	private void createNewNEDFileByWizard(String parentFolder, String fileName) {
		WorkbenchWindowAccess workbenchWindow = Access.getWorkbenchWindowAccess();
		WorkbenchUtils.choosePerspectiveFromDialog(".*OMN.*"); // so that we have "New|NED file" in the menu
		workbenchWindow.chooseFromMainMenu("File|New.*|Network Description.*");
		ShellAccess shell = Access.findShellByTitle("New NED File");
		shell.findTextAfterLabel("File name.*").clickAndType(fileName);
		shell.findButtonWithLabel("Finish").activateWithMouseClick();
		WorkspaceAccess.assertFileExists(parentFolder + "/" + fileName); // make sure file got created
	}

	//TODO incomplete code...
	public void testInheritanceErrors() throws Throwable {
		// plain inheritance
		checkErrorInSource("simple A {}", null); //OK
		checkErrorInSource("simple A extends Unknown {}", "no such type: Unknown");
		checkErrorInSource("simple A like Unknown {}", "no such type: Unknown");
		checkErrorInSource("simple A {}\nsimple B extends A {}", null); //OK
		checkErrorInSource("simple A {}\nsimple B like A {}", "A is not an interface");
		// cycles
		checkErrorInSource("simple A extends A {}", "cycle");
		checkErrorInSource("simple A extends B {}\nsimple B extends A {}", "cycle");
		checkErrorInSource("simple A extends B {}\nsimple B extends C {}\nsimple C extends A {}", "cycle");
		//TODO: same thing with "module" and "channel" instead of "simple"
	}

	private void checkErrorInSource(String nedSource, String errorText) {
		EditorPartAccess editor = Access.getWorkbenchWindowAccess().getActiveEditorPart();
		editor.activatePage("Text");
		StyledTextAccess styledText = editor.findStyledText();
		styledText.pressKey('A', SWT.CTRL); // "Select all"
		styledText.typeIn(nedSource);
		assertErrorMessageInProblemsView(errorText);
		editor.activatePage("Graphics");
		//TODO: do something in the graphical editor: check error markers are there, etc
	}

	private void assertErrorMessageInProblemsView(String errorText) {
		ViewPartAccess problemsView = Access.getWorkbenchWindowAccess().findViewPartByTitle("Problems", true);
		problemsView.activateWithMouseClick();
		problemsView.findTree().findTreeItemByContent(errorText);
	}

}
