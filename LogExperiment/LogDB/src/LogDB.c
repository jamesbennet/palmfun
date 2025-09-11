/* LogDB.c */
#include "LogDB.h"

static Err prvOpenOrCreateDb(DmOpenRef* dbP) {
    LocalID dbId;
    UInt16 cardNo;
    DmSearchStateType stateInfo;
    Err err;

    cardNo = 0;
    err = DmGetNextDatabaseByTypeCreator(true, &stateInfo, LOGDB_DB_TYPE, LOGDB_DB_CREATOR, true, &cardNo, &dbId);
    if (err == errNone) {
        *dbP = DmOpenDatabase(cardNo, dbId, dmModeReadWrite);
        if (!*dbP) return DmGetLastErr();
        return errNone;
    }

    /* Create DB */
    err = DmCreateDatabase(0, LOGDB_DB_NAME, LOGDB_DB_CREATOR, LOGDB_DB_TYPE, false);
    if (err) return err;

    /* Re-find and open */
    MemSet(&stateInfo, sizeof(stateInfo), 0);
    cardNo = 0;
    err = DmGetNextDatabaseByTypeCreator(true, &stateInfo, LOGDB_DB_TYPE, LOGDB_DB_CREATOR, true, &cardNo, &dbId);
    if (err) return err;
    *dbP = DmOpenDatabase(cardNo, dbId, dmModeReadWrite);
    if (!*dbP) return DmGetLastErr();
    return errNone;
}

typedef struct {
    UInt32 ts;   /* seconds since 1904 */
    /* followed by: appName\0message\0 (variable) */
} LogDBRecordHeader;

Err LogDB_Log(const Char* appName, const Char* message) {
    DmOpenRef db;
    Err err;
    UInt32 ts;
    UInt16 recSize;
    MemHandle h;
    UInt16 index;
    Char* p;
    LogDBRecordHeader hdr;
    UInt16 off;

    if (!appName) appName = "UnknownApp";
    if (!message) message = "";

    db = NULL;
    err = prvOpenOrCreateDb(&db);
    if (err) return err;

    ts = TimGetSeconds();

    recSize = (UInt16)(sizeof(LogDBRecordHeader) + StrLen(appName) + 1 + StrLen(message) + 1);
    h = DmNewHandle(db, recSize);
    if (!h) {
        err = memErrNotEnoughSpace;
        DmCloseDatabase(db);
        return err;
    }

    /* Insert record (unsorted; we'll do logical sort on enumeration) */
    index = dmMaxRecordIndex;
    err = DmAttachRecord(db, &index, h, 0);
    if (err) {
        MemHandleFree(h);
        DmCloseDatabase(db);
        return err;
    }

    /* Write content */
    p = MemHandleLock(h);
    if (!p) {
        DmRemoveRecord(db, index);
        DmCloseDatabase(db);
        return memErrNotEnoughSpace;
    }

    /* Header */
    hdr.ts = ts;
    DmWrite(p, 0, &hdr, sizeof(hdr));

    /* Variable data */
    off = (UInt16)sizeof(hdr);
    DmWrite(p, off, appName, StrLen(appName)+1);
    off = (UInt16)(off + StrLen(appName) + 1);
    DmWrite(p, off, message, StrLen(message)+1);

    MemHandleUnlock(h);
    /* Mark as dirty */
    DmReleaseRecord(db, index, true);

    DmCloseDatabase(db);
    return errNone;
}

static Err prvOpenDbRead(DmOpenRef* dbP) {
    LocalID dbId;
    UInt16 cardNo;
    DmSearchStateType stateInfo;
    Err err;

    cardNo = 0;
    err = DmGetNextDatabaseByTypeCreator(true, &stateInfo, LOGDB_DB_TYPE, LOGDB_DB_CREATOR, true, &cardNo, &dbId);
    if (err) return err; /* not found */
    *dbP = DmOpenDatabase(cardNo, dbId, dmModeReadOnly);
    if (!*dbP) return DmGetLastErr();
    return errNone;
}

