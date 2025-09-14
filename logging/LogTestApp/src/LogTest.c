#include <PalmOS.h>
#include "LogTest.h"
#include "../common/LogDB.h"

/* Forward decls */
static Boolean AppHandleEvent(EventType *eventP);
static Boolean MainFormHandleEvent(EventType *eventP);
static void AppEventLoop(void);
static Err AppStart(void);
static void AppStop(void);

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    Err err;

    if (cmd == sysAppLaunchCmdNormalLaunch) {
        err = AppStart();
        if (err) return err;
        AppEventLoop();
        AppStop();
    }
    return errNone;
}

static Err AppStart(void)
{
    Err e;
    e = LogDB_Init("LogTestApp");
    return e;
}

static void AppStop(void)
{
    LogDB_Close();
}

static void OpenMainForm(void)
{
    FormType *frmP;
    frmP = FrmInitForm(LogTestFormID);
    FrmSetActiveForm(frmP);
    FrmSetEventHandler(frmP, MainFormHandleEvent);
}

static void AppEventLoop(void)
{
    EventType event;
    UInt16 err;

    /* Open the main form the usual way */
    FrmGotoForm(LogTestFormID);

    do {
        EvtGetEvent(&event, evtWaitForever);

        if (!SysHandleEvent(&event))
            if (!MenuHandleEvent(0, &event, &err))
                if (!AppHandleEvent(&event))
                    FrmDispatchEvent(&event);

    } while (event.eType != appStopEvent);
}

static Boolean AppHandleEvent(EventType *eventP)
{
    if (eventP->eType == frmLoadEvent) {
        UInt16 formId;
        FormType *frmP;

        formId = eventP->data.frmLoad.formID;
        if (formId == LogTestFormID) {
            frmP = FrmInitForm(formId);
            FrmSetActiveForm(frmP);
            FrmSetEventHandler(frmP, MainFormHandleEvent);
            return true;
        }
    }
    return false;
}

static Boolean MainFormHandleEvent(EventType *eventP)
{
    Boolean handled = false;

    switch (eventP->eType) {
    case ctlSelectEvent:
        if (eventP->data.ctlSelect.controlID == LogTestBtnHelloID) {
            /* Write a log line */
            LogDB_Log("Button Clicked");
            SndPlaySystemSound(sndClick);
            handled = true;
        }
        break;

    case frmOpenEvent:
        FrmDrawForm(FrmGetActiveForm());
        handled = true;
        break;

    default:
        break;
    }
    return handled;
}
