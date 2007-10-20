package org.omnetpp.test.gui.scave;

import junit.framework.Test;
import junit.framework.TestSuite;

public class ScaveTestSuite
    extends TestSuite
{
    public ScaveTestSuite() {
        addTestSuite(OpenFileTest.class);
        addTestSuite(InputsPageTest.class);
        addTestSuite(BrowseDataPageTest.class);
        addTestSuite(DatasetsAndChartsPageTest.class);
    }

    public static Test suite() {
        return new ScaveTestSuite();
    }        
}
