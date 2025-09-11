// LogDB.h - reusable Palm OS logging database
#include <PalmOS.h>

// Database constants
#define LOGDB_DB_NAME   "AppLogsDB"
#define LOGDB_DB_TYPE   'DATA'
#define LOGDB_DB_CREATOR 'LgDb'   // Database's creator (shared logger)

// Callback type for enumeration
typedef Boolean (*LogDB_EnumCallback)(UInt32 ts, const Char* appName, const Char* message, void* ctx);

// Log a message (creates DB if missing). Returns errNone or Palm OS Err.
Err LogDB_Log(const Char* appName, const Char* message);

// Enumerate all records. If newestFirst==true, enumerates by descending timestamp.
// Return early by having callback return false. Returns errNone or Palm OS Err.
Err LogDB_Enum(Boolean newestFirst, LogDB_EnumCallback cb, void* ctx);

// Delete all records in the log DB. Returns errNone or Palm OS Err.
Err LogDB_Clear(void);

// Utility: format timestamp to "YYYY-MM-DD HH:MM"
void LogDB_FormatTimestamp(UInt32 ts, Char* outBuf, UInt16 outLen);