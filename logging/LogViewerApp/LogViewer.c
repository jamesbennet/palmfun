#include <PalmOS.h>
#include "LogViewer.h"
#include "../common/LogDB.h"

/* --- Model for the on-screen text buffer --- */

typedef struct {
    UInt32 seconds;
    Char  *app;   /* temporary pointer during aggregation */
    Char  *msg;   /* temporary pointer during aggregation */
} Item;

typedef struct {
    Item *items;
    UInt16 count;
} ItemList;

static MemHandle sTextH = NULL;  /* Field text handle */
static Char **sAppChoices = NULL; /* dynamic app names array for list */
static UInt16 sAppChoiceCount = 0;
static UInt16 sSelectedApp = 0;   /* index in sAppChoices (0 == "All") */
static UInt16 sSelectedTime = TF_All;

static void Viewer_BuildAppChoices(void);
static void Viewer_FreeAppChoices(void);
static void Viewer_Refresh(void);
static void Viewer_SetFieldText(const Char *text);
static void Viewer_UpdateScrollBar(Boolean redraw);

static Boolean AppHandleEvent(EventType *eventP);
static Boolean MainFormHandleEvent(EventType *eventP);
static void AppEventLoop(void);
static Err AppStart(void);
static void AppStop(void);

/* --- Utility formatting --- */

static void FormatDateTime(Char *dst, UInt32 secs)
{
    DateTimeType dt;
    UInt16 y, mo, d, h, mi;
    TimSecondsToDateTime(secs, &dt);
    y = dt.year; mo = dt.month; d = dt.day;
    h = dt.hour; mi = dt.minute;
    StrPrintF(dst, "%04d-%02d-%02d %02d:%02d", (Int16)y, (Int16)mo, (Int16)d, (Int16)h, (Int16)mi);
}

static Boolean TimeFilter_Passes(UInt32 nowSecs, UInt32 recSecs)
{
    UInt32 delta;
    DateTimeType now, rec;

    switch (sSelectedTime) {
    case TF_All: return true;
    case TF_LastHour:
        if (nowSecs >= recSecs) { delta = nowSecs - recSecs; return (delta <= 60UL*60UL); }
        return false;
    case TF_Last24h:
        if (nowSecs >= recSecs) { delta = nowSecs - recSecs; return (delta <= 24UL*60UL*60UL); }
        return false;
    case TF_Last7d:
        if (nowSecs >= recSecs) { delta = nowSecs - recSecs; return (delta <= 7UL*24UL*60UL*60UL); }
        return false;
    case TF_Today:
        TimSecondsToDateTime(nowSecs, &now);
        TimSecondsToDateTime(recSecs, &rec);
        return (now.year==rec.year && now.month==rec.month && now.day==rec.day);
    default:
        return true;
    }
}

/* --- Collect and render --- */

static int CmpItemsDesc(const void *a, const void *b)
{
    const Item *ia = (const Item *)a;
    const Item *ib = (const Item *)b;
    if (ia->seconds < ib->seconds) return 1;
    if (ia->seconds > ib->seconds) return -1;
    return 0;
}

static void Viewer_BuildAppChoices(void)
{
    LogDB_Iter it;
    Err e;
    UInt16 i;
    MemHandle h;
    UInt32 secs;
    Char *app, *msg;

    Viewer_FreeAppChoices();

    /* First pass: count distinct app names */
    {
        /* We collect into a fixed small array first, then copy to heap array.
           Max unique app names is limited here to avoid heavy memory use. */
        #define MAX_APPS 32
        Char *names[MAX_APPS];
        UInt16 counts = 0;
        for (i=0;i<MAX_APPS;i++) names[i]=NULL;

        e = LogDB_IterBegin(&it);
        if (e == errNone) {
            while ((h = LogDB_IterNext(&it, &secs, &app, &msg)) != NULL) {
                Boolean found = false;
                for (i=0;i<counts;i++) {
                    if (StrCompare(names[i], app) == 0) { found = true; break; }
                }
                if (!found && counts < MAX_APPS) {
                    UInt16 len = (UInt16)StrLen(app);
                    Char *copy = (Char *)MemPtrNew(len+1);
                    if (copy) {
                        MemMove(copy, app, len+1);
                        names[counts++] = copy;
                    }
                }
                LogDB_IterUnlock(h);
            }
            LogDB_IterEnd(&it);
        }

        /* Allocate choices array: +1 for "All" */
        sAppChoices = (Char **)MemPtrNew((counts+1) * sizeof(Char *));
        if (sAppChoices == NULL) {
            /* fallback */
            sAppChoiceCount = 0;
            return;
        }
        sAppChoices[0] = "All";
        for (i=0;i<counts;i++) {
            sAppChoices[i+1] = names[i];
        }
        sAppChoiceCount = counts + 1;
    }

    /* Hook into the list control */
    {
        FormType *frm = FrmGetActiveForm();
        ListType *lst = (ListType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerAppListID));
        LstSetListChoices(lst, sAppChoices, sAppChoiceCount);
        LstSetSelection(lst, 0);
        CtlSetLabel((ControlType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerAppTrigID)), sAppChoices[0]);
        sSelectedApp = 0;
    }
}

