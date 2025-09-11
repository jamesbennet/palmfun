/* LogViewerApp.c — programmatic scrollbar version (Palm OS 4 compatible, C89) */
#include <PalmOS.h>
#include <Form.h>
#include <Field.h>
#include <List.h>
#include <ScrollBar.h>

#include "LogDB.h"

#define appFileCreator 'LvAp'
#define appName        "LogViewerApp"

/* Resource IDs (must match your .rcp) */
#define ViewerFormID       2000
#define LogFieldID         2001
/* there is NO SCROLLBAR control in the .rcp; we draw it programmatically */
#define LogScrollBarID     2002
#define ClearButtonID      2003
#define AppTrigID          2004
#define AppListID          2005
#define DateTrigID         2006
#define DateListID         2007
#define WarningAlert       3000

/* State */
typedef struct {
    Char** appNames;      /* dynamic list, includes "All apps" at index 0 */
    UInt16 appCount;
    UInt16 selectedApp;   /* index into appNames */
    UInt16 selectedDate;  /* 0=All, 1=Last24h, 2=Last7d */
    MemHandle textH;      /* text for the field */

    /* Programmatic scrollbar */
    ScrollBarType sb;
    RectangleType sbBounds;
    Boolean sbInited;
    UInt16 sbLastValue;
} ViewerState;

static ViewerState G;

/* Forward decls */
static Boolean ViewerFormHandleEvent(EventType* e);
static Boolean AppHandleEvent(EventType* e);
static void    AppEventLoop(void);
static Err     AppStart(void);
static void    AppStop(void);

/* ------- Utilities & data collection ------- */

static void FreeAppNames(void) {
    UInt16 i;

    if (G.appNames) {
        for (i = 0; i < G.appCount; i++) {
            if (G.appNames[i]) {
                MemPtrFree(G.appNames[i]);
            }
        }
        MemPtrFree(G.appNames);
    }
    G.appNames = NULL;
    G.appCount = 0;
}

static Boolean EnumCollectAppsCB(UInt32 ts, const Char* app, const Char* msg, void* ctx) {
    UInt16 i;
    Char** newArr;
    Char* dup;

    (void)ts;
    (void)msg;
    (void)ctx;

    /* Build distinct list: check existence */
    for (i = 0; i < G.appCount; i++) {
        if (StrCompare(G.appNames[i], app) == 0) return true;
    }

    /* Add new */
    newArr = (Char**)MemPtrNew(sizeof(Char*) * (G.appCount + 1));
    if (!newArr) return false;
    for (i = 0; i < G.appCount; i++) newArr[i] = G.appNames[i];

    dup = (Char*)MemPtrNew(StrLen(app) + 1);
    if (!dup) {
        MemPtrFree(newArr);
        return false;
    }
    StrCopy(dup, app);
    newArr[G.appCount] = dup;
    if (G.appNames) MemPtrFree(G.appNames);
    G.appNames = newArr;
    G.appCount++;
    return true;
}

static void BuildAppList(void) {
    Char** arr;
    ListType* lst;
    ControlType* trig;
    UInt16 i;

    FreeAppNames();

    /* Collect distinct apps */
    LogDB_Enum(true, EnumCollectAppsCB, NULL);

    /* Add "All apps" at front */
    arr = (Char**)MemPtrNew(sizeof(Char*) * (G.appCount + 1));
    if (!arr) return;

    /* index 0 */
    arr[0] = (Char*)MemPtrNew(9);
    if (!arr[0]) {
        MemPtrFree(arr);
        return;
    }
    StrCopy(arr[0], "All apps");
    for (i = 0; i < G.appCount; i++) arr[i + 1] = G.appNames[i];

    if (G.appNames) MemPtrFree(G.appNames);
    G.appNames = arr;
    G.appCount = (UInt16)(G.appCount + 1);

    /* Default selection */
    if (G.selectedApp >= G.appCount) G.selectedApp = 0;

    /* Populate UI list */
    lst = (ListType*)FrmGetObjectPtr(FrmGetActiveForm(),
                                     FrmGetObjectIndex(FrmGetActiveForm(), AppListID));
    /* LstSetListChoices expects Char** */
    LstSetListChoices(lst, (Char**)G.appNames, (Int16)G.appCount);
    LstSetSelection(lst, (Int16)G.selectedApp);

    /* Update trigger label */
    trig = (ControlType*)FrmGetObjectPtr(FrmGetActiveForm(),
                                         FrmGetObjectIndex(FrmGetActiveForm(), AppTrigID));
    CtlSetLabel(trig, G.appNames[G.selectedApp]);
}

