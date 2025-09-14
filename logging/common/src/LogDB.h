#ifndef LOGDB_H
#define LOGDB_H

#include <PalmOS.h>

/* Shared Debug Log database constants */
#define LOGDB_NAME "DebugLog"
#define LOGDB_TYPE 'DATA'
#define LOGDB_CREATOR 'LgDB'

/* Initialize (open-or-create) the log DB and set the current app name. */
Err LogDB_Init(const Char *appName);

/* Close DB when app exits (safe if called repeatedly). */
void LogDB_Close(void);

/* Append one log message (timestamped internally). */
Err LogDB_Log(const Char *message);

/* Remove all log records. */
Err LogDB_ClearAll(void);

/* Lightweight reader helpers for the viewer */
typedef struct LogDB_IterTag
{
    DmOpenRef dbR;
    UInt16 index;
    UInt16 count;
} LogDB_Iter;

/* Begin iteration over all records (returns errNone or dmErrCantOpen). */
Err LogDB_IterBegin(LogDB_Iter *it);

/* Get next record; returns NULL when done.
   Out params (seconds, appPtr, msgPtr) point into locked memory.
   You MUST call LogDB_IterUnlock after you’re done with the record. */
MemHandle LogDB_IterNext(LogDB_Iter *it, UInt32 *seconds, Char **appPtr, Char **msgPtr);

/* Unlock the currently locked MemHandle returned by IterNext. */
void LogDB_IterUnlock(MemHandle h);

/* Finish iteration. */
void LogDB_IterEnd(LogDB_Iter *it);

#endif /* LOGDB_H */