Err LogDB_Clear(void) {
    DmOpenRef db;
    Err err;
    UInt16 count;
    UInt16 i;

    db = NULL;
    err = prvOpenOrCreateDb(&db);
    if (err) return err;

    /* Delete all records */
    count = DmNumRecords(db);
    for (i = 0; i < count; i++) {
        /* Always delete index 0 repeatedly as records shift down */
        if (DmRemoveRecord(db, 0) != errNone) break;
    }

    DmCloseDatabase(db);
    return errNone;
}

typedef struct {
    UInt32 ts;
    UInt16 recIndex;
} SortItem;

static Int16 CmpDescByTs(void* a, void* b, Int32 other) {
    const SortItem* pa;
    const SortItem* pb;

    (void)other; /* unused */
    pa = (const SortItem*)a;
    pb = (const SortItem*)b;
    if (pa->ts == pb->ts) return 0;
    return (pa->ts < pb->ts) ? 1 : -1;
}

Err LogDB_Enum(Boolean newestFirst, LogDB_EnumCallback cb, void* ctx) {
    DmOpenRef db;
    Err err;
    UInt16 n;
    MemHandle idxH;
    SortItem* items;
    UInt16 fill;
    UInt16 i;
    UInt16 k;
    UInt16 idx;
    UInt16 recIndex;
    MemHandle h;
    Char* p;
    LogDBRecordHeader* hdr;
    Char* app;
    Char* msg;
    Boolean cont;

    if (!cb) return sysErrParamErr;

    db = NULL;
    err = prvOpenDbRead(&db);
    if (err) return (err == dmErrCantFind) ? errNone : err; /* no DB means nothing to enumerate */

    n = DmNumRecords(db);
    if (n == 0) {
        DmCloseDatabase(db);
        return errNone;
    }

    /* Build sort index */
    idxH = MemHandleNew((UInt32)sizeof(SortItem) * n);
    if (!idxH) {
        DmCloseDatabase(db);
        return memErrNotEnoughSpace;
    }
    items = (SortItem*)MemHandleLock(idxH);

    fill = 0;
    for (i = 0; i < n; i++) {
        h = DmQueryRecord(db, i);
        if (!h) continue;
        p = MemHandleLock(h);
        if (!p) continue;
        hdr = (LogDBRecordHeader*)p;
        items[fill].ts = hdr->ts;
        items[fill].recIndex = i;
        MemHandleUnlock(h);
        fill++;
    }

    /* Sort */
    SysQSort(items, fill, sizeof(SortItem), CmpDescByTs, 0);

    /* Enumerate */
    for (k = 0; k < fill; k++) {
        if (newestFirst) {
            idx = k;
        } else {
            idx = (UInt16)(fill - 1 - k);
        }
        recIndex = items[idx].recIndex;
        h = DmQueryRecord(db, recIndex);
        if (!h) continue;
        p = MemHandleLock(h);
        if (!p) continue;

        hdr = (LogDBRecordHeader*)p;
        app = p + sizeof(LogDBRecordHeader);
        msg = app + StrLen(app) + 1;

        cont = cb(hdr->ts, app, msg, ctx);

        MemHandleUnlock(h);
        if (!cont) break;
    }

    MemHandleUnlock(idxH);
    MemHandleFree(idxH);
    DmCloseDatabase(db);
    return errNone;
}

void LogDB_FormatTimestamp(UInt32 ts, Char* outBuf, UInt16 outLen) {
    DateTimeType dt;

    (void)outLen; /* outLen is unused here; keep signature stable */
    TimSecondsToDateTime(ts, &dt);
    /* YYYY-MM-DD HH:MM */
    StrPrintF(outBuf, "%04d-%02d-%02d %02d:%02d",
              dt.year, dt.month, dt.day, dt.hour, dt.minute);
}