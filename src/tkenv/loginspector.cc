//==========================================================================
//  LOGINSPECTOR.CC - part of
//
//                     OMNeT++/OMNEST
//            Discrete System Simulation in C++
//
//==========================================================================

/*--------------------------------------------------------------*
  Copyright (C) 1992-2008 Andras Varga
  Copyright (C) 2006-2008 OpenSim Ltd.

  This file is distributed WITHOUT ANY WARRANTY. See the file
  `license' for details on this and other legal matters.
*--------------------------------------------------------------*/

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "loginspector.h"
#include "tkenv.h"
#include "tklib.h"
#include "tkutil.h"
#include "inspectorfactory.h"
#include "stringtokenizer.h"

NAMESPACE_BEGIN

void _dummy_for_loginspector() {}


class LogInspectorFactory : public InspectorFactory
{
  public:
    LogInspectorFactory(const char *name) : InspectorFactory(name) {}

    bool supportsObject(cObject *obj) {return dynamic_cast<cModule *>(obj)!=NULL;}
    int getInspectorType() {return INSP_MODULEOUTPUT;}
    double getQualityAsDefault(cObject *object) {return 0.5;}
    Inspector *createInspector() {return new LogInspector(this);}
};

Register_InspectorFactory(LogInspectorFactory);


LogInspector::LogInspector(InspectorFactory *f) : Inspector(f), mode(MESSAGES)
{
    logBuffer = getTkenv()->getLogBuffer();
    logBuffer->addListener(this);
    componentHistory = getTkenv()->getComponentHistory();
}

LogInspector::~LogInspector()
{
    logBuffer->removeListener(this);
}

void LogInspector::createWindow(const char *window, const char *geometry)
{
    Inspector::createWindow(window, geometry);

    strcpy(textWidget,windowName);
    strcat(textWidget, ".main.text");

    CHK(Tcl_VarEval(interp, "createLogInspector ", windowName, " ", TclQuotedString(geometry).get(), NULL ));

    int success = Tcl_GetCommandInfo(interp, textWidget, &textWidgetCmdInfo);
    ASSERT(success);
}

void LogInspector::useWindow(const char *window)
{
   Inspector::useWindow(window);

    strcpy(textWidget,windowName);
    strcat(textWidget, ".main.text");

    int success = Tcl_GetCommandInfo(interp, textWidget, &textWidgetCmdInfo);
    ASSERT(success);
}

void LogInspector::doSetObject(cObject *obj)
{
    Inspector::doSetObject(obj);

    if (object)
        redisplay();
    else
        CHK(Tcl_VarEval(interp, "LogInspector:clear ", windowName, NULL));
}

void LogInspector::logEntryAdded()
{
    printLastLogLine();
}

void LogInspector::logLineAdded()
{
    printLastLogLine();
}

void LogInspector::messageSendAdded()
{
    printLastMessageLine();
}

void LogInspector::textWidgetCommand(const char *arg1, const char *arg2, const char *arg3, const char *arg4, const char *arg5, const char *arg6)
{
    // Note: args do NOT need to be quoted, because they are directly passed to the text widget's cmd proc as (argc,argv)
    const char *argv[] = {textWidget, arg1, arg2, arg3, arg4, arg5, arg6};
    int argc = !arg1 ? 1 : !arg2 ? 2 : !arg3 ? 3 : !arg4 ? 4 : !arg5 ? 5 : 6;
    CHK((*textWidgetCmdInfo.proc)(textWidgetCmdInfo.clientData, interp, argc, (char **)argv));
}

void LogInspector::textWidgetInsert(const char *text, const char *tags)
{
    if (text)
        textWidgetCommand("insert", "end", text, tags);
}

void LogInspector::textWidgetGotoEnd()
{
    textWidgetCommand("see", "end");
}

void LogInspector::textWidgetBookmark(const char *bookmark)
{
    textWidgetCommand("mark", "set", bookmark, "insert");
    textWidgetCommand("mark", "gravity", bookmark, "left");
}

bool LogInspector::isAncestorModule(int moduleId, int potentialAncestorModuleId)
{
    cModule *component = simulation.getModule(moduleId);  //TODO use cModule and getComponent() in 5.0
    if (component) {
        // more efficient version for live modules
        while (component) {
            if (component->getId() == potentialAncestorModuleId)
                return true;
            component = component->getParentModule();
        }
    }
    else {
        while (moduleId != -1) {
            if (moduleId == potentialAncestorModuleId)
                return true;
            moduleId = componentHistory->getParentModuleId(moduleId);
        }
    }
    return false;
}

void LogInspector::refresh()
{
    Inspector::refresh();
    CHK(Tcl_VarEval(interp, "LogInspector:trimlines ", windowName, NULL));
}

