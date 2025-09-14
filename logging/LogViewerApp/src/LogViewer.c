#include <PalmOS.h>
#include "LogViewer.h"
#include "../common/LogDB.h"

/* --- Model for the on-screen text buffer --- */

typedef struct {
    UInt32 seconds;
    Char  *app;   /* owned copies */
    Char  *msg;   /* owned copies */
} Item;

typedef struct {
    Item *items;
    UInt16 count;
} ItemList;

static MemHandle sTextH = NULL;      /* Field text handle */
static Char **sAppChoices = NULL;    /* dynamic app names array for list */
static UInt16 sAppChoiceCount = 0;
static UInt16 sSelectedApp = 0;      /* index in sAppChoices (0 == "All") */
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
    DateTimeType now;
    DateTimeType rec;

    switch (sSelectedTime) {
    case TF_All:
        return true;

    case TF_LastHour:
        if (nowSecs >= recSecs) {
            delta = nowSecs - recSecs;
            return (delta <= 60UL * 60UL);
        }
        return false;

    case TF_Last24h:
        if (nowSecs >= recSecs) {
            delta = nowSecs - recSecs;
            return (delta <= 24UL * 60UL * 60UL);
        }
        return false;

    case TF_Last7d:
        if (nowSecs >= recSecs) {
            delta = nowSecs - recSecs;
            return (delta <= 7UL * 24UL * 60UL * 60UL);
        }
        return false;

    case TF_Today:
        TimSecondsToDateTime(nowSecs, &now);
        TimSecondsToDateTime(recSecs, &rec);
        return (now.year == rec.year && now.month == rec.month && now.day == rec.day);

    default:
        return true;
    }
}

/* --- Sorting newest-first with simple insertion sort --- */

static int CmpItemsDesc(const void *a, const void *b)
{
    const Item *ia = (const Item *)a;
    const Item *ib = (const Item *)b;
    if (ia->seconds < ib->seconds) return 1;
    if (ia->seconds > ib->seconds) return -1;
    return 0;
}

/* --- App choices --- */

static void Viewer_BuildAppChoices(void)
{
    LogDB_Iter it;
    Err e;
    UInt16 i;
    MemHandle h;
    UInt32 secs;
    Char *app;
    Char *msg;

    Viewer_FreeAppChoices();

    /* First pass: collect distinct app names (cap to avoid big allocations). */
    {
#define MAX_APPS 32
        Char *names[MAX_APPS];
        UInt16 counts;

        for (i = 0; i < MAX_APPS; i++) names[i] = NULL;
        counts = 0;

        e = LogDB_IterBegin(&it);
        if (e == errNone) {
            while ((h = LogDB_IterNext(&it, &secs, &app, &msg)) != NULL) {
                Boolean found;
                found = false;
                for (i = 0; i < counts; i++) {
                    if (StrCompare(names[i], app) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found && counts < MAX_APPS) {
                    UInt16 len;
                    Char *copy;
                    len = (UInt16)StrLen(app);
                    copy = (Char *)MemPtrNew(len + 1);
                    if (copy != NULL) {
                        MemMove(copy, app, len + 1);
                        names[counts++] = copy;
                    }
                }
                LogDB_IterUnlock(h);
            }
            LogDB_IterEnd(&it);
        }

        sAppChoices = (Char **)MemPtrNew((counts + 1) * sizeof(Char *));
        if (sAppChoices == NULL) {
            sAppChoiceCount = 0;
            return;
        }
        sAppChoices[0] = "All";
        for (i = 0; i < counts; i++) {
            sAppChoices[i + 1] = names[i];
        }
        sAppChoiceCount = counts + 1;
    }

    /* Hook into the list control */
    {
        FormType *frm;
        ListType *lst;
        ControlType *trg;

        frm = FrmGetActiveForm();
        lst = (ListType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerAppListID));
        trg = (ControlType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerAppTrigID));
        LstSetListChoices(lst, sAppChoices, sAppChoiceCount);
        LstSetSelection(lst, 0);
        CtlSetLabel(trg, sAppChoices[0]);
        sSelectedApp = 0;
    }
}

static void Viewer_FreeAppChoices(void)
{
    UInt16 i;
    if (sAppChoices != NULL) {
        for (i = 1; i < sAppChoiceCount; i++) {
            if (sAppChoices[i] != NULL) {
                MemPtrFree(sAppChoices[i]);
            }
        }
        MemPtrFree(sAppChoices);
        sAppChoices = NULL;
    }
    sAppChoiceCount = 0;
    sSelectedApp = 0;
}

/* --- Render / Refresh --- */