static UInt32 NowSeconds(void) {
    return TimGetSeconds();
}

static Boolean PassesDateFilter(UInt32 ts) {
    UInt32 now;

    if (G.selectedDate == 0) return true;
    now = NowSeconds();
    if (G.selectedDate == 1) { /* Last 24h */
        return ts >= now - 24UL * 60UL * 60UL;
    } else if (G.selectedDate == 2) { /* Last 7d */
        return ts >= now - 7UL * 24UL * 60UL * 60UL;
    }
    return true;
}

typedef struct {
    MemHandle bufH;
    UInt16 len;
} BuildBuf;

static void BufAppend(BuildBuf* bb, const Char* s) {
    UInt16 add;
    UInt16 need;
    Char* p;
    UInt16 cap;

    add = (UInt16)StrLen(s);
    /* ensure capacity */
    need = (UInt16)(bb->len + add + 1);
    if (!bb->bufH) {
        bb->bufH = MemHandleNew((UInt32)need + 256);
        if (!bb->bufH) return;
        p = MemHandleLock(bb->bufH);
        p[0] = 0;
        MemHandleUnlock(bb->bufH);
    }
    p = MemHandleLock(bb->bufH);
    cap = (UInt16)MemHandleSize(bb->bufH);
    if (need >= cap) {
        /* grow */
        MemHandleUnlock(bb->bufH);
        if (MemHandleResize(bb->bufH, (UInt32)need + 256) != 0) return;
        p = MemHandleLock(bb->bufH);
    }
    /* append */
    StrCopy(p + bb->len, s);
    bb->len = (UInt16)(bb->len + add);
    MemHandleUnlock(bb->bufH);
}

static Boolean EnumBuildTextCB(UInt32 ts, const Char* app, const Char* msg, void* ctx) {
    BuildBuf* bb;
    const Char* selectedApp;
    Char line[256];
    Char tsBuf[32];

    bb = (BuildBuf*)ctx;

    /* Filter app */
    if (G.selectedApp != 0) {
        selectedApp = G.appNames[G.selectedApp];
        if (StrCompare(app, selectedApp) != 0) return true;
    }
    /* Filter date */
    if (!PassesDateFilter(ts)) return true;

    LogDB_FormatTimestamp(ts, tsBuf, sizeof(tsBuf));
    StrPrintF(line, "%s - %s - %s\n", tsBuf, app, msg);
    BufAppend(bb, line);
    return true; /* continue */
}

/* ------- Programmatic scrollbar helpers ------- */

/* Adjust this rectangle to match where you want the scrollbar drawn in your form.
   These coordinates match earlier examples: field at (6,50,130,82) so bar at x=138. */
static void SB_SetBoundsDefault(void) {
    RectangleType r;

    r.topLeft.x = 138;
    r.topLeft.y = 50;
    r.extent.x  = 10;
    r.extent.y  = 82;
    G.sbBounds = r;
}

static void SB_Init(void) {
    UInt16 value;
    UInt16 min;
    UInt16 max;
    UInt16 pageSize;

    if (G.sbInited) return;

    SB_SetBoundsDefault();
    /* Initialize scrollbar structure and draw it */
    SclCreateScrollBar(&G.sb, LogScrollBarID, &G.sbBounds, 0, 0, 0, 1);
    SclDrawScrollBar(&G.sb);

    /* Track last value to compute delta when user drags */
    value = 0; min = 0; max = 0; pageSize = 1;
    SclGetScrollBar(&G.sb, &value, &min, &max, &pageSize);
    G.sbLastValue = value;

    G.sbInited = true;
}

