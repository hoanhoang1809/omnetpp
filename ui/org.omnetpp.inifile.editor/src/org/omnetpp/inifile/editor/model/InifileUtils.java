/*--------------------------------------------------------------*
  Copyright (C) 2006-2008 OpenSim Ltd.

  This file is distributed WITHOUT ANY WARRANTY. See the file
  'License' for details on this and other legal matters.
*--------------------------------------------------------------*/

package org.omnetpp.inifile.editor.model;

import static org.omnetpp.inifile.editor.model.ConfigRegistry.CFGID_DESCRIPTION;
import static org.omnetpp.inifile.editor.model.ConfigRegistry.CFGID_EXTENDS;
import static org.omnetpp.inifile.editor.model.ConfigRegistry.CFGID_NETWORK;
import static org.omnetpp.inifile.editor.model.ConfigRegistry.CFGID_REPEAT;
import static org.omnetpp.inifile.editor.model.ConfigRegistry.CONFIG_;
import static org.omnetpp.inifile.editor.model.ConfigRegistry.DEFAULT;
import static org.omnetpp.inifile.editor.model.ConfigRegistry.EXTENDS;
import static org.omnetpp.inifile.editor.model.ConfigRegistry.GENERAL;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.apache.commons.lang.ArrayUtils;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IMarker;
import org.eclipse.core.runtime.Assert;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Image;
import org.omnetpp.common.engine.Common;
import org.omnetpp.common.util.StringUtils;
import org.omnetpp.inifile.editor.InifileEditorPlugin;
import org.omnetpp.inifile.editor.model.IInifileDocument.LineInfo;
import org.omnetpp.inifile.editor.model.InifileAnalyzer.KeyType;
import org.omnetpp.inifile.editor.model.ParamResolution.ParamResolutionType;
import org.omnetpp.ned.core.ParamUtil;
import org.omnetpp.ned.model.interfaces.ISubmoduleOrConnection;
import org.omnetpp.ned.model.pojo.SubmoduleElement;

/**
 * Various lookups in inifiles, making use of NED declarations as well.
 * This class cannot be instantiated, all functionality is provided via
 * static methods.
 *
 * @author Andras
 */
public class InifileUtils {
	// for getSectionImage():
	public static final Image ICON_SECTION = InifileEditorPlugin.getCachedImage("icons/full/obj16/section.gif");
	public static final Image ICON_SECTION_NONEXISTENT = InifileEditorPlugin.getCachedImage("icons/full/obj16/section_nonexistent.gif");
	public static final Image ICON_SECTION_SINGLE = InifileEditorPlugin.getCachedImage("icons/full/obj16/section_single.gif");
	public static final Image ICON_SECTION_REPEAT = InifileEditorPlugin.getCachedImage("icons/full/obj16/section_repeat.gif");
	public static final Image ICON_SECTION_ITER = InifileEditorPlugin.getCachedImage("icons/full/obj16/section_iter.gif");
	public static final Image ICON_SECTION_ITERREP = InifileEditorPlugin.getCachedImage("icons/full/obj16/section_iterrep.gif");

	public static final Image ICON_OVR_ERROR = InifileEditorPlugin.getCachedImage("icons/full/ovr16/error.gif");
	public static final Image ICON_OVR_WARNING = InifileEditorPlugin.getCachedImage("icons/full/ovr16/warning.gif");
	public static final Image ICON_OVR_INFO = InifileEditorPlugin.getCachedImage("icons/full/ovr16/info.gif");

	// for getKeyImage()
    public static final Image ICON_ERROR = InifileEditorPlugin.getCachedImage("icons/full/obj16/Error.png");
    public static final Image ICON_INFO = InifileEditorPlugin.getCachedImage("icons/full/obj16/Info.gif");
    public static final Image ICON_PAR_UNASSIGNED = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_unassigned.png");
    public static final Image ICON_PAR_NED = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_ned.png");
    public static final Image ICON_PAR_INIDEFAULT = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_neddefault.png");
    public static final Image ICON_PAR_INIASK = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_iniask.png");
    public static final Image ICON_PAR_INI = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_ini.png");
    public static final Image ICON_PAR_INIOVERRIDE = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_inioverride.png");
    public static final Image ICON_PAR_ININEDDEFAULT = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_inineddefault.png");
    public static final Image ICON_PAR_IMPLICITDEFAULT = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_implicitdefault.png");
    public static final Image ICON_PAR_GROUP = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_group.png");