static void Viewer_Refresh(void)
{
    LogDB_Iter it;
    Err e;
    MemHandle h;
    UInt32 nowSecs;
    UInt32 secs;
    Char *app;
    Char *msg;
    Item *arr;
    UInt16 n;
    UInt16 cap;

    Char *outBuf;
    UInt32 outCap;
    UInt32 outLen;

    Char timeBuf[24];
    const Char *appFilter;

    arr = NULL;
    n = 0;
    cap = 0;
    outBuf = NULL;
    outCap = 0;
    outLen = 0;
    appFilter = NULL;

    if (sSelectedApp > 0 && sAppChoices != NULL && sSelectedApp < sAppChoiceCount) {
        appFilter = sAppChoices[sSelectedApp];
    }

    nowSecs = TimGetSeconds();

    /* Collect items (COPY strings while record is locked) */
    e = LogDB_IterBegin(&it);
    if (e == errNone) {
        while ((h = LogDB_IterNext(&it, &secs, &app, &msg)) != NULL) {
            Boolean ok;
            ok = true;

            if (appFilter != NULL) {
                if (StrCompare(appFilter, app) != 0) ok = false;
            }
            if (ok) ok = TimeFilter_Passes(nowSecs, secs);

            if (ok) {
                if (n == cap) {
                    UInt16 ncap;
                    Item *tmp;
                    ncap = (cap == 0) ? 32 : (UInt16)(cap * 2);
                    tmp = (Item *)MemPtrNew(ncap * sizeof(Item));
                    if (tmp == NULL) {
                        LogDB_IterUnlock(h);
                        break;
                    }
                    if (arr != NULL) {
                        MemMove(tmp, arr, n * sizeof(Item));
                        MemPtrFree(arr);
                    }
                    arr = tmp;
                    cap = ncap;
                }
                /* Copy app + msg strings now */
                {
                    UInt16 alen;
                    UInt16 mlen;
                    Char *ac;
                    Char *mc;

                    alen = (UInt16)StrLen(app);
                    mlen = (UInt16)StrLen(msg);
                    ac = (Char *)MemPtrNew(alen + 1);
                    mc = (Char *)MemPtrNew(mlen + 1);
                    if (ac == NULL || mc == NULL) {
                        if (ac != NULL) MemPtrFree(ac);
                        if (mc != NULL) MemPtrFree(mc);
                        LogDB_IterUnlock(h);
                        break;
                    }
                    MemMove(ac, app, alen + 1);
                    MemMove(mc, msg, mlen + 1);

                    arr[n].seconds = secs;
                    arr[n].app = ac;
                    arr[n].msg = mc;
                }
                n++;
            }

            LogDB_IterUnlock(h);
        }
        LogDB_IterEnd(&it);
    }

    /* Sort newest-first (insertion sort) */
    if (arr != NULL && n > 1) {
        UInt16 i;
        for (i = 1; i < n; i++) {
            Item key;
            UInt16 j;

            key = arr[i];
            j = i;
            while (j > 0 && CmpItemsDesc(&arr[j - 1], &key) > 0) {
                arr[j] = arr[j - 1];
                j--;
            }
            arr[j] = key;
        }
    }

    /* Render into one big buffer */
    {
        UInt16 i;
        for (i = 0; i < n; i++) {
            UInt16 need;
            UInt16 appLen;
            UInt16 msgLen;
            Char *dst;

            FormatDateTime(timeBuf, arr[i].seconds);
            appLen = (UInt16)StrLen(arr[i].app);
            msgLen = (UInt16)StrLen(arr[i].msg);
            need = (UInt16)(StrLen(timeBuf) + 3 + appLen + 3 + msgLen + 1);

            if (outLen + need + 1 > outCap) {
                UInt32 ncap;
                Char *tmp;
                ncap = (outCap == 0) ? 2048 : (outCap * 2);
                tmp = (Char *)MemPtrNew(ncap);
                if (tmp == NULL) {
                    break;
                }
                if (outBuf != NULL) {
                    MemMove(tmp, outBuf, outLen);
                    MemPtrFree(outBuf);
                }
                outBuf = tmp;
                outCap = ncap;
            }

            /* Append line: "YYYY-MM-DD hh:mm - App - Message\n" */
            dst = outBuf + outLen;
            StrCopy(dst, timeBuf);
            outLen += (UInt16)StrLen(timeBuf);

            StrCopy(outBuf + outLen, " - ");
            outLen += 3;

            StrCopy(outBuf + outLen, arr[i].app);
            outLen += appLen;

            StrCopy(outBuf + outLen, " - ");
            outLen += 3;

            StrCopy(outBuf + outLen, arr[i].msg);
            outLen += msgLen;

            outBuf[outLen++] = '\n';
            outBuf[outLen] = 0;
        }
    }

    Viewer_SetFieldText((outBuf != NULL) ? outBuf : "");

    /* Cleanup */
    if (arr != NULL) {
        UInt16 i2;
        for (i2 = 0; i2 < n; i2++) {
            if (arr[i2].app != NULL) MemPtrFree(arr[i2].app);
            if (arr[i2].msg != NULL) MemPtrFree(arr[i2].msg);
        }
        MemPtrFree(arr);
    }
    if (outBuf != NULL) MemPtrFree(outBuf);
}

