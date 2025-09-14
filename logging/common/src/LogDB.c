#include "LogDB.h"

static DmOpenRef sLogDB = NULL;
static Char sAppName[32]; /* short name is fine; truncated if needed */

static Err LogDB_OpenOrCreate(void)
{
    Err err = errNone;
    UInt16 mode = dmModeReadWrite;
    LocalID dbID;

    /* Try open by Type/Creator first */
    sLogDB = DmOpenDatabaseByTypeCreator(LOGDB_TYPE, LOGDB_CREATOR, mode);
    if (sLogDB != NULL)
        return errNone;

    /* Create if missing (ignore 'already exists') */
    err = DmCreateDatabase(0, LOGDB_NAME, LOGDB_CREATOR, LOGDB_TYPE, false);
    if (err != errNone && err != dmErrAlreadyExists)
        return err;

    /* Look up the DB ID */
    dbID = DmFindDatabase(0, LOGDB_NAME);
    if (dbID == 0)
    {
        /* Return the system’s last error if lookup failed */
        return DmGetLastErr();
    }

    /* Set Backup bit so HotSync backs up the log */
    {
        UInt16 attrs = 0, version = 0;
        UInt32 crDate = 0, modDate = 0, bckDate = 0, modNum = 0;
        UInt32 type = LOGDB_TYPE, creator = LOGDB_CREATOR;
        LocalID appInfoID = 0, sortInfoID = 0;
        Char name[dmDBNameLength];

        DmDatabaseInfo(0, dbID, name, &attrs, &version, &crDate, &modDate, &bckDate,
                       &modNum, &appInfoID, &sortInfoID, &type, &creator);
        /* Make sure DB is backed up and beamable (no copy prevention). */
        attrs |= dmHdrAttrBackup;
        attrs &= ~dmHdrAttrCopyPrevention;
        DmSetDatabaseInfo(0, dbID, NULL, &attrs, NULL, NULL, NULL, NULL,
                          NULL, NULL, NULL, NULL, NULL);
    }

    /* Open RW */
    sLogDB = DmOpenDatabase(0, dbID, mode);
    if (sLogDB == NULL)
        return dmErrCantOpen;

    return errNone;
}

Err LogDB_Init(const Char *appName)
{
    Err err;
    UInt16 n;

    if (appName == NULL)
        return dmErrInvalidParam;

    n = StrLen(appName);
    if (n >= sizeof(sAppName))
        n = sizeof(sAppName) - 1;
    MemMove(sAppName, appName, n);
    sAppName[n] = 0;

    err = LogDB_OpenOrCreate();
    return err;
}

void LogDB_Close(void)
{
    if (sLogDB != NULL)
    {
        DmCloseDatabase(sLogDB);
        sLogDB = NULL;
    }
}

Err LogDB_Log(const Char *message)
{
    Err err;
    UInt32 secs;
    MemHandle h;
    UInt16 index;
    Char *dst;
    UInt32 size;
    UInt16 appLen, msgLen;

    if (sLogDB == NULL)
    {
        err = LogDB_OpenOrCreate();
        if (err != errNone)
            return err;
    }

    if (message == NULL)
        message = "";

    secs = TimGetSeconds();

    appLen = (UInt16)StrLen(sAppName);
    msgLen = (UInt16)StrLen(message);
    size = 4 + (UInt32)appLen + 1 + (UInt32)msgLen + 1;

    h = DmNewRecord(sLogDB, &index, size);
    if (h == NULL)
        return dmErrMemError;

    dst = (Char *)MemHandleLock(h);
    if (dst == NULL)
    {
        DmRemoveRecord(sLogDB, index);
        return dmErrMemError;
    }

    /* [UInt32 seconds][appName\0][message\0] */
    DmWrite(dst, 0, &secs, 4);
    DmWrite(dst, 4, sAppName, appLen + 1);
    DmWrite(dst, 4 + appLen + 1, message, msgLen + 1);

    MemHandleUnlock(h);

    err = DmReleaseRecord(sLogDB, index, true);
    return err;
}

Err LogDB_ClearAll(void)
{
    Err err;
    UInt16 n, i;

    if (sLogDB == NULL)
    {
        err = LogDB_OpenOrCreate();
        if (err != errNone)
            return err;
    }

    n = DmNumRecords(sLogDB);
    for (i = 0; i < n; i++)
    {
        if (DmRemoveRecord(sLogDB, 0) != errNone)
            break;
    }
    return errNone;
}

/* --- Iteration helpers for viewer --- */

Err LogDB_IterBegin(LogDB_Iter *it)
{
    if (it == NULL)
        return dmErrInvalidParam;
    MemSet(it, sizeof(LogDB_Iter), 0);
    it->dbR = DmOpenDatabaseByTypeCreator(LOGDB_TYPE, LOGDB_CREATOR, dmModeReadOnly);
    if (it->dbR == NULL)
        return dmErrCantOpen;
    it->count = DmNumRecords(it->dbR);
    it->index = 0;
    return errNone;
}

MemHandle LogDB_IterNext(LogDB_Iter *it, UInt32 *seconds, Char **appPtr, Char **msgPtr)
{
    MemHandle h;
    Char *p;
    UInt16 len;

    if (it == NULL || it->dbR == NULL)
        return NULL;
    if (it->index >= it->count)
        return NULL;

    h = DmQueryRecord(it->dbR, it->index);
    it->index++;
    if (h == NULL)
        return NULL;

    p = (Char *)MemHandleLock(h);
    if (p == NULL)
        return NULL;

    if (seconds != NULL)
    {
        *seconds = *(UInt32 *)(p);
    }
    p += 4;

    if (appPtr != NULL)
        *appPtr = p;
    len = (UInt16)StrLen(p);
    p += (UInt32)len + 1;

    if (msgPtr != NULL)
        *msgPtr = p;

    return h;
}

void LogDB_IterUnlock(MemHandle h)
{
    if (h != NULL)
        MemHandleUnlock(h);
}

void LogDB_IterEnd(LogDB_Iter *it)
{
    if (it != NULL && it->dbR != NULL)
    {
        DmCloseDatabase(it->dbR);
        it->dbR = NULL;
    }
}
