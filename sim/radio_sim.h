#ifndef RADIO_SIM_H
#define RADIO_SIM_H

//#include <stdint.h>

#include "radio_device.h"
#include "simulith.h"
#include "simulith_42_context.h"
#include "simulith_42_commands.h"

// Configuration parameters
#define RADIO_SIM_UART_ID 5
#define RADIO_SIM_UPDATE_RATE_HZ 10

// Status codes
#define RADIO_SIM_SUCCESS 0
#define RADIO_SIM_ERROR  1

// Radio simulator state
typedef struct 
{
    // Communication handles
    uint8_t uart_port;
    uint32_t uart_handle;
    void* time_handle;
    // Simulator specifics
    double last_update_time;
    // Device specifics
    RADIO_Device_HK_tlm_t hk;
    RADIO_Device_Data_tlm_t data;
} radio_sim_state_t;

// Function declarations
static void send_housekeeping(radio_sim_state_t* state);
static void send_radio_data(radio_sim_state_t* state);
static void handle_command(radio_sim_state_t* state, const uint8_t* data, size_t length);
static void radio_sim_on_tick(uint64_t tick_time_ns, const simulith_42_context_t* context_42);
int radio_sim_init(radio_sim_state_t* state);
void radio_sim_cleanup(radio_sim_state_t* state);

#endif /* RADIO_SIM_H */ 