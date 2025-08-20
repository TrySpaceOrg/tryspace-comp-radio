#include "radio_app.h"

RADIO_AppData_t RADIO_AppData;

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
    ** Initialize the device packet message
    ** This packet is specific to your application
    */
    CFE_MSG_Init(CFE_MSG_PTR(RADIO_AppData.DevicePkt.TlmHeader), CFE_SB_ValueToMsgId(RADIO_DEVICE_TLM_MID),
                 RADIO_DEVICE_TLM_LNGTH);

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

   /* 
   ** Enable the device by default
   ** This may not be applicable to all applications, but is included here as an example
   */
   RADIO_Enable();

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

        case RADIO_REQ_DATA_TLM:
            RADIO_ReportDeviceTelemetry();
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

    /* Check that device is enabled */
    if (RADIO_AppData.HkTelemetryPkt.DeviceEnabled == RADIO_DEVICE_ENABLED)
    {
        status = RADIO_RequestHK(&RADIO_AppData.RadioUart,
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
** Collect and report device telemetry
*/
void RADIO_ReportDeviceTelemetry(void)
{
    int32 status = OS_SUCCESS;

    /* Check that device is enabled */
    if (RADIO_AppData.HkTelemetryPkt.DeviceEnabled == RADIO_DEVICE_ENABLED)
    {
        status = RADIO_RequestData(&RADIO_AppData.RadioUart,
                                    (RADIO_Device_Data_tlm_t *)&RADIO_AppData.DevicePkt.Radio);
        if (status == OS_SUCCESS)
        {
            /* Update packet count */
            RADIO_AppData.HkTelemetryPkt.DeviceCount++;

            /* Time stamp and publish data telemetry */
            CFE_SB_TimeStampMsg((CFE_MSG_Message_t *)&RADIO_AppData.DevicePkt);
            CFE_SB_TransmitMsg((CFE_MSG_Message_t *)&RADIO_AppData.DevicePkt, true);
        }
        else
        {
            RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;
            CFE_EVS_SendEvent(RADIO_REQ_DATA_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RADIO: Request device data reported error %d", status);
        }
    }
    /* Intentionally do not report errors if device disabled */
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

    /* Do any necessary checks, confirm that device is currently disabled */
    if (RADIO_AppData.HkTelemetryPkt.DeviceEnabled == RADIO_DEVICE_DISABLED)
    {
        /* Increment command success counter */
        RADIO_AppData.HkTelemetryPkt.CommandCount++;

        /*
        ** Do the action, initialize hardware interface and set enabled
        */
        RADIO_AppData.RadioUart.deviceString  = RADIO_CFG_STRING;
        RADIO_AppData.RadioUart.handle        = RADIO_CFG_HANDLE;
        RADIO_AppData.RadioUart.isOpen        = PORT_CLOSED;
        RADIO_AppData.RadioUart.baud          = RADIO_CFG_BAUDRATE_HZ;
        RADIO_AppData.RadioUart.access_option = uart_access_flag_RDWR;

        status = uart_init_port(&RADIO_AppData.RadioUart);
        if (status == OS_SUCCESS)
        {
            RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_ENABLED;

            /* Increment device success counter */
            RADIO_AppData.HkTelemetryPkt.DeviceCount++;

            /* Send device event success to the console */
            CFE_EVS_SendEvent(RADIO_ENABLE_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "RADIO: Device enabled successfully");
        }
        else
        {
            /* Increment device error counter */
            RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;

            /* Send device event failure to the console */
            CFE_EVS_SendEvent(RADIO_UART_INIT_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RADIO: Device UART port initialization error %d", status);
        }
    }
    else
    {
        /* Increment command error count */
        RADIO_AppData.HkTelemetryPkt.CommandErrorCount++;

        /* Send command event failure to the console */
        CFE_EVS_SendEvent(RADIO_ENABLE_ERR_EID, CFE_EVS_EventType_ERROR,
                          "RADIO: Device enable failed, already enabled");
    }
    return;
}

/*
** Disable component
*/
void RADIO_Disable(void)
{
    int32 status = OS_SUCCESS;

    /* Do any necessary checks, confirm that device is currently enabled */
    if (RADIO_AppData.HkTelemetryPkt.DeviceEnabled == RADIO_DEVICE_ENABLED)
    {
        /* Increment command success counter */
        RADIO_AppData.HkTelemetryPkt.CommandCount++;

        /*
        ** Do the action, close hardware interface and set disabled
        */
        status = uart_close_port(&RADIO_AppData.RadioUart);
        if (status == OS_SUCCESS)
        {
            RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_DISABLED;

            /* Increment device success counter */
            RADIO_AppData.HkTelemetryPkt.DeviceCount++;

            /* Send device event success to the console */
            CFE_EVS_SendEvent(RADIO_DISABLE_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "RADIO: Device disabled successfully");
        }
        else
        {
            /* Increment device error counter */
            RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;

            /* Send device event failure to the console */
            CFE_EVS_SendEvent(RADIO_UART_CLOSE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RADIO: Device UART port close error %d", status);
        }
    }
    else
    {
        /* Increment command error count */
        RADIO_AppData.HkTelemetryPkt.CommandErrorCount++;

        /* Send command event failure to the console */
        CFE_EVS_SendEvent(RADIO_DISABLE_ERR_EID, CFE_EVS_EventType_ERROR,
                          "RADIO: Device disable failed, already disabled");
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

        /* Do the action, command device to with a new configuration */
        device_status = RADIO_CommandDevice(&RADIO_AppData.RadioUart, RADIO_DEVICE_CFG_CMD, config_cmd->DeviceCfg);
        if (device_status == OS_SUCCESS)
        {
            /* Increment device success counter */
            RADIO_AppData.HkTelemetryPkt.DeviceCount++;

            /* Send device event success to the console */
            CFE_EVS_SendEvent(RADIO_CMD_CONFIG_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "RADIO: Configuration command received: %u", config_cmd->DeviceCfg);
        }
        else
        {
            /* Increment device error counter */
            RADIO_AppData.HkTelemetryPkt.DeviceErrorCount++;

            /* Send device event failure to the console */
            CFE_EVS_SendEvent(RADIO_CMD_CONFIG_DEV_ERR_EID, CFE_EVS_EventType_ERROR,
                              "RADIO: Configuration command received: %u", config_cmd->DeviceCfg);
        }
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
