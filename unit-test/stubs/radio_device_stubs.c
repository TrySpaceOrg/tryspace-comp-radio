#include "utgenstub.h"
#include "radio_device.h"

int32_t RADIO_ReadData(uart_info_t *device, uint8_t *read_data, uint8_t data_length)
{
    UT_GenStub_SetupReturnBuffer(RADIO_ReadData, int32_t);

    UT_GenStub_AddParam(RADIO_ReadData, uart_info_t *, device);
    UT_GenStub_AddParam(RADIO_ReadData, uint8_t *, read_data);
    UT_GenStub_AddParam(RADIO_ReadData, uint8_t, data_length);

    UT_GenStub_Execute(RADIO_ReadData, Basic, NULL);

    return UT_GenStub_GetReturnValue(RADIO_ReadData, int32_t);
}

int32_t RADIO_CommandDevice(uart_info_t *device, uint16_t cmd, uint16_t payload)
{
    UT_GenStub_SetupReturnBuffer(RADIO_CommandDevice, int32_t);

    UT_GenStub_AddParam(RADIO_CommandDevice, uart_info_t *, device);
    UT_GenStub_AddParam(RADIO_CommandDevice, uint8_t, cmd);
    UT_GenStub_AddParam(RADIO_CommandDevice, uint32_t, payload);

    UT_GenStub_Execute(RADIO_CommandDevice, Basic, NULL);

    return UT_GenStub_GetReturnValue(RADIO_CommandDevice, int32_t);
}

int32_t RADIO_RequestHK(uart_info_t *device, RADIO_Device_HK_tlm_t *data)
{
    UT_GenStub_SetupReturnBuffer(RADIO_RequestHK, int32_t);

    UT_GenStub_AddParam(RADIO_RequestHK, uart_info_t *, device);
    UT_GenStub_AddParam(RADIO_RequestHK, RADIO_Device_HK_tlm_t *, data);

    UT_GenStub_Execute(RADIO_RequestHK, Basic, NULL);

    return UT_GenStub_GetReturnValue(RADIO_RequestHK, int32_t);
}

int32_t RADIO_RequestData(uart_info_t *device, RADIO_Device_Data_tlm_t *data)
{
    UT_GenStub_SetupReturnBuffer(RADIO_RequestData, int32_t);

    UT_GenStub_AddParam(RADIO_RequestData, uart_info_t *, device);
    UT_GenStub_AddParam(RADIO_RequestData, RADIO_Device_Data_tlm_t *, data);

    UT_GenStub_Execute(RADIO_RequestData, Basic, NULL);

    return UT_GenStub_GetReturnValue(RADIO_RequestData, int32_t);
}