    public static final Image ICON_SIGNAL = InifileEditorPlugin.getCachedImage("icons/full/obj16/signal.png");
    public static final Image ICON_STATISTIC = InifileEditorPlugin.getCachedImage("icons/full/obj16/statistic.png");

    public static final Image ICON_KEY_EQUALS_DEFAULT = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_equals_default.png");
    public static final Image ICON_KEY_EQUALS_ASK = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_equals_ask.png");

    public static final Image ICON_INIPARMISC = InifileEditorPlugin.getCachedImage("icons/full/obj16/par_inimisc.png");

	/**
	 * Looks up a configuration value in the given section or its fallback sections.
	 * Returns null if not found.
	 */
	public static String lookupConfig(String section, String key, IInifileDocument doc) {
		String[] sectionChain = InifileUtils.resolveSectionChain(doc, section);
		return lookupConfig(sectionChain, key, doc);
	}

	/**
	 * Looks up a configuration value. Returns null if not found.
	 */
	public static String lookupConfig(String[] sectionChain, String key, IInifileDocument doc) {
		for (String section : sectionChain)
			if (doc.containsKey(section, key))
				return doc.getValue(section, key);
		return null;
	}

	/**
	 * Given a parameter's fullPath, returns the key of the matching
	 * inifile entry, or null if the parameter matches nothing. If hasNedDefault
	 * is set, "=default" entries are also considered, otherwise they are ignored
	 */
	public static List<SectionKey> lookupParameter(String paramFullPath, boolean hasNedDefault, String[] sectionChain, IInifileDocument doc) {
		//
		// note: we need to return multiple matches because of keys like "*.node[0].power=...",
	    // "*.node[1..5].power=...", and "net.node[6..].power=..." etc. Scanning stops at
	    // "*.node[*].power=" because that eats all matching params (anything after that cannot match)
	    //
	    List<SectionKey> result = new ArrayList<SectionKey>();
		for (String section : sectionChain) {
			for (String key : doc.getKeys(section)) {
			    ParamUtil.KeyMatcher keyMatcher = ParamUtil.getOrCreateKeyMatcher(key);
				if (keyMatcher.matcher.matches(paramFullPath)) {
				    String value = doc.getValue(section, key);
				    if (hasNedDefault || !value.equals(DEFAULT)) {
				        result.add(new SectionKey(section, key));
				        if (keyMatcher.keyEqualsGeneralizedKey)
				            return result;
				    }
				}
			}
		}
		return result;
	}

	/**
	 * Chops off potential "Config " prefix from a section name.
	 */
	public static String removeSectionNamePrefix(String sectionName) {
		return sectionName.replaceFirst(".+ +", "");
	}

	/**
	 * Resolves the run-time NED type of a "like" submodule, using the parameter
	 * settings in the inifile. Returns null if the lookup is unsuccessful.
	 */
	public static String resolveLikeParam(String moduleFullPath, ISubmoduleOrConnection element, String activeSection, InifileAnalyzer analyzer, IInifileDocument doc) {
		// get like parameter name
		String likeParamName = element.getLikeParam();
		if (!likeParamName.matches("[A-Za-z0-9_]+"))
			return null;  // sorry, we are only prepared to resolve parent module parameters (but not expressions)

		// look up parameter value
		ParamResolution res = analyzer.getParamResolutionForModuleParam(moduleFullPath, likeParamName, activeSection);
		if (res == null)
			return null; // likely no such parameter
		String value = InifileAnalyzer.getParamValue(res, doc);
		if (value == null)
			return null; // likely unassigned
		try {
			value = Common.parseQuotedString(value);
		} catch (RuntimeException e) {
			return null; // something is wrong: value is not a string constant?
		}
		return value;
	}

