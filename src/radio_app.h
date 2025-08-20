#ifndef _RADIO_APP_H_
#define _RADIO_APP_H_

/*
** Include Files
*/
#include "cfe.h"
#include "radio_device.h"
#include "radio_events.h"
#include "radio_perfids.h"
#include "radio_msg.h"
#include "radio_msgids.h"
#include "radio_version.h"
#include "hwlib.h"

/*
** Specified pipe depth - how many messages will be queued in the pipe
*/
#define RADIO_PIPE_DEPTH 32

/*
** Enabled and Disabled Definitions
*/
#define RADIO_DEVICE_DISABLED 0
#define RADIO_DEVICE_ENABLED  1

/*
** RADIO global data structure
** The cFE convention is to put all global app data in a single struct.
** This struct is defined in the `radio_app.h` file with one global instance
** in the `.c` file.
*/
typedef struct
{
    /*
    ** Housekeeping telemetry packet
    ** Each app defines its own packet which contains its OWN telemetry
    */
    RADIO_Hk_tlm_t HkTelemetryPkt; /* RADIO Housekeeping Telemetry Packet */

    /*
    ** Operational data  - not reported in housekeeping
    */
    CFE_MSG_Message_t *MsgPtr;    /* Pointer to msg received on software bus */
    CFE_SB_PipeId_t    CmdPipe;   /* Pipe Id for HK command pipe */
    uint32             RunStatus; /* App run status for controlling the application state */

    /*
     ** Device data
     */
    RADIO_Device_tlm_t DevicePkt; /* Device specific data packet */

    /*
    ** Device protocol
    */
    uart_info_t RadioUart; /* Hardware protocol definition */

} RADIO_AppData_t;

/*
** Exported Data
** Extern the global struct in the header for the Unit Test Framework (UTF).
*/
extern RADIO_AppData_t RADIO_AppData; /* RADIO App Data */

/*
**
** Local function prototypes.
**
** Note: Except for the entry point (RADIO_AppMain), these
**       functions are not called from any other source module.
*/
void  RADIO_AppMain(void);
int32 RADIO_AppInit(void);
void  RADIO_ProcessCommandPacket(void);
void  RADIO_ProcessGroundCommand(void);
void  RADIO_ProcessTelemetryRequest(void);
void  RADIO_ReportHousekeeping(void);
void  RADIO_ReportDeviceTelemetry(void);
void  RADIO_ResetCounters(void);
void  RADIO_Enable(void);
void  RADIO_Disable(void);
void  RADIO_Configure(void);
int32 RADIO_VerifyCmdLength(CFE_MSG_Message_t *msg, uint16 expected_length);

#endif /* _RADIO_APP_H_ */
