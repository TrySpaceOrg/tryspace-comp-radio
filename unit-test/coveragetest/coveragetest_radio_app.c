#include "radio_app_coveragetest_common.h"
#include "ut_radio_app.h"

typedef struct
{
    uint16      ExpectedEvent;
    uint32      MatchCount;
    const char *ExpectedFormat;
} UT_CheckEvent_t;

/*
 * An example hook function to check for a specific event.
 */
static int32 UT_CheckEvent_Hook(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context,
                                va_list va)
{
    UT_CheckEvent_t *State = UserObj;
    uint16           EventId;
    const char      *Spec;

    /*
     * The CFE_EVS_SendEvent stub passes the EventID as the
     * first context argument.
     */
    if (Context->ArgCount > 0)
    {
        EventId = UT_Hook_GetArgValueByName(Context, "EventID", uint16);
        if (EventId == State->ExpectedEvent)
        {
            if (State->ExpectedFormat != NULL)
            {
                Spec = UT_Hook_GetArgValueByName(Context, "Spec", const char *);
                if (Spec != NULL)
                {
                    /*
                     * Example of how to validate the full argument set.
                     * ------------------------------------------------
                     *
                     * If really desired one can call something like:
                     *
                     * char TestText[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
                     * vsnprintf(TestText, sizeof(TestText), Spec, va);
                     *
                     * And then compare the output (TestText) to the expected fully-rendered string.
                     *
                     * NOTE: While this can be done, use with discretion - This isn't really
                     * verifying that the FSW code unit generated the correct event text,
                     * rather it is validating what the system snprintf() library function
                     * produces when passed the format string and args.
                     *
                     * This type of check has been radionstrated to make tests very fragile,
                     * because it is influenced by many factors outside the control of the
                     * test case.
                     *
                     * __This derived string is not an actual output of the unit under test__
                     */
                    if (strcmp(Spec, State->ExpectedFormat) == 0)
                    {
                        ++State->MatchCount;
                    }
                }
            }
            else
            {
                ++State->MatchCount;
            }
        }
    }

    return 0;
}

/*
 * Helper function to set up for event checking
 * This attaches the hook function to CFE_EVS_SendEvent
 */
static void UT_CheckEvent_Setup(UT_CheckEvent_t *Evt, uint16 ExpectedEvent, const char *ExpectedFormat)
{
    memset(Evt, 0, sizeof(*Evt));
    Evt->ExpectedEvent  = ExpectedEvent;
    Evt->ExpectedFormat = ExpectedFormat;
    UT_SetVaHookFunction(UT_KEY(CFE_EVS_SendEvent), UT_CheckEvent_Hook, Evt);
}

/*
**********************************************************************************
**          TEST CASE FUNCTIONS
**********************************************************************************
*/