	/**
	 * Returns the submodule type name. If it uses "like", returns the "like" name.
	 */
	public static String getSubmoduleType(SubmoduleElement submodule) {
		//XXX should try to evaluate "like" expression and use result as type (if possible)
		String submoduleType = submodule.getType();
		if (StringUtils.isEmpty(submoduleType))
			submoduleType = submodule.getLikeType();
		return submoduleType;
	}

	/**
	 * Follows the section "extends" chain back to the [General] section, and
	 * returns the list of section names (including the given section and
	 * [General] as well).
	 *
	 * Errors (such as nonexistent section, cycle in the fallback chain, etc)
	 * are handled in a forgiving way, and a reasonably complete section chain
	 * is returned without throwing an exception -- so this method may be safely
	 * called during any calculation.
	 */
	public static String[] resolveSectionChain(IInifileDocument doc, String section) {
		ArrayList<String> sectionChain = new ArrayList<String>();
		String currentSection = section;
		while (true) {
			sectionChain.add(currentSection);
			currentSection = resolveBaseSection(doc, currentSection);
			if (currentSection==null || sectionChain.contains(currentSection))
				break; // [General] reached, or cycle in the fallback chain
		}
	    return sectionChain.toArray(new String[] {});
	}

	/**
	 * Whether the section chain contains the given section. Useful for detecting
	 * cycles in the "extends" hierarchy.
	 */
	public static boolean sectionChainContains(IInifileDocument doc, String chainStartSection, String section) {
		String[] sectionChain = resolveSectionChain(doc, chainStartSection);
		return ArrayUtils.indexOf(sectionChain, section) >= 0;
	}

	/**
	 * Returns the name of the section the given section extends.
	 * Returns null for the [General] section (it doesn't extend anything),
	 * on error (base section doesn't exist); also, it only returns [General]
	 * if such section really exists.
	 */
	public static String resolveBaseSection(IInifileDocument doc, String section) {
		if (section.equals(GENERAL))
			return null;
        String extendsName = doc.getValue(section, EXTENDS);
        if (extendsName==null)
        	return doc.containsSection(GENERAL) ? GENERAL : null;
        else if (doc.containsSection(CONFIG_+extendsName))
        	return CONFIG_+extendsName;
        else
        	return null;
	}

	/**
	 * Same as resolveBaseSection(), but it returns [General] even if it doesn't exist
	 */
	public static String resolveBaseSectionPretendingGeneralExists(IInifileDocument doc, String section) {
		String baseSection = resolveBaseSection(doc, section);
		return (baseSection==null && !section.equals(GENERAL) && !doc.containsKey(GENERAL, EXTENDS)) ? GENERAL : baseSection;
	}

	/**
	 * Parse a boolean config value. Anything not recognized also counts as false.
	 */
	public static boolean parseAsBool(String value) {
		if (value.equals("yes") || value.equals("true") || value.equals("on") || value.equals("1"))
	    	return true;
	    else if (value.equals("no") || value.equals("false") || value.equals("off") || value.equals("0"))
	    	return false;
	    else
	    	return false; // unrecognized
	}

	/**
	 * Insert a section at the right place in the file.
	 * [General] at top, and other sections ordered alphabetically.
	 */
	public static void addSection(IInifileDocument doc, String newSection) {
		if (doc.containsSection(newSection))
			return;
		String[] sections = doc.getSectionNames();
		if (newSection.equals(GENERAL)) {
			doc.addSection(newSection, sections.length==0 ? null : sections[0]);
			return;
		}
		for (String section : sections) {
			if (section.compareToIgnoreCase(newSection) > 0 && !section.equals(GENERAL)) {
				doc.addSection(newSection, section);
				return;
			}
		}
		doc.addSection(newSection, null);
	}