void LogInspector::printLastLogLine()
{
    if (mode != LOG)
        return;

    const LogBuffer::Entry *entry = logBuffer->getEntries().back();
    if (entry->moduleId == 0)
    {
        if (entry->lines.empty())
            textWidgetInsert(entry->banner, "log");
        else {
            textWidgetInsert(entry->lines.back().prefix, "prefix");
            textWidgetInsert(entry->lines.back().line);
        }
    }
    else if (excludedModuleIds.find(entry->moduleId)==excludedModuleIds.end())
    {
        if (entry->lines.empty())
            textWidgetInsert(entry->banner, "event");
        else {
            textWidgetInsert(entry->lines.back().prefix, "prefix");
            textWidgetInsert(entry->lines.back().line);
        }
    }
    textWidgetGotoEnd();
}

void LogInspector::printLastMessageLine()
{
    if (mode != MESSAGES)
        return;

    const LogBuffer::Entry *entry = logBuffer->getEntries().back();
    for (int i=0; i<(int)entry->msgs.size(); i++)
    {
        const LogBuffer::MessageSend& msgsend = entry->msgs[i];
        int hopIndex = findFirstRelevantHop(msgsend); //TODO in degenerate case , there may be more than one relevant hops of the same message (e.g. if one node is an empty compound module wired inside)
        if (hopIndex != -1)
            printMessage(entry, i, hopIndex);
    }
    textWidgetGotoEnd();
}

void LogInspector::redisplay()
{
    CHK(Tcl_VarEval(interp, "LogInspector:clear ", windowName, NULL));

    if (!object)
        return;

    const circular_buffer<LogBuffer::Entry*>& entries = logBuffer->getEntries();

    if (logBuffer->getNumEntriesDiscarded() > 0)
    {
        const LogBuffer::Entry *entry = entries.front();
        std::stringstream os;
        if (entry->eventNumber == 0)
            os << "[Some initialization lines have been discarded from the log]\n";
        else
            os << "[Log starts at event #" << entry->eventNumber << " t=" << SIMTIME_STR(entry->simtime) << ", earlier history has been discarded]\n";
        textWidgetInsert(os.str().c_str(), "warning");
    }

    bool printEventBanners = getTkenv()->opt->printEventBanners;
    cModule *mod = static_cast<cModule *>(object);
    int inspModuleId = mod->getId();
    //simtime_t prevTime = -1;
    for (int k = 0; k < entries.size(); k++)
    {
        const LogBuffer::Entry *entry = entries[k];
        if (mode == LOG)
        {
            if (entry->moduleId == 0)
            {
                textWidgetInsert(entry->banner, "log");
                for (int i=0; i<(int)entry->lines.size(); i++) {
                    textWidgetInsert(entry->lines[i].prefix, "prefix");
                    textWidgetInsert(entry->lines[i].line);
                }
            }
            else
            {
                // if this module is under the inspected module, and not excluded, display the log
                if (isAncestorModule(entry->moduleId, inspModuleId) && excludedModuleIds.find(entry->moduleId)==excludedModuleIds.end())
                {
                    if (printEventBanners)
                        textWidgetInsert(entry->banner, "event");
                    for (int i=0; i<(int)entry->lines.size(); i++) {
                        textWidgetInsert(entry->lines[i].prefix, "prefix");
                        textWidgetInsert(entry->lines[i].line);
                    }
                }
            }
        }

        if (mode == MESSAGES)
        {
            for (int i=0; i<(int)entry->msgs.size(); i++)
            {
                const LogBuffer::MessageSend& msgsend = entry->msgs[i];
                int hopIndex = findFirstRelevantHop(msgsend); //TODO in degenerate case , there may be more than one relevant hops of the same message (e.g. if one node is an empty compound module wired inside)
                if (hopIndex != -1) {
                    printMessage(entry, i, hopIndex);
                }
            }
        }

        // trim to scrollback limit once in a while;  TODO rather: display lines back to front, and quit when scrollback limit is reached
        if ((k % 100) == 0)
            CHK(Tcl_VarEval(interp, "LogInspector:trimlines ", windowName, NULL));
    }
    textWidgetGotoEnd();
}

int LogInspector::findFirstRelevantHop(const LogBuffer::MessageSend& msgsend)
{
    // msg is relevant when it has a send hop where both src and dest are
    // either this module or its direct submodule.
    cModule *mod = static_cast<cModule *>(object);
    int inspectedModuleId = mod->getId();
    int srcModuleId = msgsend.hopModuleIds[0];
    bool isSrcOk = srcModuleId == inspectedModuleId || componentHistory->getParentModuleId(srcModuleId) == inspectedModuleId;
    int n = msgsend.hopModuleIds.size();
    for (int i=0; i<n-1; i++)
    {
        int destModuleId = msgsend.hopModuleIds[i+1];
        bool isDestOk = destModuleId == inspectedModuleId || componentHistory->getParentModuleId(destModuleId) == inspectedModuleId;
        if (isSrcOk && isDestOk)
            return i;
        isSrcOk = isDestOk;
    }
    return -1;
}

#define LL  INT64_PRINTF_FORMAT

