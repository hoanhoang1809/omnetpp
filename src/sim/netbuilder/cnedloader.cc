//==========================================================================
// CNEDLOADER.CC -
//
//                     OMNeT++/OMNEST
//            Discrete System Simulation in C++
//
//==========================================================================

/*--------------------------------------------------------------*
  Copyright (C) 2002-2005 Andras Varga

  This file is distributed WITHOUT ANY WARRANTY. See the file
  `license' for details on this and other legal matters.
*--------------------------------------------------------------*/

#include "cnedloader.h"
#include "nedelements.h"
#include "nederror.h"
#include "nedparser.h"
#include "nedxmlparser.h"
#include "neddtdvalidator.h"
#include "nedbasicvalidator.h"
#include "nedsemanticvalidator.h"

#include "cproperty.h"
#include "cproperties.h"
#include "ccomponenttype.h"
#include "cexception.h"
#include "cenvir.h"
#include "cexpressionbuilder.h"
#include "cpar.h"
#include "clongpar.h"
#include "cdoublepar.h"
#include "globals.h"
#include "cdynamicmoduletype.h"
#include "cdynamicchanneltype.h"
#include "cdisplaystring.h"

cNEDLoader *cNEDLoader::instance_;

cNEDLoader *cNEDLoader::instance()
{
    if (!instance_)
        instance_ = new cNEDLoader();
    return instance_;
}

void cNEDLoader::clear()
{
    delete instance_;
    instance_ = NULL;
}

cNEDLoader::cNEDLoader()
{
    registerBuiltinDeclarations();
}

void cNEDLoader::registerBuiltinDeclarations()
{
    // NED code to define built-in types
    const char *nedcode =
        "channel cBasicChannel\n"
        "{\n"
        "    bool disabled = false;\n"
        "    double delay = 0 @unit(s);\n"
        "    double error = 0;\n"
        "    double datarate = 0 @unit(bps);\n"
        "}\n"

        "channel cIdealChannel\n"
        "{\n"
        "}\n"

        "interface IBidirectionalChannel\n"
        "{\n"
        "    gates:\n"
        "        inout a;\n"
        "        inout b;\n"
        "}\n"

        "interface IUnidirectionalChannel\n"
        "{\n"
        "    gates:\n"
        "        input i;\n"
        "        output o;\n"
        "}\n"
    ;

    NEDErrorStore errors;
    NEDParser parser(&errors);
    NEDElement *tree = parser.parseNEDText(nedcode);
    if (errors.containsError())
    {
        delete tree;
        throw cRuntimeError("error during parsing of internal NED declarations");
    }
    //FIXME check errors; run validation perhaps, etc!

    try
    {
        addFile("[builtin-declarations]", tree);
    }
    catch (NEDException& e)
    {
        throw cRuntimeError("NED error: %s", e.what()); // FIXME or something
    }
}

void cNEDLoader::addComponent(const char *qname, NEDElement *node)
{
    if (!areDependenciesResolved(node))
    {
        // we'll process it later
        pendingList.push_back(node);
        return;
    }

    // Note: base class (nedxml's NEDResourceCache) has already checked for duplicates, no need here
    cNEDDeclaration *decl = buildNEDDeclaration(qname, node);
    components[qname] = decl;

    //printf("DBG: registered %s\n",name);

    // if module or channel, register corresponding object which can be used to instantiate it
    cComponentType *type = NULL;
    if (node->getTagCode()==NED_SIMPLE_MODULE || node->getTagCode()==NED_COMPOUND_MODULE)
        type = new cDynamicModuleType(qname);
    else if (node->getTagCode()==NED_CHANNEL)
        type = new cDynamicChannelType(qname);
    if (type)
        componentTypes.instance()->add(type);
}

cNEDDeclaration *cNEDLoader::lookup2(const char *qname) const
{
    return dynamic_cast<cNEDDeclaration *>(NEDResourceCache::lookup(qname));
}

cNEDDeclaration *cNEDLoader::getDecl(const char *qname) const
{
    cNEDDeclaration *decl = cNEDLoader::instance()->lookup2(qname);
    if (!decl)
        throw cRuntimeError("NED declaration '%s' not found", qname);
    return decl;
}

NEDElement *cNEDLoader::parseAndValidateNedFile(const char *fname, bool isXML)
{
    // load file
    NEDElement *tree = 0;
    NEDErrorStore errors;
    errors.setPrintToStderr(true); //XXX
    if (isXML)
    {
        tree = parseXML(fname, &errors);
    }
    else
    {
        NEDParser parser(&errors);
        parser.setParseExpressions(true);
        parser.setStoreSource(false);
        tree = parser.parseNEDFile(fname);
    }
    if (errors.containsError())
    {
        delete tree;
        throw cRuntimeError("errors while loading or parsing file `%s'", fname);
    }

    // DTD validation and additional basic validation
    NEDDTDValidator dtdvalidator(&errors);
    dtdvalidator.validate(tree);
    if (errors.containsError())
    {
        delete tree;
        throw cRuntimeError("errors during DTD validation of file `%s'", fname);
    }

    NEDBasicValidator basicvalidator(true, &errors);
    basicvalidator.validate(tree);
    if (errors.containsError())
    {
        delete tree;
        throw cRuntimeError("errors during validation of file `%s'", fname);
    }
    return tree;
}

void cNEDLoader::loadNedFile(const char *nedfname, bool isXML)
{
    // parse file
    NEDElement *tree = parseAndValidateNedFile(nedfname, isXML);

    try
    {
        addFile(nedfname, tree);
    }
    catch (NEDException& e)
    {
        throw cRuntimeError("NED error: %s", e.what()); // FIXME or something
    }
}


bool cNEDLoader::areDependenciesResolved(NEDElement *node)
{
    for (NEDElement *child=node->getFirstChild(); child; child=child->getNextSibling())
    {
        if (child->getTagCode()!=NED_EXTENDS && child->getTagCode()!=NED_INTERFACE_NAME)
            continue;

        const char *name = child->getAttribute("name");
        cNEDDeclaration *decl = lookup2(name);
        if (!decl)
            return false;
    }
    return true;
}

void cNEDLoader::tryResolvePendingDeclarations()
{
    bool again = true;
    while (again)
    {
        again = false;
        for (int i=0; i<(int)pendingList.size(); i++)
        {
            NEDElement *node = pendingList[i];
            if (areDependenciesResolved(node))
            {
                addComponent(node->getAttribute("name"), node); //FIXME we have to use QUALIFIED name here!!!!
                pendingList.erase(pendingList.begin() + i--);
                again = true;
            }
        }
    }
}

cNEDDeclaration *cNEDLoader::buildNEDDeclaration(const char *qname, NEDElement *node)
{
    return new cNEDDeclaration(qname, node);
}

void cNEDLoader::done()
{
    // we've loaded all NED files now, try resolving those which had missing dependencies
    tryResolvePendingDeclarations();

    for (int i=0; i<(int)pendingList.size(); i++)
    {
        NEDElement *tree = pendingList[i];
        ev.printfmsg("WARNING: Type `%s' at %s could not be fully resolved, dropped (base type or interface missing)",
                     tree->getAttribute("name"), tree->getSourceLocation()); // FIXME create an ev.warning() or something...
        delete tree;
    }
}

