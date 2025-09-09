#include <PalmOS.h>
#include "HelloPalm.h"

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
           WinEraseChars("Low button pressed", StrLen("Low button pressed"),
               40, 90);
           WinDrawChars("High button pressed", StrLen("High button pressed"),
               40, 90);
         }
       else if (eventP->data.ctlEnter.controlID == MainLowButton)
         {
        	WinEraseChars("High button pressed", StrLen("High button pressed"),
               40, 90);
           WinDrawChars("Low button pressed", StrLen("Low button pressed"),
               40, 90);
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
	
   return(err);
 }

