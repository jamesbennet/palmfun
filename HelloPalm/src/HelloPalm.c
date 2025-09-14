#include <PalmOS.h>
#include "HelloPalm.h"
#include "LogDB.h"

// Define version 4.0 as the minimum OS version
#define MinOSVersion	sysMakeROMVersion(4,0,0,sysROMStageRelease,0)

static Err RomVersionCompatible(UInt32 requiredVersion)
{
  UInt32 romVersion;
  FormType *frmP;


  FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);

  if (romVersion < requiredVersion)
  {
    frmP = FrmInitForm(ROMMsgForm);
    FrmDoDialog(frmP);		// Display the invalid ROM form
    FrmDeleteForm(frmP);
    return sysErrRomIncompatible;
  }

  return errNone;
}

static Boolean AppHandleEvent(EventPtr eventP)
 {
   UInt16 formId;
   Boolean handled = false;
   FormType *frmP;

   if (eventP->eType == frmLoadEvent)
     {
       // Initialize and activate the form resource. 
       formId = eventP->data.frmLoad.formID;
       frmP = FrmInitForm(formId);
       FrmSetActiveForm(frmP);
       handled = true;
     }
   else if (eventP->eType == frmOpenEvent)
     {
       // Load the form resource.
       frmP = FrmGetActiveForm();
       FrmDrawForm(frmP);
       handled = true;
     }
   else if (eventP->eType == appStopEvent)
     {
       // Load the form resource.
       frmP = FrmGetActiveForm();
       FrmEraseForm(frmP);
       FrmDeleteForm(frmP);
       handled = true;
     }
    else if (eventP->eType == menuEvent)
    {
      if (eventP->data.menu.itemID == MenuAboutMenuHelloPalm)
      {
        MenuEraseStatus(0);	// Clear the menu from the display.
        frmP = FrmInitForm(AboutForm);
        FrmDoDialog(frmP);					// Display the About Box.
        FrmDeleteForm(frmP);
        handled = true;
      }
      else if (eventP->data.menu.itemID == ConfirmationAlertMenu)
      {
        MenuEraseStatus(0);	// Clear the menu from the display.
        FrmAlert(ConfirmationAlert);
        handled = true;
      }
      else if (eventP->data.menu.itemID == ErrorAlertMenu)
      {
        MenuEraseStatus(0);	// Clear the menu from the display.
        FrmAlert(ErrorAlert);
        handled = true;
      }
      else if (eventP->data.menu.itemID == InformationAlertMenu)
      {
        MenuEraseStatus(0);	// Clear the menu from the display.
        FrmAlert(InformationAlert);
        handled = true;
      }
      else if (eventP->data.menu.itemID == WarningAlertMenu)
      {
        MenuEraseStatus(0);	// Clear the menu from the display.
        FrmAlert(WarningAlert);
        handled = true;
      }
    }
    else if (eventP->eType == ctlSelectEvent)
     {
       if (eventP->data.ctlEnter.controlID == MainHighButton)
         {
           WinEraseChars("High button pressed", StrLen("High button pressed"),
               60, 40);
           WinDrawChars("High button pressed", StrLen("High button pressed"),
               60, 40);
         }
       else if (eventP->data.ctlEnter.controlID == MainLowButton)
         {
        	WinEraseChars("High button pressed", StrLen("High button pressed"),
               60, 40);
           WinDrawChars("Low button pressed", StrLen("Low button pressed"),
               60, 40);
         }
       else if (eventP->data.ctlSelect.controlID == MainSubmitButton)
         {

        ControlType *One,*Two;
        FieldPtr TextFieldPtr;
        char Name[128];
        int NameLength;

         /* Write a log line */
         LogDB_Log("MainSubmitButton Clicked");

        frmP = FrmGetActiveForm();

        One = FrmGetObjectPtr(frmP,
                  FrmGetObjectIndex(frmP, MainItemOneCheckbox));

        Two = FrmGetObjectPtr(frmP,
                  FrmGetObjectIndex(frmP, MainItemTwoCheckbox));

        TextFieldPtr = FrmGetObjectPtr(frmP,
          FrmGetObjectIndex(frmP, MainNameField));

        NameLength = FldGetTextLength(TextFieldPtr);

          if (NameLength > 0)
          {
             MemHandle TextHandle = FldGetTextHandle(TextFieldPtr);
             StrCopy(Name, MemHandleLock(TextHandle));

             MemHandleUnlock(TextHandle);
             Name[NameLength] = NULL;

             WinEraseChars("Must specify name",
               StrLen("Must specify name"), 60, 60);
          }
        else
            {
          WinDrawChars("Must specify name",
            StrLen("Must specify name"), 60, 60);
          }

        if (NameLength)
          {
                       WinEraseChars("Must specify name",
               StrLen("Must specify name"), 60, 60);
                  FldEraseField(TextFieldPtr);
                    WinDrawChars(Name, StrLen(Name), 60, 60);
          }

        if (CtlGetValue(One))
          {
        	WinEraseChars("One Not Checked", StrLen("One Not Checked"),
               80, 80);
           WinDrawChars("One Checked", StrLen("One Checked"),
               80, 80);
           }
        else
             {
        	WinEraseChars("One Not Checked", StrLen("One Not Checked"),
               80, 80);
           WinDrawChars("One Not Checked", StrLen("One Not Checked"),
               80, 80);
           }

        if (CtlGetValue(Two))
          {
        	WinEraseChars("Two Not Checked", StrLen("Two Not Checked"),
               80, 90);
           WinDrawChars("Two Checked", StrLen("Two Checked"),
               80, 90);
           }
        else
             {
        	WinEraseChars("Two Not Checked", StrLen("Two Not Checked"),
               80, 90);
           WinDrawChars("Two Not Checked", StrLen("Two Not Checked"),
               80, 90);
           }
     }
       handled = true;
     }
  return(handled);
 }


UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
 {
   EventType event;
   Err err = 0;

  if ((err = RomVersionCompatible(MinOSVersion)))
    return(err);

   switch (cmd)
     {
       case sysAppLaunchCmdNormalLaunch:

         err = LogDB_Init("HelloPalm");
         if (err) return err;

         FrmGotoForm(MainForm);
			
           do {
             UInt16 MenuError;
            
             EvtGetEvent(&event, evtWaitForever);

             if (! SysHandleEvent(&event))
               if (! MenuHandleEvent(0, &event, &MenuError))
                 if (! AppHandleEvent(&event))
                   FrmDispatchEvent(&event);

           } while (event.eType != appStopEvent);

         break;

	   default:
         break;
     }

  LogDB_Close();

   return(err);
 }

