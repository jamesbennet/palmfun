#include <PalmOS.h>
#include "HelloPalm.h"

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
    
  return(handled);
 }


UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
 {
   EventType event;
   Err err = 0;
  
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

