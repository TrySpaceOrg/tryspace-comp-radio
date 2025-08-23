#include "radio_app.h"

RADIO_AppData_t RADIO_AppData;

/* Downlink subscription table handle and pointer */
CFE_TBL_Handle_t RADIO_SubsTblHandle;
RADIO_Subs_t *RADIO_SubsTblPtr = NULL;

/* Downlink pipe */
#define RADIO_DOWNLINK_PIPE_DEPTH 50
#define RADIO_DOWNLINK_PIPE_NAME "RADIO_DOWNLINK_PIPE"
uint32 RADIO_DownlinkPipe;

/*
** Application entry point and main process loop
*/
void RADIO_AppMain(void)
{
    int32 status = OS_SUCCESS;

    /*
    ** Create the first performance log entry
    */
    CFE_ES_PerfLogEntry(RADIO_PERF_ID);

    /*
    ** Perform application initialization
    */
    status = RADIO_AppInit();
    if (status != CFE_SUCCESS)
    {
        RADIO_AppData.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** Main loop
    */
    while (CFE_ES_RunLoop(&RADIO_AppData.RunStatus) == true)
    {
        /*
        ** Performance log exit stamp
        */
        CFE_ES_PerfLogExit(RADIO_PERF_ID);

        /*
        ** Pend on the arrival of the next software bus message
        ** Note that this is the standard, but timeouts are available
        */
        status = CFE_SB_ReceiveBuffer((CFE_SB_Buffer_t **)&RADIO_AppData.MsgPtr, RADIO_AppData.CmdPipe, CFE_SB_PEND_FOREVER);

        /*
        ** Begin performance metrics on anything after this line.
        */
        CFE_ES_PerfLogEntry(RADIO_PERF_ID);

        /*
        ** If the CFE_SB_ReceiveBuffer was successful, then continue to process the command packet
        ** If not, then exit the application in error.
        ** Note that a SB read error should not always result in an app quitting.
        */
        if (status == CFE_SUCCESS)
        {
            RADIO_ProcessCommandPacket();
        }
        else
        {
            CFE_EVS_SendEvent(RADIO_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "RADIO: SB Pipe Read Error = %d",
                              (int)status);
            RADIO_AppData.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    /*
    ** Disable component, which cleans up the interface, upon exit
    */
    RADIO_Disable();

    /*
    ** Performance log exit stamp
    */
    CFE_ES_PerfLogExit(RADIO_PERF_ID);

    /*
    ** Exit the application
    */
    CFE_ES_ExitApp(RADIO_AppData.RunStatus);
}

/*
** Initialize application
*/
int32 RADIO_AppInit(void)
{
    int32 status = OS_SUCCESS;

    RADIO_AppData.RunStatus = CFE_ES_RunStatus_APP_RUN;

    /*
    ** Register the events
    */
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY); /* as default, no filters are used */
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("RADIO: Error registering for event services: 0x%08X\n", (unsigned int)status);
        return status;
    }

    /* Register and load downlink subscription table */
    status = CFE_TBL_Register(&RADIO_SubsTblHandle, "RADIO_Subs", sizeof(RADIO_Subs_t), CFE_TBL_OPT_DEFAULT, NULL);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(RADIO_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "Error registering downlink table,RC=0x%08X", (unsigned int)status);
        return status;
    }
    status = CFE_TBL_Load(RADIO_SubsTblHandle, CFE_TBL_SRC_FILE, "/cf/radio_sub.tbl");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(RADIO_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "Error loading downlink table,RC=0x%08X", (unsigned int)status);
        return status;
    }
    status = CFE_TBL_GetAddress((void **)&RADIO_SubsTblPtr, RADIO_SubsTblHandle);
    if (status != CFE_SUCCESS && status != CFE_TBL_INFO_UPDATED)
    {
        CFE_EVS_SendEvent(RADIO_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "Error getting downlink table addr,RC=0x%08X", (unsigned int)status);
        return status;
    }

    /* Create downlink pipe */
    status = CFE_SB_CreatePipe(&RADIO_DownlinkPipe, RADIO_DOWNLINK_PIPE_DEPTH, RADIO_DOWNLINK_PIPE_NAME);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(RADIO_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "Error Creating Downlink Pipe,RC=0x%08X", (unsigned int)status);
        return status;
    }

    /* Subscribe to downlink messages from table */
    if (RADIO_SubsTblPtr != NULL)
    {
        for (int i = 0; i < RADIO_CFG_MAX_SUBSCRIPTIONS; i++)
        {
            if (!CFE_SB_IsValidMsgId(RADIO_SubsTblPtr->Subs[i].Stream))
            {
                break;
            }
            status = CFE_SB_SubscribeEx(RADIO_SubsTblPtr->Subs[i].Stream, RADIO_DownlinkPipe, RADIO_SubsTblPtr->Subs[i].Flags, RADIO_SubsTblPtr->Subs[i].BufLimit);
            if (status != CFE_SUCCESS)
            {
                CFE_EVS_SendEvent(RADIO_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "Error subscribing to downlink stream 0x%X,RC=0x%08X", (unsigned int)CFE_SB_MsgIdToValue(RADIO_SubsTblPtr->Subs[i].Stream), (unsigned int)status);
            }
        }
    }

    /*
    ** Create the Software Bus command pipe
    */
    status = CFE_SB_CreatePipe(&RADIO_AppData.CmdPipe, RADIO_PIPE_DEPTH, "RADIO_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(RADIO_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "Error Creating SB Pipe,RC=0x%08X",
                          (unsigned int)status);
        return status;
    }

    /*
    ** Subscribe to ground commands
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(RADIO_CMD_MID), RADIO_AppData.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(RADIO_SUB_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Error Subscribing to HK Gnd Cmds, MID=0x%04X, RC=0x%08X", RADIO_CMD_MID,
                          (unsigned int)status);
        return status;
    }

    /*
    ** Subscribe to housekeeping (hk) message requests
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(RADIO_REQ_HK_MID), RADIO_AppData.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(RADIO_SUB_REQ_HK_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Error Subscribing to HK Request, MID=0x%04X, RC=0x%08X", RADIO_REQ_HK_MID,
                          (unsigned int)status);
        return status;
    }

    /*
    ** Initialize the published HK message - this HK message will contain the
    ** telemetry that has been defined in the RADIO_Hk_tlm_t for this app.
    */
    CFE_MSG_Init(CFE_MSG_PTR(RADIO_AppData.HkTelemetryPkt.TlmHeader), CFE_SB_ValueToMsgId(RADIO_HK_TLM_MID),
                 RADIO_HK_TLM_LNGTH);

    /*
    ** Reset all counters during application initialization
    */
    RADIO_ResetCounters();

    /*
    ** Initialize application data
    ** Note that counters are excluded as they were reset in the previous code block
    */
   RADIO_AppData.HkTelemetryPkt.DeviceErrorCount = 0;
   RADIO_AppData.HkTelemetryPkt.DeviceCount      = 0;
   RADIO_AppData.HkTelemetryPkt.DeviceEnabled    = RADIO_DEVICE_DISABLED;

   /* Initialize SPI and GPIO devices (reference radio_cli.c) */
   RADIO_AppData.RadioSpi.bus = RADIO_CFG_SPI_BUS;
   RADIO_AppData.RadioSpi.cs = RADIO_CFG_SPI_CS;
   RADIO_AppData.RadioSpi.isOpen = SPI_DEVICE_CLOSED;

   RADIO_AppData.RadioPowerGpio.pin = RADIO_CFG_GPIO_POWER_PIN;
   RADIO_AppData.RadioPowerGpio.direction = GPIO_OUTPUT;
   RADIO_AppData.RadioPowerGpio.isOpen = GPIO_CLOSED;

   RADIO_AppData.RadioInterruptGpio.pin = RADIO_CFG_GPIO_INTERRUPT_PIN;
   RADIO_AppData.RadioInterruptGpio.direction = GPIO_INPUT;
   RADIO_AppData.RadioInterruptGpio.isOpen = GPIO_CLOSED;

   /* Initialize radio device (SPI + GPIO) */
   status = RADIO_InitDevice(&RADIO_AppData.RadioSpi, &RADIO_AppData.RadioPowerGpio, &RADIO_AppData.RadioInterruptGpio);
   if (status == OS_SUCCESS)
   {
       status = RADIO_PowerOn(&RADIO_AppData.RadioPowerGpio);
       if (status == OS_SUCCESS)
       {
           RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_ENABLED;
       }
       else
       {
           RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_DISABLED;
       }
   }
   else
   {
       RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_DISABLED;
   }

    /*
     ** Send an information event that the app has initialized.
     ** This is useful for debugging the loading of individual applications.
     */
    status = CFE_EVS_SendEvent(RADIO_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                               "RADIO App Initialized. Version %d.%d.%d.%d", RADIO_MAJOR_VERSION,
                               RADIO_MINOR_VERSION, RADIO_REVISION, RADIO_MISSION_REV);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("RADIO: Error sending initialization event: 0x%08X\n", (unsigned int)status);
    }
    return status;
}