static void Viewer_FreeAppChoices(void)
{
    UInt16 i;
    if (sAppChoices != NULL) {
        for (i=1; i<sAppChoiceCount; i++) {
            /* free copies we allocated; index 0 is static "All" */
            MemPtrFree(sAppChoices[i]);
        }
        MemPtrFree(sAppChoices);
        sAppChoices = NULL;
    }
    sAppChoiceCount = 0;
    sSelectedApp = 0;
}

static void Viewer_Refresh(void)
{
    LogDB_Iter it;
    Err e;
    MemHandle h;
    UInt32 nowSecs;
    UInt32 secs;
    Char *app, *msg;
    Item *arr = NULL;
    UInt16 n = 0, cap = 0;

    Char *outBuf = NULL;
    UInt32 outCap = 0, outLen = 0;

    Char timeBuf[24];
    const Char *appFilter = NULL;

    if (sSelectedApp > 0 && sAppChoices != NULL && sSelectedApp < sAppChoiceCount) {
        appFilter = sAppChoices[sSelectedApp];
    }

    nowSecs = TimGetSeconds();

    /* Collect items */
    e = LogDB_IterBegin(&it);
    if (e == errNone) {
        while ((h = LogDB_IterNext(&it, &secs, &app, &msg)) != NULL) {
            Boolean ok = true;
            if (appFilter != NULL) {
                if (StrCompare(appFilter, app) != 0) ok = false;
            }
            if (ok) ok = TimeFilter_Passes(nowSecs, secs);

            if (ok) {
                if (n == cap) {
                    UInt16 ncap = (cap == 0) ? 32 : (UInt16)(cap * 2);
                    Item *tmp = (Item *)MemPtrNew(ncap * sizeof(Item));
                    if (tmp == NULL) { LogDB_IterUnlock(h); break; }
                    if (arr != NULL) {
                        MemMove(tmp, arr, n * sizeof(Item));
                        MemPtrFree(arr);
                    }
                    arr = tmp;
                    cap = ncap;
                }
                arr[n].seconds = secs;
                arr[n].app = app; /* pointers valid until we unlock; but we will copy strings later */
                arr[n].msg = msg;
                n++;
            }

            LogDB_IterUnlock(h);
        }
        LogDB_IterEnd(&it);
    }

    /* Sort newest-first */
    if (arr != NULL && n > 1) {
        /* Simple qsort-like (Palm OS doesn’t have qsort; implement a tiny insertion sort) */
        UInt16 i, j;
        for (i = 1; i < n; i++) {
            Item key = arr[i];
            j = i;
            while (j > 0 && CmpItemsDesc(&arr[j-1], &key) > 0) {
                arr[j] = arr[j-1];
                j--;
            }
            arr[j] = key;
        }
    }

    /* Render into one big buffer */
    {
        UInt16 i;
        for (i=0;i<n;i++) {
            UInt16 need;
            UInt16 appLen = (UInt16)StrLen(arr[i].app);
            UInt16 msgLen = (UInt16)StrLen(arr[i].msg);

            FormatDateTime(timeBuf, arr[i].seconds);
            need = (UInt16)(StrLen(timeBuf) + 3 + appLen + 3 + msgLen + 1); /* " - " twice, + '\n' */
            if (outLen + need + 1 > outCap) {
                UInt32 ncap = (outCap == 0) ? 2048 : (outCap * 2);
                Char *tmp = (Char *)MemPtrNew(ncap);
                if (tmp == NULL) break;
                if (outBuf != NULL) {
                    MemMove(tmp, outBuf, outLen);
                    MemPtrFree(outBuf);
                }
                outBuf = tmp;
                outCap = ncap;
            }
            /* Append line */
            {
                Char *dst = outBuf + outLen;
                StrCopy(dst, timeBuf); outLen += (UInt16)StrLen(timeBuf);
                StrCopy(outBuf + outLen, " - "); outLen += 3;
                StrCopy(outBuf + outLen, arr[i].app); outLen += appLen;
                StrCopy(outBuf + outLen, " - "); outLen += 3;
                StrCopy(outBuf + outLen, arr[i].msg); outLen += msgLen;
                outBuf[outLen++] = '\n';
                outBuf[outLen] = 0;
            }
        }
    }

    Viewer_SetFieldText((outBuf != NULL) ? outBuf : "");

    /* Cleanup */
    if (arr != NULL) MemPtrFree(arr);
    if (outBuf != NULL) MemPtrFree(outBuf);
}