void Test_RADIO_AppMain(void)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    /*
     * Test Case For:
     * void RADIO_AppMain( void )
     */
    UT_CheckEvent_t EventTest;

    /*
     * RADIO_AppMain does not return a value,
     * but it has several internal decision points
     * that need to be exercised here.
     *
     * First call it in "nominal" mode where all
     * dependent calls should be successful by default.
     */
    RADIO_AppMain();

    /*
     * Confirm that CFE_ES_ExitApp() was called at the end of execution
     */
    UtAssert_True(UT_GetStubCount(UT_KEY(CFE_ES_ExitApp)) == 1, "CFE_ES_ExitApp() called");

    /*
     * Now set up individual cases for each of the error paths.
     * The first is for RADIO_AppInit().  As this is in the same
     * code unit, it is not a stub where the return code can be
     * easily set.  In order to get this to fail, an underlying
     * call needs to fail, and the error gets propagated through.
     * The call to CFE_EVS_Register is the first opportunity.
     * Any identifiable (non-success) return code should work.
     */
    UT_SetDeferredRetcode(UT_KEY(CFE_EVS_Register), 1, CFE_EVS_INVALID_PARAMETER);

    /*
     * Just call the function again.  It does not return
     * the value, so there is nothing to test for here directly.
     * However, it should show up in the coverage report that
     * the RADIO_AppInit() failure path was taken.
     */
    RADIO_AppMain();

    /*
     * This can validate that the internal "RunStatus" was
     * set to CFE_ES_RunStatus_APP_ERROR, by querying the struct directly.
     *
     * It is always advisable to include the _actual_ values
     * when asserting on conditions, so if/when it fails, the
     * log will show what the incorrect value was.
     */
    UtAssert_True(RADIO_AppData.RunStatus == CFE_ES_RunStatus_APP_ERROR,
                  "RADIO_AppData.RunStatus (%lu) == CFE_ES_RunStatus_APP_ERROR",
                  (unsigned long)RADIO_AppData.RunStatus);

    UT_SetDeferredRetcode(UT_KEY(CFE_EVS_SendEvent), 5, CFE_EVS_INVALID_PARAMETER);
    RADIO_AppMain();

    /*
     * Note that CFE_ES_RunLoop returns a boolean value,
     * so in order to exercise the internal "while" loop,
     * it needs to return TRUE.  But this also needs to return
     * FALSE in order to get out of the loop, otherwise
     * it will stay there infinitely.
     *
     * The deferred retcode will accomplish this.
     */
    UT_SetDeferredRetcode(UT_KEY(CFE_ES_RunLoop), 1, true);

    /* Set up buffer for command processing */
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    /*
     * Invoke again
     */
    RADIO_AppMain();

    /*
     * Confirm that CFE_SB_ReceiveBuffer() (inside the loop) was called
     */
    UtAssert_True(UT_GetStubCount(UT_KEY(CFE_SB_ReceiveBuffer)) == 1, "CFE_SB_ReceiveBuffer() called");

    /*
     * Now also make the CFE_SB_ReceiveBuffer call fail,
     * to exercise that error path.  This sends an
     * event which can be checked with a hook function.
     */
    UT_SetDeferredRetcode(UT_KEY(CFE_ES_RunLoop), 1, true);
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_ReceiveBuffer), 1, CFE_SB_PIPE_RD_ERR);
    UT_CheckEvent_Setup(&EventTest, RADIO_PIPE_ERR_EID, "RADIO: SB Pipe Read Error = %d");

    /*
     * Invoke again
     */
    RADIO_AppMain();

    /*
     * Confirm that the event was generated
     */
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_PIPE_ERR_EID generated (%u)", (unsigned int)EventTest.MatchCount);
}

void Test_RADIO_AppInit(void)
{
    /*
     * Test Case For:
     * int32 RADIO_AppInit( void )
     */

    /* nominal case should return CFE_SUCCESS */
    UT_TEST_FUNCTION_RC(RADIO_AppInit(), CFE_SUCCESS);

    /* trigger a failure for each of the sub-calls,
     * and confirm a write to syslog for each.
     * Note that this count accumulates, because the status
     * is _not_ reset between these test cases. */
    UT_SetDeferredRetcode(UT_KEY(CFE_EVS_Register), 1, CFE_EVS_INVALID_PARAMETER);
    UT_TEST_FUNCTION_RC(RADIO_AppInit(), CFE_EVS_INVALID_PARAMETER);
    UtAssert_True(UT_GetStubCount(UT_KEY(CFE_ES_WriteToSysLog)) == 1, "CFE_ES_WriteToSysLog() called");

    UT_SetDeferredRetcode(UT_KEY(CFE_SB_CreatePipe), 1, CFE_SB_BAD_ARGUMENT);
    UT_TEST_FUNCTION_RC(RADIO_AppInit(), CFE_SB_BAD_ARGUMENT);

    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 1, CFE_SB_BAD_ARGUMENT);
    UT_TEST_FUNCTION_RC(RADIO_AppInit(), CFE_SB_BAD_ARGUMENT);

    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 2, CFE_SB_BAD_ARGUMENT);
    UT_TEST_FUNCTION_RC(RADIO_AppInit(), CFE_SB_BAD_ARGUMENT);

    // UT_SetDeferredRetcode(UT_KEY(CFE_EVS_SendEvent), 1, CFE_SB_BAD_ARGUMENT);
    // UT_TEST_FUNCTION_RC(RADIO_AppInit(), CFE_SB_BAD_ARGUMENT);
}

