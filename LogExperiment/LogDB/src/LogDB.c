// LogDB.c
#include "LogDB.h"

static Err prvOpenOrCreateDb(DmOpenRef* dbP) {
    LocalID dbId;
    UInt16 cardNo = 0;
    DmSearchStateType stateInfo;
    Err err = DmGetNextDatabaseByTypeCreator(true, &stateInfo, LOGDB_DB_TYPE, LOGDB_DB_CREATOR, true, &cardNo, &dbId);
    if (err == errNone) {
        *dbP = DmOpenDatabase(cardNo, dbId, dmModeReadWrite);
        if (!*dbP) return DmGetLastErr();
        return errNone;
    }

    // Create DB
    err = DmCreateDatabase(0, LOGDB_DB_NAME, LOGDB_DB_CREATOR, LOGDB_DB_TYPE, false);
    if (err) return err;

    // Re-find and open
    MemSet(&stateInfo, sizeof(stateInfo), 0);
    cardNo = 0;
    err = DmGetNextDatabaseByTypeCreator(true, &stateInfo, LOGDB_DB_TYPE, LOGDB_DB_CREATOR, true, &cardNo, &dbId);
    if (err) return err;
    *dbP = DmOpenDatabase(cardNo, dbId, dmModeReadWrite);
    if (!*dbP) return DmGetLastErr();
    return errNone;
}

typedef struct {
    UInt32 ts;   // seconds since 1904
    // followed by: appName\0message\0 (variable)
} LogDBRecordHeader;

Err LogDB_Log(const Char* appName, const Char* message) {
    if (!appName) appName = "UnknownApp";
    if (!message) message = "";

    DmOpenRef db = NULL;
    Err err = prvOpenOrCreateDb(&db);
    if (err) return err;

    UInt32 ts = TimGetSeconds();

    UInt16 recSize = sizeof(LogDBRecordHeader) + StrLen(appName) + 1 + StrLen(message) + 1;
    MemHandle h = DmNewHandle(db, recSize);
    if (!h) {
        err = memErrNotEnoughSpace;
        DmCloseDatabase(db);
        return err;
    }

    // Insert record (unsorted; we'll do logical sort on enumeration)
    UInt16 index = dmMaxRecordIndex;
    err = DmAttachRecord(db, &index, h, 0);
    if (err) {
        MemHandleFree(h);
        DmCloseDatabase(db);
        return err;
    }

    // Write content
    Char* p = MemHandleLock(h);
    if (!p) {
        DmRemoveRecord(db, index);
        DmCloseDatabase(db);
        return memErrNotEnoughSpace;
    }

    // Header
    LogDBRecordHeader hdr;
    hdr.ts = ts;
    DmWrite(p, 0, &hdr, sizeof(hdr));

    // Variable data
    UInt16 off = sizeof(hdr);
    DmWrite(p, off, appName, StrLen(appName)+1);
    off += (UInt16)(StrLen(appName)+1);
    DmWrite(p, off, message, StrLen(message)+1);

    MemHandleUnlock(h);
    // Mark as dirty
    DmReleaseRecord(db, index, true);

    DmCloseDatabase(db);
    return errNone;
}

static Err prvOpenDbRead(DmOpenRef* dbP) {
    LocalID dbId;
    UInt16 cardNo = 0;
    DmSearchStateType stateInfo;
    Err err = DmGetNextDatabaseByTypeCreator(true, &stateInfo, LOGDB_DB_TYPE, LOGDB_DB_CREATOR, true, &cardNo, &dbId);
    if (err) return err; // not found
    *dbP = DmOpenDatabase(cardNo, dbId, dmModeReadOnly);
    if (!*dbP) return DmGetLastErr();
    return errNone;
}

Err LogDB_Clear(void) {
    DmOpenRef db = NULL;
    Err err = prvOpenOrCreateDb(&db);
    if (err) return err;

    // Delete all records
    UInt16 count = DmNumRecords(db);
    for (UInt16 i = 0; i < count; i++) {
        // Always delete index 0 repeatedly as records shift down
        if (DmRemoveRecord(db, 0) != errNone) break;
    }

    DmCloseDatabase(db);
    return errNone;
}

typedef struct {
    UInt32 ts;
    UInt16 recIndex;
} SortItem;

static Int16 CmpDescByTs(void* a, void* b, Int16 other) {
    // descending timestamp
    const SortItem* pa = (const SortItem*)a;
    const SortItem* pb = (const SortItem*)b;
    if (pa->ts == pb->ts) return 0;
    return (pa->ts < pb->ts) ? 1 : -1;
}

Err LogDB_Enum(Boolean newestFirst, LogDB_EnumCallback cb, void* ctx) {
    if (!cb) return dmErrParamErr;

    DmOpenRef db = NULL;
    Err err = prvOpenDbRead(&db);
    if (err) return (err == dmErrCantFind) ? errNone : err; // no DB means nothing to enumerate

    UInt16 n = DmNumRecords(db);
    if (n == 0) { DmCloseDatabase(db); return errNone; }

    // Build sort index
    MemHandle idxH = MemHandleNew(sizeof(SortItem) * n);
    if (!idxH) { DmCloseDatabase(db); return memErrNotEnoughSpace; }
    SortItem* items = MemHandleLock(idxH);

    UInt16 fill = 0;
    for (UInt16 i = 0; i < n; i++) {
        MemHandle h = DmQueryRecord(db, i);
        if (!h) continue;
        Char* p = MemHandleLock(h);
        if (!p) continue;
        LogDBRecordHeader* hdr = (LogDBRecordHeader*)p;
        items[fill].ts = hdr->ts;
        items[fill].recIndex = i;
        MemHandleUnlock(h);
        fill++;
    }

    // Sort
    SysQSort(items, fill, sizeof(SortItem), CmpDescByTs, 0);

    // Enumerate
    for (UInt16 k = 0; k < fill; k++) {
        UInt16 idx = newestFirst ? k : (fill - 1 - k);
        UInt16 recIndex = items[idx].recIndex;
        MemHandle h = DmQueryRecord(db, recIndex);
        if (!h) continue;
        Char* p = MemHandleLock(h);
        if (!p) continue;

        LogDBRecordHeader* hdr = (LogDBRecordHeader*)p;
        Char* app = p + sizeof(LogDBRecordHeader);
        Char* msg = app + StrLen(app) + 1;

        Boolean cont = cb(hdr->ts, app, msg, ctx);

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
    TimSecondsToDateTime(ts, &dt);
    // YYYY-MM-DD HH:MM
    StrPrintF(outBuf, "%04d-%02d-%02d %02d:%02d",
        dt.year, dt.month, dt.day, dt.hour, dt.minute);
}