/* Recompute scrollbar range/position from the field content and redraw it. */
static void UpdateScrollBar(void) {
    FormType* frm;
    FieldType* fld;
    UInt16 pos;
    UInt16 max;
    UInt16 pageSize;

    if (!G.sbInited) return;

    frm = FrmGetActiveForm();
    fld = (FieldType*)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogFieldID));

    max = 0;
    pos = 0;
    pageSize = 0;
    FldGetScrollValues(fld, &pos, &max, &pageSize);

    /* Ensure pageSize is at least 1 to keep the thumb visible */
    if (pageSize == 0) pageSize = 1;

    SclSetScrollBar(&G.sb, pos, 0, max, pageSize);
    SclDrawScrollBar(&G.sb);
    G.sbLastValue = pos;
}

/* When user manipulates the scrollbar, scroll the field accordingly. */
static Boolean SB_HandleEvent(EventType* e) {
    FormType* frm;
    FieldType* fld;
    UInt16 value;
    UInt16 min;
    UInt16 max;
    UInt16 pageSize;
    UInt16 curPos;

    /* Let the scrollbar consume pen events etc. If it returns true,
       its internal value may have changed. */
    if (!SclHandleEvent(&G.sb, e)) return false;

    frm = FrmGetActiveForm();
    fld = (FieldType*)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogFieldID));

    /* Current field pos */
    curPos = 0;
    max = 0;
    pageSize = 0;
    FldGetScrollValues(fld, &curPos, &max, &pageSize);

    /* New scrollbar value */
    value = 0; min = 0; max = 0; pageSize = 1;
    SclGetScrollBar(&G.sb, &value, &min, &max, &pageSize);

    if (value > curPos) {
        FldScrollField(fld, (UInt16)(value - curPos), winDown);
    } else if (value < curPos) {
        FldScrollField(fld, (UInt16)(curPos - value), winUp);
    }

    UpdateScrollBar();
    return true;
}

/* ------- UI composition ------- */

static void LoadAndShowLogs(void) {
    BuildBuf bb;
    FormType* frm;
    FieldType* fld;
    Char* p;

    /* Build text with filters applied (newest to oldest) */
    if (G.textH) {
        MemHandleFree(G.textH);
        G.textH = NULL;
    }

    bb.bufH = NULL;
    bb.len = 0;
    LogDB_Enum(true, EnumBuildTextCB, &bb);

    if (!bb.bufH) {
        bb.bufH = MemHandleNew(1);
        if (bb.bufH) {
            p = MemHandleLock(bb.bufH);
            p[0] = 0;
            MemHandleUnlock(bb.bufH);
        }
    }

    G.textH = bb.bufH;

    /* Set field text */
    frm = FrmGetActiveForm();
    fld = (FieldType*)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogFieldID));
    FldSetTextHandle(fld, G.textH);
    FldDrawField(fld);

    UpdateScrollBar();
}

static void InitDateList(void) {
    static Char* kDates[] = { "All time", "Last 24h", "Last 7 days" };
    ListType* lst;
    ControlType* trig;

    lst = (ListType*)FrmGetObjectPtr(FrmGetActiveForm(),
                                     FrmGetObjectIndex(FrmGetActiveForm(), DateListID));
    LstSetListChoices(lst, kDates, 3);
    if (G.selectedDate > 2) G.selectedDate = 0;
    LstSetSelection(lst, (Int16)G.selectedDate);

    trig = (ControlType*)FrmGetObjectPtr(FrmGetActiveForm(),
                                         FrmGetObjectIndex(FrmGetActiveForm(), DateTrigID));
    CtlSetLabel(trig, kDates[G.selectedDate]);
}

static void ScrollLines(Int16 delta) {
    FormType* frm;
    FieldType* fld;

    frm = FrmGetActiveForm();
    fld = (FieldType*)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogFieldID));
    if (delta < 0) {
        FldScrollField(fld, (UInt16)(-delta), winUp);
    } else if (delta > 0) {
        FldScrollField(fld, (UInt16)(delta), winDown);
    }
    UpdateScrollBar();
}

/* ------- Event handling ------- */