void Test_RADIO_ProcessTelemetryRequest(void)
{
    CFE_SB_MsgId_t    TestMsgId;
    UT_CheckEvent_t   EventTest;
    CFE_MSG_FcnCode_t FcnCode;
    FcnCode = RADIO_REQ_DATA_TLM;

    TestMsgId = CFE_SB_ValueToMsgId(RADIO_CMD_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDeferredRetcode(UT_KEY(RADIO_RequestData), 1, OS_SUCCESS);

    UT_CheckEvent_Setup(&EventTest, RADIO_REQ_DATA_ERR_EID, NULL);
    RADIO_ProcessTelemetryRequest();
    UtAssert_True(EventTest.MatchCount == 0, "RADIO_REQ_DATA_ERR_EID generated (%u)",
                  (unsigned int)EventTest.MatchCount);

    FcnCode = 99;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    RADIO_ProcessTelemetryRequest();
    UtAssert_True(EventTest.MatchCount == 0, "RADIO_REQ_DATA_ERR_EID generated (%u)",
                  (unsigned int)EventTest.MatchCount);
}

void Test_RADIO_ProcessCommandPacket(void)
{
    /*
     * Test Case For:
     * void RADIO_ProcessCommandPacket
     */
    /* a buffer large enough for any command message */
    union
    {
        CFE_SB_Buffer_t     SBBuf;
        RADIO_NoArgs_cmd_t Noop;
    } TestMsg;
    CFE_SB_MsgId_t    TestMsgId;
    CFE_MSG_FcnCode_t FcnCode;
    size_t            MsgSize;
    UT_CheckEvent_t   EventTest;

    memset(&TestMsg, 0, sizeof(TestMsg));
    UT_CheckEvent_Setup(&EventTest, RADIO_PROCESS_CMD_ERR_EID, NULL);

    /*
     * The CFE_MSG_GetMsgId() stub uses a data buffer to hold the
     * message ID values to return.
     */
    TestMsgId = CFE_SB_ValueToMsgId(RADIO_CMD_MID);
    FcnCode   = RADIO_NOOP_CC;
    MsgSize   = sizeof(TestMsg.Noop);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    RADIO_ProcessCommandPacket();
    UtAssert_True(EventTest.MatchCount == 0, "RADIO_CMD_ERR_EID not generated (%u)",
                  (unsigned int)EventTest.MatchCount);

    TestMsgId = CFE_SB_ValueToMsgId(RADIO_REQ_HK_MID);
    FcnCode   = RADIO_REQ_HK_TLM;
    MsgSize   = sizeof(TestMsg.Noop);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    RADIO_ProcessCommandPacket();
    UtAssert_True(EventTest.MatchCount == 0, "RADIO_CMD_ERR_EID not generated (%u)",
                  (unsigned int)EventTest.MatchCount);

    /* invalid message id */
    TestMsgId = CFE_SB_INVALID_MSG_ID;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    RADIO_ProcessCommandPacket();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_CMD_ERR_EID generated (%u)", (unsigned int)EventTest.MatchCount);
}

void Test_RADIO_ProcessGroundCommand(void)
{
    /*
     * Test Case For:
     * void RADIO_ProcessGroundCommand
     */
    CFE_SB_MsgId_t    TestMsgId = CFE_SB_ValueToMsgId(RADIO_CMD_MID);
    CFE_MSG_FcnCode_t FcnCode;
    size_t            Size;

    /* a buffer large enough for any command message */
    union
    {
        CFE_SB_Buffer_t     SBBuf;
        RADIO_NoArgs_cmd_t Noop;
        RADIO_NoArgs_cmd_t Reset;
        RADIO_NoArgs_cmd_t Enable;
        RADIO_NoArgs_cmd_t Disable;
        RADIO_Config_cmd_t Config;
    } TestMsg;
    UT_CheckEvent_t EventTest;

    memset(&TestMsg, 0, sizeof(TestMsg));

    /*
     * call with each of the supported command codes
     * The CFE_MSG_GetFcnCode stub allows the code to be
     * set to whatever is needed.  There is no return
     * value here and the actual implementation of these
     * commands have separate test cases, so this just
     * needs to exercise the "switch" statement.
     */

    /* test dispatch of NOOP */
    FcnCode = RADIO_NOOP_CC;
    Size    = sizeof(TestMsg.Noop);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_CMD_NOOP_INF_EID, NULL);
    RADIO_ProcessGroundCommand();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_CMD_NOOP_INF_EID generated (%u)",
                  (unsigned int)EventTest.MatchCount);
    /* test failure of command length */
    FcnCode = RADIO_NOOP_CC;
    Size    = sizeof(TestMsg.Config);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_LEN_ERR_EID, NULL);
    RADIO_ProcessGroundCommand();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_LEN_ERR_EID generated (%u)", (unsigned int)EventTest.MatchCount);

    /* test dispatch of RESET */
    FcnCode = RADIO_RESET_COUNTERS_CC;
    Size    = sizeof(TestMsg.Reset);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_CMD_RESET_INF_EID, NULL);
    RADIO_ProcessGroundCommand();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_CMD_RESET_INF_EID generated (%u)",
                  (unsigned int)EventTest.MatchCount);
    /* test failure of command length */
    FcnCode = RADIO_RESET_COUNTERS_CC;
    Size    = sizeof(TestMsg.Config);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_LEN_ERR_EID, NULL);
    RADIO_ProcessGroundCommand();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_LEN_ERR_EID generated (%u)", (unsigned int)EventTest.MatchCount);

    /* test dispatch of ENABLE */
    FcnCode = RADIO_ENABLE_CC;
    Size    = sizeof(TestMsg.Enable);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_ENABLE_INF_EID, NULL);
    RADIO_ProcessGroundCommand();
    // UtAssert_True(EventTest.MatchCount == 1, "RADIO_ENABLE_INF_EID generated (%u)",
    //               (unsigned int)EventTest.MatchCount);

    /* test failure of command length */
    FcnCode = RADIO_ENABLE_CC;
    Size    = sizeof(TestMsg.Config);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_LEN_ERR_EID, NULL);
    RADIO_ProcessGroundCommand();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_LEN_ERR_EID generated (%u)", (unsigned int)EventTest.MatchCount);

    /* test dispatch of DISABLE */
    FcnCode = RADIO_DISABLE_CC;
    Size    = sizeof(TestMsg.Disable);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_DISABLE_INF_EID, NULL);
    RADIO_ProcessGroundCommand();
    // UtAssert_True(EventTest.MatchCount == 1, "RADIO_CMD_DISABLE_INF_EID generated (%u)",
    //               (unsigned int)EventTest.MatchCount);

    /* test failure of command length */
    FcnCode = RADIO_DISABLE_CC;
    Size    = sizeof(TestMsg.Config);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_LEN_ERR_EID, NULL);
    RADIO_ProcessGroundCommand();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_LEN_ERR_EID generated (%u)", (unsigned int)EventTest.MatchCount);

    /* test dispatch of CONFIG */
    FcnCode = RADIO_CONFIG_CC;
    Size    = sizeof(TestMsg.Config);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_CMD_CONFIG_INF_EID, NULL);
    UT_SetDeferredRetcode(UT_KEY(RADIO_CommandDevice), 1, OS_ERROR);
    CFE_MSG_Message_t msgPtr;
    RADIO_AppData.MsgPtr = &msgPtr;
    RADIO_ProcessGroundCommand();
    // UtAssert_True(EventTest.MatchCount == 1, "RADIO_CMD_CONFIG_INF_EID generated (%u)",
    //               (unsigned int)EventTest.MatchCount);

    /* test failure of command length */
    FcnCode = RADIO_CONFIG_CC;
    Size    = sizeof(TestMsg.Reset);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_LEN_ERR_EID, NULL);
    RADIO_ProcessGroundCommand();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_LEN_ERR_EID generated (%u)", (unsigned int)EventTest.MatchCount);

    FcnCode = RADIO_CONFIG_CC;
    Size    = sizeof(TestMsg.Config);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &Size, sizeof(Size), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_CMD_CONFIG_INF_EID, NULL);
    UT_SetDeferredRetcode(UT_KEY(RADIO_CommandDevice), 1, OS_SUCCESS);
    RADIO_AppData.MsgPtr = &msgPtr;
    RADIO_ProcessGroundCommand();
    // UtAssert_True(EventTest.MatchCount == 1, "RADIO_CMD_CONFIG_INF_EID generated (%u)",
    //               (unsigned int)EventTest.MatchCount);
    

    /* test an invalid CC */
    FcnCode = 99;
    Size    = sizeof(TestMsg.Noop);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &TestMsgId, sizeof(TestMsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_CMD_ERR_EID, NULL);
    RADIO_ProcessGroundCommand();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_CMD_ERR_EID generated (%u)", (unsigned int)EventTest.MatchCount);
}