void LogInspector::printMessage(const LogBuffer::Entry *entry, int msgIndex, int hopIndex)
{
    char bookmark[256];
    sprintf(bookmark, "msghop-%" LL "d:%d:%d", entry->eventNumber, msgIndex, hopIndex);
    textWidgetBookmark(bookmark);

    std::stringstream os;
    os << entry->eventNumber << "\t";
    textWidgetInsert(os.str().c_str(), "eventnumcol");

    std::stringstream os0;
    os0 << entry->simtime << "\t";    // TODO add propdelays of previous hops
    textWidgetInsert(os0.str().c_str(), "timecol");

    const LogBuffer::MessageSend& msgsend = entry->msgs[msgIndex];
    int srcModuleId = msgsend.hopModuleIds[hopIndex];
    int destModuleId = msgsend.hopModuleIds[hopIndex+1];

    std::stringstream os1;
    int inspectedModuleId = static_cast<cModule *>(object)->getId();
    if (srcModuleId != inspectedModuleId)
        os1 << componentHistory->getComponentFullName(srcModuleId) << " ";
    os1 << "-->";
    if (destModuleId != inspectedModuleId)
        os1 << " " << componentHistory->getComponentFullName(destModuleId);
    os1 << "\t";
    textWidgetInsert(os1.str().c_str(), "srcdestcol");

    cMessage *msg = msgsend.msg;
    cMessagePrinter *printer = chooseMessagePrinter(msg);
    std::stringstream os2;
    if (printer)
        printer->printMessage(os2, msg);
    else
        os2 << "(" << msg->getClassName() << ") " << msg->getFullName() << " [no message printer for this object]";
    os2 << "\n";
    textWidgetInsert(os2.str().c_str(), "msginfocol");
}

cMessagePrinter *LogInspector::chooseMessagePrinter(cMessage *msg)
{
    cRegistrationList *instance = messagePrinters.getInstance();
    cMessagePrinter *bestPrinter = NULL;
    int bestScore = 0;
    for (int i=0; i<(int)instance->size(); i++) {
        cMessagePrinter *printer = (cMessagePrinter *)instance->get(i);
        int score = printer->getScoreFor(msg);
        if (score > bestScore) {
            bestPrinter = printer;
            bestScore = score;
        }
    }
    return bestPrinter;
}

void LogInspector::setMode(Mode m)
{
    if (mode != m)
    {
        mode = m;
        redisplay();
    }
}

int LogInspector::inspectorCommand(Tcl_Interp *interp, int argc, const char **argv)
{
    if (argc<1) {Tcl_SetResult(interp, TCLCONST("wrong number of args"), TCL_STATIC); return TCL_ERROR;}

    if (strcmp(argv[0],"redisplay")==0)
    {
       if (argc!=1) {Tcl_SetResult(interp, TCLCONST("wrong argcount"), TCL_STATIC); return TCL_ERROR;}
       TRY(redisplay());
       return TCL_OK;
    }
    else if (strcmp(argv[0],"getexcludedmoduleids")==0)
    {
       if (argc!=1) {Tcl_SetResult(interp, TCLCONST("wrong argcount"), TCL_STATIC); return TCL_ERROR;}
       Tcl_Obj *listobj = Tcl_NewListObj(0, NULL);
       for (std::set<int>::iterator it=excludedModuleIds.begin(); it!=excludedModuleIds.end(); it++)
           Tcl_ListObjAppendElement(interp, listobj, Tcl_NewIntObj(*it));
       Tcl_SetObjResult(interp, listobj);
       return TCL_OK;
    }
    else if (strcmp(argv[0],"setexcludedmoduleids")==0)
    {
       if (argc!=2) {Tcl_SetResult(interp, TCLCONST("wrong argcount"), TCL_STATIC); return TCL_ERROR;}
       excludedModuleIds.clear();
       StringTokenizer tokenizer(argv[1]);
       while (tokenizer.hasMoreTokens())
           excludedModuleIds.insert(atoi(tokenizer.nextToken()));
       return TCL_OK;
    }
    else if (strcmp(argv[0],"getmode")==0)
    {
       if (argc!=1) {Tcl_SetResult(interp, TCLCONST("wrong argcount"), TCL_STATIC); return TCL_ERROR;}
       Tcl_SetResult(interp, TCLCONST(getMode()==LOG?"log":getMode()==MESSAGES?"messages":"???"), TCL_STATIC);
       return TCL_OK;
    }
    else if (strcmp(argv[0],"setmode")==0)
    {
       if (argc!=2) {Tcl_SetResult(interp, TCLCONST("wrong argcount"), TCL_STATIC); return TCL_ERROR;}
       const char *arg = argv[1];
       if (strcmp(arg, "log")==0)
           setMode(LOG);
       else if (strcmp(arg, "messages")==0)
           setMode(MESSAGES);
       else
           {Tcl_SetResult(interp, TCLCONST("wrong mode"), TCL_STATIC); return TCL_ERROR;}
       return TCL_OK;
    }

    return Inspector::inspectorCommand(interp, argc, argv);
}

NAMESPACE_END

