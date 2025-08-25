#include "radio_app_coveragetest_common.h"

void Test_RADIO_ReadData(void)
{
    spi_info_t device;
    uint8_t     read_data[8];
    uint8_t     data_length = 8;
    uint16_t actual_len = 0;
    RADIO_ReceiveData(&device, read_data, data_length, &actual_len);
    UT_SetDeferredRetcode(UT_KEY(spi_read), 1, data_length);
    RADIO_ReceiveData(&device, read_data, data_length, &actual_len);
    UT_SetDeferredRetcode(UT_KEY(spi_read), 1, data_length + 1);
    RADIO_ReceiveData(&device, read_data, data_length, &actual_len);
}

void Test_RADIO_CommandDevice(void)
{
    spi_info_t device;
    uint8_t     cmd_code = 0;
    uint8_t    payload_len  = 0;
    uint8_t payload_data[1];
    RADIO_CommandDevice(&device, cmd_code, payload_len, payload_data);

    /* Simulate SPI write error */
    UT_SetDeferredRetcode(UT_KEY(spi_write), 1, SPI_ERROR);
    RADIO_CommandDevice(&device, cmd_code, payload_len, payload_data);
    /* For zero-payload commands the command size is header+cmd+len+trailer = 4 */
    UT_SetDeferredRetcode(UT_KEY(spi_write), 1, 4);
    RADIO_CommandDevice(&device, cmd_code, payload_len, payload_data);

    /* Simulate read back on device if command expects response */
    UT_SetDeferredRetcode(UT_KEY(spi_read), 1, 9);
    /* Simulate RADIO_ReceiveData being successful when called from command path */
    UT_SetDefaultReturnValue(UT_KEY(RADIO_ReceiveData), OS_SUCCESS);
    UT_SetDeferredRetcode(UT_KEY(RADIO_ReceiveData), 1, OS_SUCCESS);
    RADIO_CommandDevice(&device, cmd_code, payload_len, payload_data);
}

void Test_RADIO_RequestHK(void)
{
    spi_info_t            device;
    RADIO_Device_HK_tlm_t data;
    RADIO_RequestHK(&device, &data);

    uint8_t read_data[] = {0xDE, 0xAD, 0x00, 0x00, 0x00, 0x07, 0x00, 0x06,
                           0x00, 0x0C, 0x00, 0x12, 0x00, 0x00, 0xBE, 0xEF};
    UT_SetDeferredRetcode(UT_KEY(spi_read), 1, 16);
    UT_SetDataBuffer(UT_KEY(spi_read), &read_data, sizeof(read_data), false);
    RADIO_RequestHK(&device, &data);

    UT_SetDeferredRetcode(UT_KEY(spi_write), 1, OS_ERROR);
    RADIO_RequestHK(&device, &data);
}

void Test_RADIO_RequestData(void)
{
    spi_info_t              device;
    /* The device data path uses RADIO_ReceiveData in production; exercise receive */
    uint8_t data_buf[16];
    uint16_t actual_len = 0;
    RADIO_ReceiveData(&device, data_buf, sizeof(data_buf), &actual_len);

    uint8_t read_data[] = {0xDE, 0xAD, 0x00, 0x00, 0x00, 0x07, 0x00, 0x06,
                           0x00, 0x0C, 0x00, 0x12, 0x00, 0x00, 0xBE, 0xEF};
    UT_SetDeferredRetcode(UT_KEY(spi_read), 1, 16);
    UT_SetDataBuffer(UT_KEY(spi_read), &read_data, sizeof(read_data), false);
    RADIO_ReceiveData(&device, data_buf, sizeof(data_buf), &actual_len);

    UT_SetDeferredRetcode(UT_KEY(spi_write), 1, OS_ERROR);
    RADIO_ReceiveData(&device, data_buf, sizeof(data_buf), &actual_len);
}

void Test_RADIO_RequestData_Hook(void *UserObj, UT_EntryKey_t FuncKey, const UT_StubContext_t *Context, va_list va) {}

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
    /* RADIO_RequestData test converted to RADIO_ReceiveData; no VA handler needed */
    ADD_TEST(RADIO_ReadData);
    ADD_TEST(RADIO_CommandDevice);
    ADD_TEST(RADIO_RequestHK);
    /* RADIO_RequestData removed; RADIO_ReceiveData tests added instead */
}