void Test_RADIO_ReportHousekeeping(void)
{
    /*
     * Test Case For:
     * void RADIO_ReportHousekeeping()
     */
    CFE_MSG_Message_t *MsgSend;
    CFE_MSG_Message_t *MsgTimestamp;
    CFE_SB_MsgId_t     MsgId = CFE_SB_ValueToMsgId(RADIO_REQ_HK_TLM);

    /* Set message id to return so RADIO_Housekeeping will be called */
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    /* Set up to capture send message address */
    UT_SetDataBuffer(UT_KEY(CFE_SB_TransmitMsg), &MsgSend, sizeof(MsgSend), false);

    /* Set up to capture timestamp message address */
    UT_SetDataBuffer(UT_KEY(CFE_SB_TimeStampMsg), &MsgTimestamp, sizeof(MsgTimestamp), false);

    RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_ENABLED;

    /* Call unit under test, NULL pointer confirms command access is through APIs */
    RADIO_ReportHousekeeping();

    /* Confirm message sent*/
    UtAssert_True(UT_GetStubCount(UT_KEY(CFE_SB_TransmitMsg)) == 1, "CFE_SB_TransmitMsg() called once");
    UtAssert_True(MsgSend == &RADIO_AppData.HkTelemetryPkt.TlmHeader.Msg,
                  "CFE_SB_TransmitMsg() address matches expected");

    /* Confirm timestamp msg address */
    UtAssert_True(UT_GetStubCount(UT_KEY(CFE_SB_TimeStampMsg)) == 1, "CFE_SB_TimeStampMsg() called once");
    UtAssert_True(MsgTimestamp == &RADIO_AppData.HkTelemetryPkt.TlmHeader.Msg,
                  "CFE_SB_TimeStampMsg() address matches expected");

    UT_CheckEvent_t EventTest;
    UT_SetDeferredRetcode(UT_KEY(RADIO_RequestHK), 1, OS_ERROR);
    RADIO_ReportHousekeeping();
    UT_CheckEvent_Setup(&EventTest, RADIO_REQ_HK_ERR_EID, "RADIO: Request device HK reported error -1");
}