static Boolean ViewerFormHandleEvent(EventType* e) {
    Boolean handled;
    FormType* frm;

    handled = false;

    /* First, let our programmatic scrollbar try to handle the event */
    if (G.sbInited) {
        if (SB_HandleEvent(e)) {
            return true;
        }
    }

    switch (e->eType) {
    case frmOpenEvent: {
        frm = FrmGetActiveForm();
        FrmDrawForm(frm);

        /* Build UI */
        BuildAppList();
        InitDateList();
        LoadAndShowLogs();

        /* Init programmatic scrollbar LAST (bounds rely on drawn form) */
        SB_Init();
        UpdateScrollBar();

        handled = true;
        break;
    }

    case ctlSelectEvent: {
        if (e->data.ctlSelect.controlID == ClearButtonID) {
            if (FrmCustomAlert(WarningAlert, "Clear all logs?", "", "") == 0) {
                LogDB_Clear();
                BuildAppList();
                LoadAndShowLogs();
            }
            handled = true;
        }
        break;
    }

    case keyDownEvent: {
        if (TxtCharIsHardKey(e->data.keyDown.modifiers, e->data.keyDown.chr)) {
            /* fallthrough to system */
        } else {
            if (e->data.keyDown.chr == pageUpChr) {
                ScrollLines(-(Int16)FldGetVisibleLines((FieldType*)FrmGetObjectPtr(
                    FrmGetActiveForm(), FrmGetObjectIndex(FrmGetActiveForm(), LogFieldID))));
                handled = true;
            } else if (e->data.keyDown.chr == pageDownChr) {
                ScrollLines((Int16)FldGetVisibleLines((FieldType*)FrmGetObjectPtr(
                    FrmGetActiveForm(), FrmGetObjectIndex(FrmGetActiveForm(), LogFieldID))));
                handled = true;
            } else if (e->data.keyDown.chr == prevFieldChr) {
                ScrollLines(-1);
                handled = true;
            } else if (e->data.keyDown.chr == nextFieldChr) {
                ScrollLines(1);
                handled = true;
            }
        }
        break;
    }

    case popSelectEvent: {
        if (e->data.popSelect.listID == AppListID) {
            G.selectedApp = (UInt16)e->data.popSelect.selection;
            LoadAndShowLogs();
            handled = true;
        } else if (e->data.popSelect.listID == DateListID) {
            G.selectedDate = (UInt16)e->data.popSelect.selection; /* 0..2 */
            LoadAndShowLogs();
            handled = true;
        }
        break;
    }

    case frmCloseEvent: {
        /* Free text handle detached from field automatically on close */
        if (G.textH) {
            MemHandleFree(G.textH);
            G.textH = NULL;
        }
        FreeAppNames();
        handled = false; /* allow default close */
        break;
    }

    default:
        break;
    }

    return handled;
}

/* ------- App plumbing ------- */

static Boolean AppHandleEvent(EventType* e) {
    if (e->eType == frmLoadEvent) {
        UInt16 formID;
        FormType* frm;

        formID = e->data.frmLoad.formID;
        frm = FrmInitForm(formID);
        FrmSetActiveForm(frm);
        switch (formID) {
        case ViewerFormID:
            FrmSetEventHandler(frm, ViewerFormHandleEvent);
            return true;
        default:
            break;
        }
    }
    return false;
}

static Err AppStart(void) {
    MemSet(&G, sizeof(G), 0);
    G.sbInited = false;
    G.sbLastValue = 0;
    SB_SetBoundsDefault();
    return errNone;
}

static void AppStop(void) {
    FrmCloseAllForms();
}

static void AppEventLoop(void) {
    UInt16 error;
    EventType e;

    do {
        EvtGetEvent(&e, evtWaitForever);
        if (!SysHandleEvent(&e))
        if (!MenuHandleEvent(0, &e, &error))
        if (!AppHandleEvent(&e))
            FrmDispatchEvent(&e);
    } while (e.eType != appStopEvent);
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags) {
    Err err;

    (void)cmdPBP;
    (void)launchFlags;

    if (cmd == sysAppLaunchCmdNormalLaunch) {
        err = AppStart();
        if (err) return err;
        FrmGotoForm(ViewerFormID);
        AppEventLoop();
        AppStop();
    }
    return errNone;
}