/*
** Process packets received on the RADIO command pipe
*/
void RADIO_ProcessCommandPacket(void)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_GetMsgId(RADIO_AppData.MsgPtr, &MsgId);
    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        /*
        ** Ground Commands with command codes fall under the RADIO_CMD_MID (Message ID)
        */
        case RADIO_CMD_MID:
            RADIO_ProcessGroundCommand();
            break;

        /*
        ** Housekeeping requests with command codes fall under the RADIO_REQ_HK_MID (Message ID)
        */
        case RADIO_REQ_HK_MID:
            RADIO_ProcessTelemetryRequest();
            break;

        /*
        ** All other invalid messages that this app doesn't recognize,
        ** increment the command error counter and log as an error event.
        */
        default:
            /* Increment the command error counter upon receipt of an invalid command packet */
            RADIO_AppData.HkTelemetryPkt.CommandErrorCount++;

            /* Send event failure to the console*/
            CFE_EVS_SendEvent(RADIO_PROCESS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RADIO: Invalid command packet, MID = 0x%x", CFE_SB_MsgIdToValue(MsgId));
            break;
    }
    return;
}

/*
** Process ground commands
*/
void RADIO_ProcessGroundCommand(void)
{
    CFE_SB_MsgId_t    MsgId       = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t CommandCode = 0;

    /*
    ** MsgId is only needed if the command code is not recognized. See default case
    */
    CFE_MSG_GetMsgId(RADIO_AppData.MsgPtr, &MsgId);

    /*
    ** Ground Commands have a command code (_CC) associated with them
    ** Pull this command code from the message and then process
    */
    CFE_MSG_GetFcnCode(RADIO_AppData.MsgPtr, &CommandCode);
    switch (CommandCode)
    {
        /*
        ** NOOP Command
        */
        case RADIO_NOOP_CC:
            /*
            ** Verify the command length immediately after CC identification
            */
            if (RADIO_VerifyCmdLength(RADIO_AppData.MsgPtr, sizeof(RADIO_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                #ifdef RADIO_CFG_DEBUG
                    OS_printf("RADIO: RADIO_NOOP_CC received \n");
                #endif

                /* Do any necessary checks, none for a NOOP */

                /* Increment command success or error counter, NOOP can only be successful */
                RADIO_AppData.HkTelemetryPkt.CommandCount++;

                /* Do the action, none for a NOOP */

                /* Increment device success or error counter, none for NOOP as application only */

                /* Send event success or failure to the console, NOOP can only be successful */
                CFE_EVS_SendEvent(RADIO_CMD_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION,
                                    "RADIO: NOOP command received");
            }
            break;

        /*
        ** Reset Counters Command
        */
        case RADIO_RESET_COUNTERS_CC:
            if (RADIO_VerifyCmdLength(RADIO_AppData.MsgPtr, sizeof(RADIO_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                #ifdef RADIO_CFG_DEBUG
                    OS_printf("RADIO: RADIO_RESET_COUNTERS_CC received \n");
                #endif
                RADIO_ResetCounters();
            }
            break;

        /*
        ** Enable Command
        */
        case RADIO_ENABLE_CC:
            if (RADIO_VerifyCmdLength(RADIO_AppData.MsgPtr, sizeof(RADIO_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                #ifdef RADIO_CFG_DEBUG
                    OS_printf("RADIO: RADIO_ENABLE_CC received \n");
                #endif
                RADIO_Enable();
            }
            break;

        /*
        ** Disable Command
        */
        case RADIO_DISABLE_CC:
            if (RADIO_VerifyCmdLength(RADIO_AppData.MsgPtr, sizeof(RADIO_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                #ifdef RADIO_CFG_DEBUG
                    OS_printf("RADIO: RADIO_DISABLE_CC received \n");
                #endif
                RADIO_Disable();
            }
            break;

        /*
        ** Set Configuration Command
        ** Note that this is an example of a command that has additional arguments
        */
        case RADIO_CONFIG_CC:
            if (RADIO_VerifyCmdLength(RADIO_AppData.MsgPtr, sizeof(RADIO_Config_cmd_t)) == OS_SUCCESS)
            {
                #ifdef RADIO_CFG_DEBUG
                    OS_printf("RADIO: RADIO_CONFIG_CC received \n");
                #endif
                RADIO_Configure();
            }
            break;

        /*
        ** Radio Service
        */
       case RADIO_SERVICE_CC:
            if (RADIO_VerifyCmdLength(RADIO_AppData.MsgPtr, sizeof(RADIO_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                #ifdef RADIO_CFG_DEBUG
                    OS_printf("RADIO: RADIO_SERVICE_CC received \n");
                #endif
                RADIO_Service();
            }
            break;  

        /*
        ** Invalid Command Codes
        */
        default:
            /* Increment the command error counter upon receipt of an invalid command */
            RADIO_AppData.HkTelemetryPkt.CommandErrorCount++;

            /* Send invalid command code failure to the console */
            CFE_EVS_SendEvent(RADIO_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                                "RADIO: Invalid command code for packet, MID = 0x%x, cmdCode = 0x%x",
                                CFE_SB_MsgIdToValue(MsgId), CommandCode);
            break;
    }
    return;
}

/*
** Process Telemetry Request - Triggered in response to a telemetry request
*/
void RADIO_ProcessTelemetryRequest(void)
{
    CFE_SB_MsgId_t    MsgId       = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t CommandCode = 0;

    /* MsgId is only needed if the command code is not recognized. See default case */
    CFE_MSG_GetMsgId(RADIO_AppData.MsgPtr, &MsgId);

    /* Pull this command code from the message and then process */
    CFE_MSG_GetFcnCode(RADIO_AppData.MsgPtr, &CommandCode);
    switch (CommandCode)
    {
        case RADIO_REQ_HK_TLM:
            RADIO_ReportHousekeeping();
            break;

        /*
        ** Invalid Command Codes
        */
        default:
            /* Increment the error counter upon receipt of an invalid command */
            RADIO_AppData.HkTelemetryPkt.CommandErrorCount++;

            /* Send invalid command code failure to the console */
            CFE_EVS_SendEvent(RADIO_DEVICE_TLM_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RADIO: Invalid command code for packet, MID = 0x%x, cmdCode = 0x%x",
                              CFE_SB_MsgIdToValue(MsgId), CommandCode);
            break;
    }
    return;
}

/*
** Report Application Housekeeping
*/
void RADIO_ReportHousekeeping(void)
{
    int32 status = OS_SUCCESS;

    /* Use SPI for HK request */
    if (RADIO_AppData.HkTelemetryPkt.DeviceEnabled == RADIO_DEVICE_ENABLED)
    {
        status = RADIO_RequestHK(&RADIO_AppData.RadioSpi,
                                 (RADIO_Device_HK_tlm_t *)&RADIO_AppData.HkTelemetryPkt.DeviceHK);
        if (status == OS_SUCCESS)
        {
            RADIO_AppData.HkTelemetryPkt.DeviceCount++;
        }
        else
        {
            RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;
            CFE_EVS_SendEvent(RADIO_REQ_HK_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RADIO: Request device HK reported error %d", status);
        }
    }
    /* Intentionally do not report errors if disabled */

    /* Time stamp and publish housekeeping telemetry */
    CFE_SB_TimeStampMsg((CFE_MSG_Message_t *)&RADIO_AppData.HkTelemetryPkt);
    CFE_SB_TransmitMsg((CFE_MSG_Message_t *)&RADIO_AppData.HkTelemetryPkt, true);
    return;
}

/*
** Reset all global counter variables
*/
void RADIO_ResetCounters(void)
{
    /* Do any necessary checks, none for reset counters */

    /* Increment command success or error counter, omitted as action is to reset */

    /* Do the action, clear all global counter variables */
    RADIO_AppData.HkTelemetryPkt.CommandErrorCount = 0;
    RADIO_AppData.HkTelemetryPkt.CommandCount      = 0;
    RADIO_AppData.HkTelemetryPkt.DeviceErrorCount  = 0;
    RADIO_AppData.HkTelemetryPkt.DeviceCount       = 0;

    /* Increment device success or error counter, none as application only */

    /* Send event success to the console */
    CFE_EVS_SendEvent(RADIO_CMD_RESET_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "RADIO: RESET counters command received");
    return;
}

/*
** Enable component
*/
void RADIO_Enable(void)
{
    int32 status = OS_SUCCESS;

    /* Enable device using GPIO */
    if (RADIO_AppData.HkTelemetryPkt.DeviceEnabled == RADIO_DEVICE_DISABLED)
    {
        status = RADIO_PowerOn(&RADIO_AppData.RadioPowerGpio);
        if (status == OS_SUCCESS)
        {
            RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_ENABLED;
            CFE_EVS_SendEvent(RADIO_ENABLE_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "RADIO: Device enabled");
        }
        else
        {
            RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;
            CFE_EVS_SendEvent(RADIO_ENABLE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RADIO: Failed to enable device");
        }
    }
    return;
}

/*
** Disable component
*/
void RADIO_Disable(void)
{
    int32 status = OS_SUCCESS;

    /* Disable device using GPIO */
    if (RADIO_AppData.HkTelemetryPkt.DeviceEnabled == RADIO_DEVICE_ENABLED)
    {
        status = RADIO_PowerOff(&RADIO_AppData.RadioPowerGpio);
        if (status == OS_SUCCESS)
        {
            RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_DISABLED;
            CFE_EVS_SendEvent(RADIO_DISABLE_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "RADIO: Device disabled");
        }
        else
        {
            RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;
            CFE_EVS_SendEvent(RADIO_DISABLE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RADIO: Failed to disable device");
        }
    }
    return;
}

/*
** Configure component
*/
void RADIO_Configure(void)
{
    int32 status        = OS_SUCCESS;
    int32 device_status = OS_SUCCESS;
    RADIO_Config_cmd_t *config_cmd    = (RADIO_Config_cmd_t *)RADIO_AppData.MsgPtr;

    /* Do any necessary checks, confirm that device is currently enabled */
    if (RADIO_AppData.HkTelemetryPkt.DeviceEnabled != RADIO_DEVICE_ENABLED)
    {
        status = OS_ERROR;
        /* Increment command error count */
        RADIO_AppData.HkTelemetryPkt.CommandErrorCount++;

        /* Send event logging failure of check to the console */
        CFE_EVS_SendEvent(RADIO_CMD_CONFIG_EN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "RADIO: Configuration command invalid when device disabled");
    }

    /* Do any necessary checks, confirm valid configuration value */
    if (config_cmd->DeviceCfg == 65535)
    {
        status = OS_ERROR;
        /* Increment command error count */
        RADIO_AppData.HkTelemetryPkt.CommandErrorCount++;

        /* Send event logging failure of check to the console */
        CFE_EVS_SendEvent(RADIO_CMD_CONFIG_VAL_ERR_EID, CFE_EVS_EventType_ERROR,
                          "RADIO: Configuration command with value %u is invalid", config_cmd->DeviceCfg);
    }

    if (status == OS_SUCCESS)
    {
        /* Increment command success counter */
        RADIO_AppData.HkTelemetryPkt.CommandCount++;

        /* Do the action, command device with new configuration using SPI */
        device_status = RADIO_SetConfiguration(&RADIO_AppData.RadioSpi, (RADIO_Device_Config_t *)config_cmd);
        if (device_status == OS_SUCCESS)
        {
            /* Increment device success counter */
            RADIO_AppData.HkTelemetryPkt.DeviceCount++;

            /* Send device event success to the console */
            CFE_EVS_SendEvent(RADIO_CMD_CONFIG_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "RADIO: Configuration command received");
        }
        else
        {
            /* Increment device error counter */
            RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;

            /* Send device event failure to the console */
            CFE_EVS_SendEvent(RADIO_CMD_CONFIG_DEV_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RADIO: Configuration command failed");
        }
    }
    return;
}

/*
** Service the radio, sending and receiving data available
*/
void RADIO_Service(void)
{
    uint8 actual_length = 0;
    CFE_SB_Buffer_t *SBBufPtr;
    uint32 max_rx_transactions = RADIO_CFG_MAX_RX_MSGS_PER_POLL;

    if (RADIO_AppData.HkTelemetryPkt.DeviceEnabled == RADIO_DEVICE_ENABLED)
    {
        /* Outer loop - perform up to max_rx_transactions receive attempts */
        while (max_rx_transactions > 0)
        {
            /* Receive data from the radio into the tail of the buffer */
            RADIO_ReceiveData(&RADIO_AppData.RadioSpi, RADIO_AppData.ReceiveBuffer + RADIO_AppData.ReceiveBuffLength,
                            RADIO_MAX_PAYLOAD_SIZE - RADIO_AppData.ReceiveBuffLength, &actual_length);
            #ifdef RADIO_CFG_DEBUG
            OS_printf("RADIO_Service: Received %u bytes from SPI\n", actual_length);
            if (actual_length > 0) {
                OS_printf("RADIO_Service: SPI payload: ");
                for (uint32 i = 0; i < actual_length; ++i) {
                    OS_printf("%02X ", RADIO_AppData.ReceiveBuffer[RADIO_AppData.ReceiveBuffLength + i]);
                }
                OS_printf("\n");
            }
            #endif
            if (actual_length == 0)
            {
                /* No more data available from device */
                break;
            }

            /* Advance the buffer length, but guard against overflow */
            RADIO_AppData.ReceiveBuffLength += actual_length;
            #ifdef RADIO_CFG_DEBUG
            OS_printf("RADIO_Service: Buffer length after append: %u\n", RADIO_AppData.ReceiveBuffLength);
            #endif
            if (RADIO_AppData.ReceiveBuffLength > RADIO_MAX_PAYLOAD_SIZE)
            {
                /* Buffer overflow - drop contents and report error */
                RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;
                CFE_EVS_SendEvent(RADIO_REQ_DATA_ERR_EID, CFE_EVS_EventType_ERROR,
                                "RADIO: receive buffer overflow, len=%u", (unsigned int)RADIO_AppData.ReceiveBuffLength);
                RADIO_AppData.ReceiveBuffLength = 0;
                break;
            }

            /* Try to extract one or more complete CFE messages from the accumulated buffer */
            while (RADIO_AppData.ReceiveBuffLength >= sizeof(CFE_MSG_CommandHeader_t))
            {
                SBBufPtr = (CFE_SB_Buffer_t *)RADIO_AppData.ReceiveBuffer;
                CFE_MSG_Size_t msg_size = 0;
                CFE_Status_t get_size_status = CFE_MSG_GetSize((CFE_MSG_Message_t *)&SBBufPtr->Msg, &msg_size);
                #ifdef RADIO_CFG_DEBUG
                OS_printf("RADIO_Service: Trying to extract CFE message, header size=%zu, buffer size=%u\n",
                        sizeof(CFE_MSG_CommandHeader_t), RADIO_AppData.ReceiveBuffLength);
                OS_printf("RADIO_Service: Header bytes: ");
                for (uint32 i = 0; i < sizeof(CFE_MSG_CommandHeader_t) && i < RADIO_AppData.ReceiveBuffLength; ++i) {
                    OS_printf("%02X ", RADIO_AppData.ReceiveBuffer[i]);
                }
                OS_printf("\n");
                OS_printf("RADIO_Service: get_size_status=%d, msg_size=%zu\n", get_size_status, (size_t)msg_size);
                #endif
                if (get_size_status != CFE_SUCCESS)
                {
                    /* Malformed header or error extracting size; drop buffer */
                    RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;
                    CFE_EVS_SendEvent(RADIO_REQ_DATA_ERR_EID, CFE_EVS_EventType_ERROR,
                                    "RADIO: Failed to get message size from header, rc=%d", (int)get_size_status);
                    RADIO_AppData.ReceiveBuffLength = 0;
                    break;
                }

                /* If the header reports a size larger than we currently have, wait for more data */
                if (msg_size > RADIO_AppData.ReceiveBuffLength)
                {
                    #ifdef RADIO_CFG_DEBUG
                    OS_printf("RADIO_Service: Incomplete message, need %zu bytes, have %u\n", (size_t)msg_size, RADIO_AppData.ReceiveBuffLength);
                    #endif
                    break; /* need more bytes */
                }

                /* We have a complete message - transmit it onto the software bus */
                if (CFE_SB_TransmitMsg((CFE_MSG_Message_t *)SBBufPtr, true) == CFE_SUCCESS)
                {
                    RADIO_AppData.HkTelemetryPkt.DeviceCount++;
                    #ifdef RADIO_CFG_DEBUG
                    OS_printf("RADIO_Service: Transmitted CFE message of size %zu\n", (size_t)msg_size);
                    #endif
                }
                else
                {
                    RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;
                    CFE_EVS_SendEvent(RADIO_REQ_DATA_ERR_EID, CFE_EVS_EventType_ERROR,
                                    "RADIO: Failed to transmit received message to SB");
                    #ifdef RADIO_CFG_DEBUG
                    OS_printf("RADIO_Service: Failed to transmit CFE message\n");
                    #endif
                }

                /* Remove the processed message from the front of the buffer */
                if (msg_size < RADIO_AppData.ReceiveBuffLength)
                {
                    memmove(RADIO_AppData.ReceiveBuffer, RADIO_AppData.ReceiveBuffer + msg_size,
                            RADIO_AppData.ReceiveBuffLength - msg_size);
                }
                RADIO_AppData.ReceiveBuffLength -= msg_size;
                #ifdef RADIO_CFG_DEBUG
                OS_printf("RADIO_Service: Buffer length after message extraction: %u\n", RADIO_AppData.ReceiveBuffLength);
                #endif
            }

            /* Decrement receive attempts and continue to try to read more packets */
            max_rx_transactions--;
        }

        /* Downlink: forward packets from downlink pipe to radio */
        uint32 pkt_count = 0;
        uint32 pkt_debug_count = 0;
        CFE_Status_t sb_status;
        do {
            sb_status = CFE_SB_ReceiveBuffer(&SBBufPtr, RADIO_DownlinkPipe, CFE_SB_POLL);
            if (sb_status == CFE_SUCCESS && SBBufPtr != NULL)
            {
                /* Transmit the packet over the radio interface */
                CFE_MSG_Size_t msg_size = 0;
                CFE_MSG_GetSize((CFE_MSG_Message_t *)&SBBufPtr->Msg, &msg_size);
                int32 tx_status = RADIO_SendData(&RADIO_AppData.RadioSpi, (uint8 *)&SBBufPtr->Msg, msg_size);
                if (tx_status == OS_SUCCESS)
                {
                    RADIO_AppData.HkTelemetryPkt.DeviceCount++;
                    #ifdef RADIO_CFG_DEBUG
                    OS_printf("RADIO_Service: Downlink packet transmitted\n");
                    #endif
                }
                else
                {
                    RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;
                    CFE_EVS_SendEvent(RADIO_REQ_DATA_ERR_EID, CFE_EVS_EventType_ERROR,
                                    "RADIO: Failed to transmit downlink packet, status=%d", (int)tx_status);
                }
                pkt_debug_count++;
            }
            pkt_count++;
        } while (sb_status == CFE_SUCCESS && pkt_count < RADIO_CFG_MAX_TX_MSGS_PER_POLL);
        #ifdef RADIO_CFG_DEBUG
        OS_printf("RADIO_Service: Downlink packets processed this call: %u\n", pkt_debug_count);
        #endif
    }

    return;
}

/*
** Verify command packet length matches expected
*/
int32 RADIO_VerifyCmdLength(CFE_MSG_Message_t *msg, uint16 expected_length)
{
    int32             status        = OS_SUCCESS;
    CFE_SB_MsgId_t    msg_id        = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t cmd_code      = 0;
    size_t            actual_length = 0;

    CFE_MSG_GetSize(msg, &actual_length);
    if (expected_length != actual_length)
    {
        CFE_MSG_GetMsgId(msg, &msg_id);
        CFE_MSG_GetFcnCode(msg, &cmd_code);

        CFE_EVS_SendEvent(RADIO_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid msg length: ID = 0x%X,  CC = %d, Len = %ld, Expected = %d",
                          CFE_SB_MsgIdToValue(msg_id), cmd_code, actual_length, expected_length);

        status = OS_ERROR;

        /* Increment the command error counter upon receipt of an invalid command length */
        RADIO_AppData.HkTelemetryPkt.CommandErrorCount++;
    }
    return status;
}