void Test_RADIO_VerifyCmdLength(void)
{
    /*
     * Test Case For:
     * bool RADIO_VerifyCmdLength
     */
    UT_CheckEvent_t   EventTest;
    size_t            size    = 1;
    CFE_MSG_FcnCode_t fcncode = 2;
    CFE_SB_MsgId_t    msgid   = CFE_SB_ValueToMsgId(RADIO_CMD_MID);

    /*
     * test a match case
     */
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &size, sizeof(size), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_LEN_ERR_EID, NULL);

    RADIO_VerifyCmdLength(NULL, size);

    /*
     * Confirm that the event was NOT generated
     */
    UtAssert_True(EventTest.MatchCount == 0, "RADIO_LEN_ERR_EID NOT generated (%u)",
                  (unsigned int)EventTest.MatchCount);

    /*
     * test a mismatch case
     */
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &size, sizeof(size), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &msgid, sizeof(msgid), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &fcncode, sizeof(fcncode), false);
    UT_CheckEvent_Setup(&EventTest, RADIO_LEN_ERR_EID, NULL);
    RADIO_VerifyCmdLength(NULL, size + 1);

    /*
     * Confirm that the event WAS generated
     */
    UtAssert_True(EventTest.MatchCount == 1, "RADIO_LEN_ERR_EID generated (%u)", (unsigned int)EventTest.MatchCount);
}

void Test_RADIO_ReportDeviceTelemetry(void)
{
    RADIO_ReportDeviceTelemetry();

    UT_SetDeferredRetcode(UT_KEY(RADIO_RequestData), 1, OS_SUCCESS);
    RADIO_ReportDeviceTelemetry();

    UT_SetDeferredRetcode(UT_KEY(RADIO_RequestData), 1, OS_ERROR);
    RADIO_ReportDeviceTelemetry();

    RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_DISABLED;
    RADIO_ReportDeviceTelemetry();

    RADIO_AppData.HkTelemetryPkt.DeviceEnabled         = RADIO_DEVICE_ENABLED;
    RADIO_ReportDeviceTelemetry();
}

