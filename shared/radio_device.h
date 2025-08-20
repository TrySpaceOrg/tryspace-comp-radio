#ifndef _RADIO_DEVICE_H_
#define _RADIO_DEVICE_H_

/*
** Required header files.
*/
#include "device_cfg.h"
#include "hwlib.h"

/*
** Type definitions
*/
#define RADIO_DEVICE_HDR_0 0xC0
#define RADIO_DEVICE_HDR_1 0xFF
#define RADIO_DEVICE_HDR   ((RADIO_DEVICE_HDR_0 << 8) | RADIO_DEVICE_HDR_1)

#define RADIO_DEVICE_NOOP_CMD     0x00
#define RADIO_DEVICE_REQ_HK_CMD   0x01
#define RADIO_DEVICE_REQ_DATA_CMD 0x02
#define RADIO_DEVICE_CFG_CMD      0x03

#define RADIO_DEVICE_TRAILER_0 0xFE
#define RADIO_DEVICE_TRAILER_1 0xFE
#define RADIO_DEVICE_TRAILER   ((RADIO_DEVICE_TRAILER_0 << 8) | RADIO_DEVICE_TRAILER_1)

#define RADIO_DEVICE_CMD_SIZE    8
#define RADIO_DEVICE_HDR_TRL_LEN 4

/*
** RADIO device housekeeping telemetry definition
*/
typedef struct
{
    uint16_t DeviceCounter;
    uint16_t DeviceConfig;

} __attribute__((packed)) RADIO_Device_HK_tlm_t;
#define RADIO_DEVICE_HK_LNGTH sizeof(RADIO_Device_HK_tlm_t)
#define RADIO_DEVICE_HK_SIZE  RADIO_DEVICE_HK_LNGTH + RADIO_DEVICE_HDR_TRL_LEN

/*
** RADIO device data telemetry definition
*/
typedef struct
{
    uint16_t Chan1;
    uint16_t Chan2;
    uint16_t Chan3;

} __attribute__((packed)) RADIO_Device_Data_tlm_t;
#define RADIO_DEVICE_DATA_LNGTH sizeof(RADIO_Device_Data_tlm_t)
#define RADIO_DEVICE_DATA_SIZE  RADIO_DEVICE_DATA_LNGTH + RADIO_DEVICE_HDR_TRL_LEN

/*
** Prototypes
*/
int32_t RADIO_ReadData(uart_info_t *device, uint8_t *read_data, uint8_t data_length);
int32_t RADIO_CommandDevice(uart_info_t *device, uint16_t cmd, uint16_t payload);
int32_t RADIO_RequestHK(uart_info_t *device, RADIO_Device_HK_tlm_t *data);
int32_t RADIO_RequestData(uart_info_t *device, RADIO_Device_Data_tlm_t *data);

#endif /* _RADIO_DEVICE_H_ */
