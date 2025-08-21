#include "radio_device.h"

/*
** Generic initialize radio device (SPI + GPIO)
*/
int32_t RADIO_InitDevice(spi_info_t *spi_device, gpio_info_t *power_gpio, gpio_info_t *interrupt_gpio)
{
    int32_t status = OS_SUCCESS;
    
    /* Initialize SPI device */
    if (spi_device != NULL)
    {
        status = spi_init_dev(spi_device);
        if (status != SPI_SUCCESS)
        {
            OS_printf("RADIO_InitDevice: SPI initialization failed with error %d\n", status);
            return OS_ERROR;
        }
    }
    
    /* Initialize power GPIO */
    if (power_gpio != NULL)
    {
        status = gpio_init(power_gpio);
        if (status != GPIO_SUCCESS)
        {
            OS_printf("RADIO_InitDevice: Power GPIO initialization failed with error %d\n", status);
            return OS_ERROR;
        }
    }
    
    /* Initialize interrupt GPIO */
    if (interrupt_gpio != NULL)
    {
        status = gpio_init(interrupt_gpio);
        if (status != GPIO_SUCCESS)
        {
            OS_printf("RADIO_InitDevice: Interrupt GPIO initialization failed with error %d\n", status);
            return OS_ERROR;
        }
    }
    
    return OS_SUCCESS;
}

/*
** Power on radio device
*/
int32_t RADIO_PowerOn(gpio_info_t *power_gpio)
{
    if (power_gpio == NULL)
    {
        return OS_ERROR;
    }
    
    int32_t status = gpio_write(power_gpio, 1); /* Active high */
    if (status != GPIO_SUCCESS)
    {
        OS_printf("RADIO_PowerOn: Failed to set power GPIO high\n");
        return OS_ERROR;
    }
    
    /* Small delay for power stabilization */
    OS_TaskDelay(10);
    
    return OS_SUCCESS;
}

/*
** Power off radio device
*/
int32_t RADIO_PowerOff(gpio_info_t *power_gpio)
{
    if (power_gpio == NULL)
    {
        return OS_ERROR;
    }
    
    int32_t status = gpio_write(power_gpio, 0); /* Active high, so 0 = off */
    if (status != GPIO_SUCCESS)
    {
        OS_printf("RADIO_PowerOff: Failed to set power GPIO low\n");
        return OS_ERROR;
    }
    
    return OS_SUCCESS;
}

/*
** Check interrupt status
*/
int32_t RADIO_CheckInterrupt(gpio_info_t *interrupt_gpio, uint8_t *interrupt_status)
{
    if (interrupt_gpio == NULL || interrupt_status == NULL)
    {
        return OS_ERROR;
    }
    
    int32_t status = gpio_read(interrupt_gpio, interrupt_status);
    if (status != GPIO_SUCCESS)
    {
        OS_printf("RADIO_CheckInterrupt: Failed to read interrupt GPIO\n");
        return OS_ERROR;
    }
    
    return OS_SUCCESS;
}

/*
** Generic command to device via SPI
*/
int32_t RADIO_CommandDevice(spi_info_t *device, uint8_t cmd, uint8_t payload_len, uint8_t *payload)
{
    int32_t status = OS_SUCCESS;
    uint8_t tx_buffer[RADIO_MAX_PAYLOAD_SIZE + 5]; /* Header + cmd + len + payload + trailer */
    uint8_t rx_buffer[RADIO_MAX_PAYLOAD_SIZE + 5];
    uint32_t total_len;
    
    if (device == NULL)
    {
        return OS_ERROR;
    }
    
    /* Build command packet */
    tx_buffer[0] = RADIO_DEVICE_HDR;          /* Header byte */
    tx_buffer[1] = cmd;                       /* Command */
    tx_buffer[2] = payload_len;               /* Payload length */
    
    /* Copy payload if provided */
    if (payload_len > 0 && payload != NULL)
    {
        memcpy(&tx_buffer[3], payload, payload_len);
    }
    
    /* Add trailer */
    tx_buffer[3 + payload_len] = RADIO_DEVICE_TRAILER;  /* Trailer byte */
    
    total_len = 4 + payload_len; /* Header(1) + cmd(1) + len(1) + payload + trailer(1) */
    
    #ifdef RADIO_CFG_DEBUG
        OS_printf("RADIO_CommandDevice[%d] = ", total_len);
        for (uint32_t i = 0; i < total_len; i++)
        {
            OS_printf("%02x", tx_buffer[i]);
        }
        OS_printf("\n");
    #endif
    
    /* Perform SPI transaction */
    status = spi_transaction(device, tx_buffer, rx_buffer, total_len, 0, 8, 1);
    if (status != SPI_SUCCESS)
    {
        OS_printf("RADIO_CommandDevice: SPI transaction failed with error %d\n", status);
        return OS_ERROR;
    }
    
    return OS_SUCCESS;
}