void Test_RADIO_Configure(void)
{
    RADIO_Configure();

    RADIO_Config_cmd_t command;
    RADIO_AppData.MsgPtr                                     = (CFE_MSG_Message_t *)&command;
    ((RADIO_Config_cmd_t *)RADIO_AppData.MsgPtr)->DeviceCfg = 0xFFFF;
    RADIO_Configure();

    ((RADIO_Config_cmd_t *)RADIO_AppData.MsgPtr)->DeviceCfg = 0x0;
    RADIO_AppData.HkTelemetryPkt.DeviceEnabled               = RADIO_DEVICE_ENABLED;
    RADIO_Configure();

    UT_SetDeferredRetcode(UT_KEY(RADIO_CommandDevice), 1, OS_ERROR);
    RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_ENABLED;
    RADIO_Configure();
}

void Test_RADIO_Enable(void)
{
    UT_CheckEvent_t EventTest;

    UT_CheckEvent_Setup(&EventTest, RADIO_ENABLE_INF_EID, NULL);
    RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_DISABLED;
    UT_SetDeferredRetcode(UT_KEY(uart_init_port), 1, OS_SUCCESS);
    RADIO_Enable();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO: Device enabled (%u)", (unsigned int)EventTest.MatchCount);

    UT_CheckEvent_Setup(&EventTest, RADIO_UART_INIT_ERR_EID, NULL);
    RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_DISABLED;
    UT_SetDeferredRetcode(UT_KEY(uart_init_port), 1, OS_ERROR);
    RADIO_Enable();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO: UART port initialization error (%u)",
                  (unsigned int)EventTest.MatchCount);

    UT_CheckEvent_Setup(&EventTest, RADIO_ENABLE_ERR_EID, NULL);
    RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_ENABLED;
    UT_SetDeferredRetcode(UT_KEY(uart_init_port), 1, OS_ERROR);
    RADIO_Enable();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO: Device enable failed, already enabled (%u)",
                  (unsigned int)EventTest.MatchCount);
}

void Test_RADIO_Disable(void)
{
    UT_CheckEvent_t EventTest;

    UT_CheckEvent_Setup(&EventTest, RADIO_DISABLE_INF_EID, NULL);
    RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_ENABLED;
    UT_SetDeferredRetcode(UT_KEY(uart_close_port), 1, OS_SUCCESS);
    RADIO_Disable();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO: Device disabled (%u)", (unsigned int)EventTest.MatchCount);

    UT_CheckEvent_Setup(&EventTest, RADIO_UART_CLOSE_ERR_EID, NULL);
    RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_ENABLED;
    UT_SetDeferredRetcode(UT_KEY(uart_close_port), 1, OS_ERROR);
    RADIO_Disable();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO: UART port close error (%u)", (unsigned int)EventTest.MatchCount);

    UT_CheckEvent_Setup(&EventTest, RADIO_DISABLE_ERR_EID, NULL);
    RADIO_AppData.HkTelemetryPkt.DeviceEnabled = RADIO_DEVICE_DISABLED;
    UT_SetDeferredRetcode(UT_KEY(uart_close_port), 1, OS_ERROR);
    RADIO_Disable();
    UtAssert_True(EventTest.MatchCount == 1, "RADIO: Device disable failed, already disabled (%u)",
                  (unsigned int)EventTest.MatchCount);
}

/*
 * Setup function prior to every test
 */
void Radio_UT_Setup(void)
{
    UT_ResetState(0);
}

/*
 * Teardown function after every test
 */
void Radio_UT_TearDown(void) {}

/*
 * Register the test cases to execute with the unit test tool
 */
void UtTest_Setup(void)
{
    ADD_TEST(RADIO_AppMain);
    ADD_TEST(RADIO_AppInit);
    ADD_TEST(RADIO_ProcessCommandPacket);
    ADD_TEST(RADIO_ProcessGroundCommand);
    ADD_TEST(RADIO_ReportHousekeeping);
    ADD_TEST(RADIO_VerifyCmdLength);
    ADD_TEST(RADIO_ReportDeviceTelemetry);
    ADD_TEST(RADIO_ProcessTelemetryRequest);
    ADD_TEST(RADIO_Configure);
    ADD_TEST(RADIO_Enable);
    ADD_TEST(RADIO_Disable);
}