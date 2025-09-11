/* LogViewerApp.c — custom programmatic scrollbar (no Scl* API)
   Palm OS 4 compatible, C89 declarations at block starts. */
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
#define LogScrollBarID     2002 /* used only symbolically for consistency */
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

    /* Custom scrollbar state */
    RectangleType sbBounds;    /* outer bar rect */
    RectangleType sbTrack;     /* inner track area where thumb moves */
    Boolean sbDragging;
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
/* Adjust to your layout: right-side 10px gutter next to a field at (6,50,w=130,h=82) */
static void SB_SetBoundsDefault(void) {
    RectangleType r;
    RectangleType t;

    r.topLeft.x = 138;
    r.topLeft.y = 50;
    r.extent.x  = 10;
    r.extent.y  = 82;
    G.sbBounds = r;

    /* Track is the interior where the thumb moves (with 2px padding) */
    t.topLeft.x = (Coord)(r.topLeft.x + 2);
    t.topLeft.y = (Coord)(r.topLeft.y + 2);
    t.extent.x  = (Coord)(r.extent.x  - 4);
    t.extent.y  = (Coord)(r.extent.y  - 4);
    G.sbTrack = t;
}

/* Utility: clamp within [a, b] (a <= b) */
static Int16 Clamp16(Int16 v, Int16 a, Int16 b) {
    if (v < a) return a;
    if (v > b) return b;
    return v;
}

/* Map field scroll values to a thumb rectangle and draw the bar. */
static void SB_Draw(void) {
    FormType* frm;
    FieldType* fld;
    UInt16 pos;
    UInt16 max;
    UInt16 page;
    RectangleType frameR;
    RectangleType trackR;
    RectangleType thumbR;
    Int16 trackTop;
    Int16 trackBottom;
    Int16 trackH;
    Int16 thumbH;
    Int16 thumbTop;

    frm = FrmGetActiveForm();
    fld = (FieldType*)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogFieldID));

    pos = 0; max = 0; page = 0;
    FldGetScrollValues(fld, &pos, &max, &page);

    /* Draw frame */
    frameR = G.sbBounds;
    WinDrawRectangleFrame(simpleFrame, &frameR);

    /* Track area */
    trackR = G.sbTrack;
    WinEraseRectangle(&trackR, 0);

    /* Compute thumb */
    trackTop = trackR.topLeft.y;
    trackBottom = (Int16)(trackR.topLeft.y + trackR.extent.y - 1);
    trackH = (Int16)(trackBottom - trackTop + 1);

    if (max == 0) {
        /* Everything visible: draw full thumb */
        thumbH = trackH;
        thumbTop = trackTop;
    } else {
        /* Thumb height proportional to page/max, min height 6px */
        thumbH = (Int16)((long)page * trackH / (long)(max + page));
        if (thumbH < 6) thumbH = 6;

        /* Thumb top proportional to pos/max */
        thumbTop = (Int16)(trackTop + ((long)pos * (trackH - thumbH)) / (long)max);
    }

    /* Draw thumb as a filled rectangle */
    thumbR.topLeft.x = trackR.topLeft.x;
    thumbR.topLeft.y = thumbTop;
    thumbR.extent.x  = trackR.extent.x;
    thumbR.extent.y  = (Coord)thumbH;

    WinDrawRectangleFrame(simpleFrame, &thumbR);
}

/* Convert a pen Y coordinate in the track to a target field 'pos' (top line index). */
static UInt16 SB_YToPos(Int16 penY) {
    FormType* frm;
    FieldType* fld;
    UInt16 pos;
    UInt16 max;
    UInt16 page;
    Int16 trackTop;
    Int16 trackH;
    Int16 y;
    UInt16 target;

    frm = FrmGetActiveForm();
    fld = (FieldType*)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogFieldID));

    pos = 0; max = 0; page = 0;
    FldGetScrollValues(fld, &pos, &max, &page);

    trackTop = G.sbTrack.topLeft.y;
    trackH = (Int16)G.sbTrack.extent.y;

    if (max == 0 || trackH <= 0) return 0;

    y = Clamp16((Int16)(penY - trackTop), 0, (Int16)(trackH - 1));
    target = (UInt16)(((long)y * (long)max) / (long)trackH);
    return target;
}

/* Scroll the field so its top line becomes approximately 'targetPos'. */
static void SB_ScrollTo(UInt16 targetPos) {
    FormType* frm;
    FieldType* fld;
    UInt16 pos;
    UInt16 max;
    UInt16 page;

    frm = FrmGetActiveForm();
    fld = (FieldType*)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogFieldID));

    pos = 0; max = 0; page = 0;
    FldGetScrollValues(fld, &pos, &max, &page);

    if (targetPos > pos) {
        FldScrollField(fld, (UInt16)(targetPos - pos), winDown);
    } else if (targetPos < pos) {
        FldScrollField(fld, (UInt16)(pos - targetPos), winUp);
    }
}

/* Handle pen events over the custom scrollbar. Returns true if consumed. */
static Boolean SB_HandlePenEvent(EventType* e) {
    Int16 x;
    Int16 y;
    Boolean inBar;

    x = 0; y = 0;
    if (e->eType == penDownEvent) {
        x = e->screenX; y = e->screenY;
        inBar = RctPtInRectangle(x, y, &G.sbBounds);
        if (inBar) {
            G.sbDragging = true;
            SB_ScrollTo(SB_YToPos(y));
            SB_Draw();
            return true;
        }
    } else if (e->eType == penMoveEvent) {
        if (G.sbDragging) {
            x = e->screenX; y = e->screenY;
            SB_ScrollTo(SB_YToPos(y));
            SB_Draw();
            return true;
        }
    } else if (e->eType == penUpEvent) {
        if (G.sbDragging) {
            G.sbDragging = false;
            return true;
        }
    }

    return false;
}

/* Re-sync and redraw the bar with the field’s current scroll values. */
static void UpdateScrollBar(void) {
    SB_Draw();
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

    /* Let custom scrollbar consume pen events first */
    if (e->eType == penDownEvent || e->eType == penMoveEvent || e->eType == penUpEvent) {
        if (SB_HandlePenEvent(e)) return true;
    }

    switch (e->eType) {
    case frmOpenEvent: {
        frm = FrmGetActiveForm();
        FrmDrawForm(frm);

        SB_SetBoundsDefault();   /* establish bar & track rects */
        BuildAppList();
        InitDateList();
        LoadAndShowLogs();       /* draws field */
        SB_Draw();               /* draw bar after content */

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
    SB_SetBoundsDefault();
    G.sbDragging = false;
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