static void Viewer_SetFieldText(const Char *text)
{
    FormType *frm = FrmGetActiveForm();
    FieldType *fld = (FieldType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerFldID));
    UInt32 len = (text != NULL) ? StrLen(text) : 0;
    Char *p;

    if (sTextH != NULL) {
        MemHandleFree(sTextH);
        sTextH = NULL;
    }

    sTextH = MemHandleNew(len + 1);
    if (sTextH == NULL) {
        FldSetTextHandle(fld, NULL);
        FldDrawField(fld);
        Viewer_UpdateScrollBar(true);
        return;
    }

    p = (Char *)MemHandleLock(sTextH);
    MemMove(p, text, len + 1);
    MemHandleUnlock(sTextH);

    FldSetTextHandle(fld, sTextH);
    FldRecalculateField(fld, true);
    FldDrawField(fld);
    Viewer_UpdateScrollBar(true);
}

static void Viewer_UpdateScrollBar(Boolean redraw)
{
    FormType *frm = FrmGetActiveForm();
    FieldType *fld = (FieldType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerFldID));
    ScrollBarType *scb = (ScrollBarType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerScbID));
    UInt16 scrollPos = 0, textHeight = 0, fieldHeight = 0, maxValue = 0;

    FldGetScrollValues(fld, &scrollPos, &textHeight, &fieldHeight);
    if (textHeight > fieldHeight) {
        maxValue = textHeight - fieldHeight;
    } else {
        maxValue = 0;
    }
    SclSetScrollBar(scb, scrollPos, 0, maxValue, fieldHeight - 1);
    if (redraw) FldDrawField(fld);
}

/* --- Standard app skeleton --- */

static Err AppStart(void)
{
    /* Initialize viewer side: no need to init LogDB with an app name */
    return errNone;
}

static void AppStop(void)
{
    if (sTextH != NULL) {
        MemHandleFree(sTextH);
        sTextH = NULL;
    }
    Viewer_FreeAppChoices();
}

static void OpenMainForm(void)
{
    FormType *frm;
    frm = FrmInitForm(LogViewerFormID);
    FrmSetActiveForm(frm);
    FrmSetEventHandler(frm, MainFormHandleEvent);
}

static void AppEventLoop(void)
{
    EventType event;
    UInt16 err;

    /* Open the main form the usual way */
    FrmGotoForm(LogViewerFormID);

    do {
        EvtGetEvent(&event, evtWaitForever);

        if (!SysHandleEvent(&event))
            if (!MenuHandleEvent(0, &event, &err))
                if (!AppHandleEvent(&event))
                    FrmDispatchEvent(&event);

    } while (event.eType != appStopEvent);
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    switch (cmd) {
    case sysAppLaunchCmdNormalLaunch:
        if (AppStart() == errNone) {
            AppEventLoop();
            AppStop();
        }
        break;
    default:
        break;
    }
    return 0;
}

static Boolean AppHandleEvent(EventType *eventP)
{
    if (eventP->eType == frmLoadEvent) {
        UInt16 formId = eventP->data.frmLoad.formID;
        if (formId == LogViewerFormID) {
            FormType *frm = FrmInitForm(formId);
            FrmSetActiveForm(frm);
            FrmSetEventHandler(frm, MainFormHandleEvent);
            return true;
        }
    }
    return false;
}

static Boolean MainFormHandleEvent(EventType *eventP)
{
    Boolean handled = false;

    switch (eventP->eType) {
    case frmOpenEvent:
    {
        FormType *frm = FrmGetActiveForm();
        FrmDrawForm(frm);
        Viewer_BuildAppChoices();
        Viewer_Refresh();
        handled = true;
        break;
    }

    case ctlSelectEvent:
        if (eventP->data.ctlSelect.controlID == LogViewerBtnClearID) {
            /* Clear DB and refresh */
            LogDB_ClearAll();
            Viewer_BuildAppChoices();
            Viewer_Refresh();
            handled = true;
        }
        break;

    case sclRepeatEvent:
    {
        FormType *frm = FrmGetActiveForm();
        FieldType *fld = (FieldType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerFldID));
        Int16 newValue = eventP->data.sclRepeat.newValue;
        Int16 value = eventP->data.sclRepeat.value;

        if (newValue > value) {
            FldScrollField(fld, newValue - value, winDown);
        } else if (newValue < value) {
            FldScrollField(fld, value - newValue, winUp);
        }
        Viewer_UpdateScrollBar(false);
        handled = true;
        break;
    }

    case fldChangedEvent:
        Viewer_UpdateScrollBar(false);
        handled = true;
        break;

    case popSelectEvent:
        if (eventP->data.popSelect.listID == LogViewerAppListID) {
            sSelectedApp = eventP->data.popSelect.selection;
            Viewer_Refresh();
            handled = true;
        } else if (eventP->data.popSelect.listID == LogViewerTimeListID) {
            sSelectedTime = eventP->data.popSelect.selection;
            Viewer_Refresh();
            handled = true;
        }
        break;

    default:
        break;
    }
    return handled;
}