	/**
	 * Insert a key at the right place in the file. Config keys at top
	 * (extends= first, description= next, network= after, and the
	 * rest follows in alphabetical order), then parameters.
	 * If even the section is not present, it is added first.
	 * The entry MUST NOT exist yet.
	 */
	public static void addEntry(IInifileDocument doc, String section, String newKey, String value, String rawComment) {
		if (!doc.containsSection(section))
			addSection(doc, section);
		Assert.isTrue(!doc.containsKey(section, newKey));
		for (String key : doc.getKeys(section)) {
			if (precedesKey(newKey, key)) {
				doc.addEntry(section, newKey, value, rawComment, key);
				return;
			}
		}
		doc.addEntry(section, newKey, value, rawComment, null);
	}

	/**
	 * Returns true if key1 should precede key2 when new keys are added
	 * in the inifile (preferred order)
	 */
	private static boolean precedesKey(String key1, String key2) {
	    int rank1 = getKeyRank(key1);
        int rank2 = getKeyRank(key2);
        return rank1 == rank2 ? key1.compareToIgnoreCase(key2) < 0 : rank1 < rank2;
	}

	private static int getKeyRank(String key) {
		if (key.equals(CFGID_EXTENDS.getName())) return 1;
		if (key.equals(CFGID_DESCRIPTION.getName())) return 2;
		if (key.equals(CFGID_NETWORK.getName())) return 3;
		KeyType type = InifileAnalyzer.getKeyType(key);
		if (type == KeyType.CONFIG) return 4;
		//FIXME "=default" should come here: if (key.endsWith(dot_APPLY_DEFAULT)) return 7; // (!!!)
        if (type == KeyType.PER_OBJECT_CONFIG) return 5;
        if (type == KeyType.PARAM) return 6;
        return 100; // cannot get here
	}

	public static void addOrSetOrRemoveEntry(IInifileDocument doc, String section, String key, String value) {
		if (value==null) {
			// remove
			if (doc.containsKey(section, key))
				doc.removeKey(section, key);
		}
		else {
			// set or add
			if (!doc.containsKey(section, key))
				addEntry(doc, section, key, value, "");
			else if (!value.equals(doc.getValue(section, key)))
				doc.setValue(section, key, value);
		}
	}

	/**
	 * Renames the given section. Also changes the extends= keys in other sections
	 * that refer to it.
	 */
	public static void renameSection(IInifileDocument doc, String oldSectionName, String newSectionName) {
		doc.renameSection(oldSectionName, newSectionName);

		// change referring extends= keys in other sections
		String oldName = removeSectionNamePrefix(oldSectionName);
		String newName = removeSectionNamePrefix(newSectionName);
		for (String section : doc.getSectionNames())
			if (oldName.equals(doc.getValue(section, EXTENDS)))
				doc.setValue(section, EXTENDS, newName);
	}

	/**
	 * Returns problem markers for an inifile entry or section heading
	 */
	public static IMarker[] getProblemMarkersFor(String section, String key, IInifileDocument doc) {
		LineInfo line = key==null ? doc.getSectionLineDetails(section) : doc.getEntryLineDetails(section, key);
		return line==null ? new IMarker[0] : getProblemMarkersFor(line.getFile(), line.getLineNumber(), line.getLineNumber()+1);
	}

	/**
	 * Returns problem markers for a whole inifile section
	 */
	public static IMarker[] getProblemMarkersForWholeSection(String section, IInifileDocument doc) {
		LineInfo line = doc.getSectionLineDetails(section);
		return line==null ? new IMarker[0] : getProblemMarkersFor(line.getFile(), line.getLineNumber(), line.getLineNumber() + line.getNumLines());
	}

	/**
	 * Returns the problem markers for the given line range in the given file.
	 */
	public static IMarker[] getProblemMarkersFor(IFile file, int startLine, int endLine) {
		try {
			IMarker[] markers = file.findMarkers(IMarker.PROBLEM, true, 0);
			ArrayList<IMarker> result = new ArrayList<IMarker>();
			for (IMarker marker : markers) {
				int lineNumber = marker.getAttribute(IMarker.LINE_NUMBER, -1);
				if (lineNumber >= startLine && lineNumber < endLine)
					result.add(marker);
			}
			return result.toArray(new IMarker[]{});
		}
		catch (CoreException e) {
			InifileEditorPlugin.logError(e);
			return new IMarker[0];
		}
	}