/*
** Request housekeeping command
*/
int32_t RADIO_RequestHK(spi_info_t *device, RADIO_Device_HK_tlm_t *data)
{
    int32_t status = OS_SUCCESS;
    uint8_t rx_buffer[RADIO_DEVICE_HK_SIZE];
    uint8_t tx_buffer[6]; /* Header + cmd + len + trailer */
    
    if (device == NULL || data == NULL)
    {
        return OS_ERROR;
    }
    
    /* Send HK request command */
    status = RADIO_CommandDevice(device, RADIO_DEVICE_REQ_HK_CMD, 0, NULL);
    if (status != OS_SUCCESS)
    {
        OS_printf("RADIO_RequestHK: Command failed with error %d\n", status);
        return status;
    }
    
    /* Wait briefly for response */
    OS_TaskDelay(10);
    
    /* Read HK response */
    memset(tx_buffer, 0, sizeof(tx_buffer));
    status = spi_transaction(device, tx_buffer, rx_buffer, RADIO_DEVICE_HK_SIZE, 0, 8, 1);
    if (status != SPI_SUCCESS)
    {
        OS_printf("RADIO_RequestHK: SPI read failed with error %d\n", status);
        return OS_ERROR;
    }
    
    #ifdef RADIO_CFG_DEBUG
        OS_printf("RADIO_RequestHK response = ");
        for (uint32_t i = 0; i < RADIO_DEVICE_HK_SIZE; i++)
        {
            OS_printf("%02x", rx_buffer[i]);
        }
        OS_printf("\n");
    #endif
    
    /* Verify response header and trailer */
    if ((rx_buffer[0] != RADIO_DEVICE_HDR) ||
        (rx_buffer[RADIO_DEVICE_HK_SIZE-1] != RADIO_DEVICE_TRAILER))
    {
        OS_printf("RADIO_RequestHK: Invalid response header/trailer\n");
        return OS_ERROR;
    }
    
    /* Parse housekeeping data */
    data->CommandCounter = (rx_buffer[1] << 8) | rx_buffer[2];
    data->Mode = rx_buffer[3];
    data->GroundLock = rx_buffer[4];
    data->RxSpeedSetting = rx_buffer[5];
    data->RxWavelengthSetting = rx_buffer[6];
    data->TxSpeedSetting = rx_buffer[7];
    data->TxWavelengthSetting = rx_buffer[8];
    data->BytesInRxBuffer = (rx_buffer[9] << 24) | (rx_buffer[10] << 16) | (rx_buffer[11] << 8) | rx_buffer[12];
    data->BytesReceived = (rx_buffer[13] << 24) | (rx_buffer[14] << 16) | (rx_buffer[15] << 8) | rx_buffer[16];
    data->BytesSent = (rx_buffer[17] << 24) | (rx_buffer[18] << 16) | (rx_buffer[19] << 8) | rx_buffer[20];
    
    #ifdef RADIO_CFG_DEBUG
        OS_printf("  CommandCounter    = %d\n", data->CommandCounter);
        OS_printf("  Mode              = %d\n", data->Mode);
        OS_printf("  GroundLock        = %d\n", data->GroundLock);
        OS_printf("  BytesInRxBuffer   = %d\n", data->BytesInRxBuffer);
        OS_printf("  BytesReceived     = %d\n", data->BytesReceived);
        OS_printf("  BytesSent         = %d\n", data->BytesSent);
    #endif
    
    return OS_SUCCESS;
}

