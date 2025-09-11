/* LogTestApp.c */
#include <PalmOS.h>
#include "LogDB.h"

#define appFileCreator 'LtAp'
#define appName        "LogTestApp"

/* Resource IDs */
#define MainFormID     1000
#define HelloButtonID  1001

static Boolean MainFormHandleEvent(EventType* eventP);
static Boolean AppHandleEvent(EventType* eventP);
static void    AppEventLoop(void);
static Err     AppStart(void);
static void    AppStop(void);

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags) {
    Err err;

    (void)cmdPBP;       /* unused */
    (void)launchFlags;  /* unused */

    if (cmd == sysAppLaunchCmdNormalLaunch) {
        err = AppStart();
        if (err) return err;
        FrmGotoForm(MainFormID);
        AppEventLoop();
        AppStop();
    }
    return errNone;
}

static Err AppStart(void) {
    return errNone;
}

static void AppStop(void) {
    FrmCloseAllForms();
}

static void OnHello(void) {
    /* Log the click */
    LogDB_Log(appName, "Button Clicked");
    /* Optional immediate feedback */
    SndPlaySystemSound(sndClick);
}

static Boolean MainFormHandleEvent(EventType* eventP) {
    Boolean handled = false;
    FormType* frm;

    switch (eventP->eType) {
    case frmOpenEvent:
        frm = FrmGetActiveForm();
        FrmDrawForm(frm);
        handled = true;
        break;

    case ctlSelectEvent:
        if (eventP->data.ctlSelect.controlID == HelloButtonID) {
            OnHello();
            handled = true;
        }
        break;

    default:
        break;
    }
    return handled;
}

static Boolean AppHandleEvent(EventType* eventP) {
    if (eventP->eType == frmLoadEvent) {
        UInt16 formID;
        FormType* frm;

        formID = eventP->data.frmLoad.formID;
        frm = FrmInitForm(formID);
        FrmSetActiveForm(frm);

        switch (formID) {
        case MainFormID:
            FrmSetEventHandler(frm, MainFormHandleEvent);
            return true;

        default:
            break;
        }
    }
    return false;
}

static void AppEventLoop(void) {
    UInt16 error;
    EventType event;

    do {
        EvtGetEvent(&event, evtWaitForever);
        if (!SysHandleEvent(&event))
        if (!MenuHandleEvent(0, &event, &error))
        if (!AppHandleEvent(&event))
            FrmDispatchEvent(&event);
    } while (event.eType != appStopEvent);
}