	/**
	 * Returns the maximum severity of the given markers, or -1.
	 * Complements getProblemMarkersFor().
	 */
	public static int getMaximumSeverity(IMarker[] markers) {
		int severity = -1;
		for (IMarker marker : markers)
			if (severity < marker.getAttribute(IMarker.SEVERITY, -1))
				severity = marker.getAttribute(IMarker.SEVERITY, -1);
		return severity;
	}

	/**
	 * Returns an image for the given section, complete with error/warning markers etc.
	 */
	public static Image getSectionImage(String sectionName, InifileAnalyzer analyzer) {
		IInifileDocument doc = analyzer.getDocument();

		// base image
		boolean exists = analyzer.getDocument().containsSection(sectionName);
		boolean containsIteration = exists ? analyzer.containsIteration(sectionName) : false;
		boolean containsRepeat = exists ? lookupConfig(sectionName, CFGID_REPEAT.getName(), doc) != null : false;
		Image sectionImage = !exists ? ICON_SECTION_NONEXISTENT :
			(containsIteration && containsRepeat) ? ICON_SECTION_ITERREP :
				(containsIteration && !containsRepeat) ? ICON_SECTION_ITER :
					(!containsIteration && containsRepeat) ? ICON_SECTION_REPEAT :
						(!containsIteration && !containsRepeat) ? ICON_SECTION_SINGLE :
						ICON_SECTION; // never happens

		// error/warning decoration
		IMarker[] markers = getProblemMarkersForWholeSection(sectionName, doc);
		int maxProblemSeverity = getMaximumSeverity(markers);
		Image overlayImage = maxProblemSeverity == IMarker.SEVERITY_ERROR ? ICON_OVR_ERROR :
			maxProblemSeverity == IMarker.SEVERITY_WARNING ? ICON_OVR_WARNING : null; // note: ignore Infos

		// return decorated image
		String key = "section:"+exists+":"+containsIteration+":"+containsRepeat+":"+maxProblemSeverity;
		return InifileEditorPlugin.getDecoratedImage(sectionImage, overlayImage, SWT.BEGINNING|SWT.BOTTOM, key);
	}

	/**
	 * Returns an image for a given inifile key, suitable for displaying in a table or tree.
	 */
	public static Image getKeyImage(String section, String key, InifileAnalyzer analyzer) {
	    // return an icon based on ParamResolutions
	    ParamResolution[] paramResolutions = analyzer.getParamResolutionsForKey(section, key);
	    if (paramResolutions == null || paramResolutions.length == 0)
	        return ICON_PAR_INI;
        if (paramResolutions.length == 1)
            return suggestImage(paramResolutions[0].type);

        // there are more than one ParamResolutions -- collect their types
	    Set<ParamResolutionType> types = new HashSet<ParamResolutionType>();
	    for (ParamResolution p : paramResolutions)
	        types.add(p.type);
        if (types.size() == 1)
            return suggestImage(paramResolutions[0].type);
        return ICON_INIPARMISC;
	}

	/**
     * Helper function: suggests an icon for a table or tree entry.
     */
    public static Image suggestImage(ParamResolutionType type) {
        switch (type) {
            case UNASSIGNED: return ICON_PAR_UNASSIGNED;
            case NED: return ICON_PAR_NED;
            case INI: return ICON_PAR_INI;
            case INI_DEFAULT: return ICON_PAR_INIDEFAULT;
            case INI_ASK: return ICON_PAR_INIASK;
            case INI_OVERRIDE:  return ICON_PAR_INIOVERRIDE;
            case INI_NEDDEFAULT: return ICON_PAR_ININEDDEFAULT;
            case IMPLICITDEFAULT: return ICON_PAR_IMPLICITDEFAULT;
        }
        return null;
    }

}