/*
** Set configuration command
*/
int32_t RADIO_SetConfiguration(spi_info_t *device, RADIO_Device_Config_t *config)
{
    uint8_t payload[RADIO_CFG_PAYLOAD_SIZE];
    
    if (device == NULL || config == NULL)
    {
        return OS_ERROR;
    }
    
    /* Build configuration payload */
    payload[0] = config->Mode;
    payload[1] = config->RxSpeedSetting;
    payload[2] = config->RxWavelengthSetting;
    payload[3] = config->TxSpeedSetting;
    payload[4] = config->TxWavelengthSetting;
    
    /* Send configuration command */
    return RADIO_CommandDevice(device, RADIO_DEVICE_SET_CFG_CMD, RADIO_CFG_PAYLOAD_SIZE, payload);
}

/*
** Send data command
*/
int32_t RADIO_SendData(spi_info_t *device, uint8_t *data, uint8_t data_length)
{
    if (device == NULL || data == NULL)
    {
        return OS_ERROR;
    }
    
    /* Send data command with data as payload */
    return RADIO_CommandDevice(device, RADIO_DEVICE_SEND_CMD, data_length, data);
}

/*
** Receive data command
*/
int32_t RADIO_ReceiveData(spi_info_t *device, uint8_t *data, uint8_t max_length, uint8_t *actual_length)
{
    int32_t status = OS_SUCCESS;
    uint8_t payload[RADIO_RECEIVE_PAYLOAD_SIZE];
    uint8_t rx_buffer[RADIO_MAX_PAYLOAD_SIZE + 6]; /* Max response size */
    uint8_t tx_buffer[6];
    uint8_t response_len;
    
    if (device == NULL || data == NULL || actual_length == NULL)
    {
        return OS_ERROR;
    }
    
    /* Build receive request payload (number of bytes to receive) */
    payload[0] = max_length;
    
    /* Send receive command */
    status = RADIO_CommandDevice(device, RADIO_DEVICE_RECEIVE_CMD, RADIO_RECEIVE_PAYLOAD_SIZE, payload);
    if (status != OS_SUCCESS)
    {
        return status;
    }
    
    /* Wait briefly for response */
    OS_TaskDelay(10);
    
    /* Read response - first read to get header and length */
    memset(tx_buffer, 0, sizeof(tx_buffer));
    status = spi_transaction(device, tx_buffer, rx_buffer, 6, 0, 8, 1); /* Read header + cmd + len + 2 bytes data + trailer start */
    if (status != SPI_SUCCESS)
    {
        OS_printf("RADIO_ReceiveData: SPI read failed with error %d\n", status);
        return OS_ERROR;
    }
    
    /* Verify header */
    if (rx_buffer[0] != RADIO_DEVICE_HDR)
    {
        OS_printf("RADIO_ReceiveData: Invalid response header\n");
        return OS_ERROR;
    }
    
    /* Get payload length from response */
    response_len = rx_buffer[2];
    if (response_len > max_length)
    {
        OS_printf("RADIO_ReceiveData: Response too large (%d > %d)\n", response_len, max_length);
        return OS_ERROR;
    }
    
    /* Read remaining data if needed */
    if (response_len > 1) /* Already read 1 byte of data */
    {
        memset(tx_buffer, 0, sizeof(tx_buffer));
        status = spi_transaction(device, tx_buffer, &rx_buffer[4], response_len - 1 + 1, 0, 8, 1); /* Remaining data + trailer */
        if (status != SPI_SUCCESS)
        {
            OS_printf("RADIO_ReceiveData: SPI read remaining failed with error %d\n", status);
            return OS_ERROR;
        }
    }
    
    /* Copy received data */
    memcpy(data, &rx_buffer[3], response_len);
    *actual_length = response_len;
    
    return OS_SUCCESS;
}
