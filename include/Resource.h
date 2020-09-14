#ifndef RESOURCE_H
#define RESOURCE_H

// Nothing special about 101 except that Microsoft uses it in their example so why not.
#define ACCELERATOR_TABLE_ID 101

// The following are notification codes. Codes below 0x8000 are reserved by Windows.
#define FILE_ACTION_NEW 0x8001
#define FILE_ACTION_OPEN 0x8002
#define FILE_ACTION_SAVE 0x8003
#define FILE_ACTION_SAVEAS 0x8004
#define EDIT_ACTION_REDO 0x8005
#define EDIT_ACTION_UNDO 0x8006
#define EDIT_ACTION_APPLY 0x8007

#endif