static void Viewer_SetFieldText(const Char *text)
{
    FormType *frm;
    FieldType *fld;
    UInt32 len;
    Char *p;

    frm = FrmGetActiveForm();
    fld = (FieldType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerFldID));
    len = (text != NULL) ? StrLen(text) : 0;

    /* Detach any existing handle from the field BEFORE freeing it */
    if (sTextH != NULL) {
        FldSetTextHandle(fld, NULL);
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
    if (p != NULL) {
        if (len > 0) {
            MemMove(p, text, len);
        }
        p[len] = 0;
        MemHandleUnlock(sTextH);
    }

    FldSetTextHandle(fld, sTextH);
    FldRecalculateField(fld, true);
    FldDrawField(fld);
    Viewer_UpdateScrollBar(true);
}

static void Viewer_UpdateScrollBar(Boolean redraw)
{
    FormType *frm;
    FieldType *fld;
    ScrollBarType *scb;
    UInt16 scrollPos;
    UInt16 textHeight;
    UInt16 fieldHeight;
    UInt16 maxValue;

    frm = FrmGetActiveForm();
    fld = (FieldType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerFldID));
    scb = (ScrollBarType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerScbID));

    scrollPos = 0;
    textHeight = 0;
    fieldHeight = 0;
    maxValue = 0;

    FldGetScrollValues(fld, &scrollPos, &textHeight, &fieldHeight);
    if (textHeight > fieldHeight) {
        maxValue = textHeight - fieldHeight;
    } else {
        maxValue = 0;
    }

    SclSetScrollBar(scb, scrollPos, 0, maxValue, (fieldHeight > 0) ? (fieldHeight - 1) : 0);
    if (redraw) {
        FldDrawField(fld);
    }
}

/* --- Standard app skeleton --- */

static Err AppStart(void)
{
    /* Nothing special needed for viewer at start */
    return errNone;
}

static void AppStop(void)
{
    if (sTextH != NULL) {
        /* Ensure field does not hold it before freeing */
        FormType *frm;
        FieldType *fld;
        frm = FrmGetActiveForm();
        if (frm != NULL) {
            fld = (FieldType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerFldID));
            if (fld != NULL) {
                FldSetTextHandle(fld, NULL);
            }
        }
        MemHandleFree(sTextH);
        sTextH = NULL;
    }
    Viewer_FreeAppChoices();
}

static void AppEventLoop(void)
{
    EventType event;
    UInt16 err;

    /* Properly open: generates frmLoadEvent then frmOpenEvent */
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
        UInt16 formId;
        FormType *frm;

        formId = eventP->data.frmLoad.formID;
        if (formId == LogViewerFormID) {
            frm = FrmInitForm(formId);
            FrmSetActiveForm(frm);
            FrmSetEventHandler(frm, MainFormHandleEvent);
            return true;
        }
    }
    return false;
}

static Boolean MainFormHandleEvent(EventType *eventP)
{
    Boolean handled;
    handled = false;

    switch (eventP->eType) {

    case frmOpenEvent:
    {
        FormType *frm;
        FieldType *fld;

        frm = FrmGetActiveForm();
        FrmDrawForm(frm);

        /* Make sure the big field is non-editable at runtime too (RCP should already set NONEDITABLE) */
        //fld = (FieldType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerFldID));
        //if (fld != NULL) {
        //    FldSetEditable(fld, false);
        //}

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
        FormType *frm;
        FieldType *fld;
        Int16 newValue;
        Int16 value;

        frm = FrmGetActiveForm();
        fld = (FieldType *)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, LogViewerFldID));
        newValue = eventP->data.sclRepeat.newValue;
        value = eventP->data.sclRepeat.value;

        if (newValue > value) {
            FldScrollField(fld, (UInt16)(newValue - value), winDown);
        } else if (newValue < value) {
            FldScrollField(fld, (UInt16)(value - newValue), winUp);
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
