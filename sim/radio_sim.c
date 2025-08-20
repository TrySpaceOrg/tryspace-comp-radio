#include "radio_sim.h"
#include "simulith_uart.h"
#include <math.h>

#ifndef STANDALONE_MODE
#include "simulith_component.h"
#endif

// Global state pointer for callback access
static radio_sim_state_t* g_state = NULL;

// UART port struct for Simulith
static uart_port_t g_uart_port = {0};

static void send_housekeeping(radio_sim_state_t* state)
{
    if (!state) return;
    uint8_t response[8];
    response[0] = RADIO_DEVICE_HDR_0;
    response[1] = RADIO_DEVICE_HDR_1;
    response[2] = (state->hk.DeviceCounter >> 8) & 0xFF;
    response[3] = state->hk.DeviceCounter & 0xFF;
    response[4] = (state->hk.DeviceConfig >> 8) & 0xFF;
    response[5] = state->hk.DeviceConfig & 0xFF;
    response[6] = RADIO_DEVICE_TRAILER_0;
    response[7] = RADIO_DEVICE_TRAILER_1;
    simulith_uart_send(&g_uart_port, response, sizeof(response));
}

static void send_radio_data(radio_sim_state_t* state)
{
    if (!state) return;
    uint8_t response[10];
    response[0] = RADIO_DEVICE_HDR_0;
    response[1] = RADIO_DEVICE_HDR_1;
    response[2] = (state->data.Chan1 >> 8) & 0xFF;
    response[3] = state->data.Chan1 & 0xFF;
    response[4] = (state->data.Chan2 >> 8) & 0xFF;
    response[5] = state->data.Chan2 & 0xFF;
    response[6] = (state->data.Chan3 >> 8) & 0xFF;
    response[7] = state->data.Chan3 & 0xFF;
    response[8] = RADIO_DEVICE_TRAILER_0;
    response[9] = RADIO_DEVICE_TRAILER_1;
    simulith_uart_send(&g_uart_port, response, sizeof(response));
}

static void handle_command(radio_sim_state_t* state, const uint8_t* data, size_t length)
{
    if (!state || !data || length < RADIO_DEVICE_CMD_SIZE) 
    {  // Check for minimum command size
        printf("Invalid command parameters: state=%p, data=%p, length=%zu\n", 
               (void*)state, (const void*)data, length);
        return;
    }
    
    uint16_t header  = ((uint16_t) data[0] << 8) | data[1];
    uint16_t cmd_id  = ((uint16_t) data[2] << 8) | data[3];
    uint16_t payload = ((uint16_t) data[4] << 8) | data[5];
    uint16_t trailer = ((uint16_t) data[6] << 8) | data[7];

    // Validate header
    if (header != RADIO_DEVICE_HDR) 
    {
        printf("Invalid command header (0x%04X)\n", header);
        return;
    }

    // Validate trailer
    if (trailer != RADIO_DEVICE_TRAILER) 
    {
        printf("Invalid command trailer (0x%04X)\n", trailer);
        return;
    }

    // Echo command back
    printf("handle_command: Echo command back to UART: ID=%d, Payload=0x%08X\n", cmd_id, payload);
    simulith_uart_send(&g_uart_port, data, length);

    // Process command
    switch (cmd_id) 
    {
        case RADIO_DEVICE_NOOP_CMD:
            printf("Processing NOOP command\n");
            // Just echo the command back, which was already done
            break;

        case RADIO_DEVICE_REQ_HK_CMD:
            printf("Processing GET_HK command\n");
            send_housekeeping(state);
            break;

        case RADIO_DEVICE_REQ_DATA_CMD:
            printf("Processing GET_DATA command\n");
            send_radio_data(state);
            break;

        case RADIO_DEVICE_CFG_CMD:
            printf("Processing SET_CONFIG command with payload 0x%08X\n", payload);
            state->hk.DeviceConfig = payload;
            break;

        default:
            printf("Unknown command ID: %d\n", cmd_id);
            break;
    }

    // Increment command counter
    state->hk.DeviceCounter++;
}

static void radio_sim_on_tick(uint64_t tick_time_ns, const simulith_42_context_t* context_42)
{
    int bytes;
    uint8_t data[256];

    if (!g_state) return;
    
    // Convert nanoseconds to seconds
    double current_time = tick_time_ns / 1e9;
    
    // Update radio data at the specified rate
    if (current_time - g_state->last_update_time >= (1.0 / RADIO_SIM_UPDATE_RATE_HZ)) 
    {
        // If 42 context is available, populate channels with Sun Vector Body (SVB)
        if (context_42 && context_42->valid) {
            // Chan1: SVB X-component (scaled and offset for uint16)
            // Scale by 10000 and add 32768 offset to handle negative values
            g_state->data.Chan1 = (uint16_t)((context_42->sun_vector_body[0] * 10000.0) + 32768.0);
            
            // Chan2: SVB Y-component (scaled and offset for uint16)
            g_state->data.Chan2 = (uint16_t)((context_42->sun_vector_body[1] * 10000.0) + 32768.0);
            
            // Chan3: SVB Z-component (scaled and offset for uint16)
            g_state->data.Chan3 = (uint16_t)((context_42->sun_vector_body[2] * 10000.0) + 32768.0);
            
            // Optional: Print SVB data for debugging
            //if (g_state->hk.DeviceCounter % 1000 == 0) { // Print every 1000 cycles
            //    printf("42 SVB - Time: %.3f, Sun Vector Body: [%.6f, %.6f, %.6f], Channels: [%u, %u, %u]\n",
            //           context_42->sim_time, 
            //           context_42->sun_vector_body[0], context_42->sun_vector_body[1], context_42->sun_vector_body[2],
            //           g_state->data.Chan1, g_state->data.Chan2, g_state->data.Chan3);
            //}
        } else {
            // Fallback: Use command counter if no 42 context available
            g_state->data.Chan1 = (uint16_t)(g_state->hk.DeviceCounter * 1);
            g_state->data.Chan2 = (uint16_t)(g_state->hk.DeviceCounter * 2);
            g_state->data.Chan3 = (uint16_t)(g_state->hk.DeviceCounter * 3);
        }
        
        g_state->last_update_time = current_time;
    }

    // Process UART
    bytes = simulith_uart_available(&g_uart_port);
    if (bytes > 0)
    {
        // Read UART
        bytes = simulith_uart_receive(&g_uart_port, data, sizeof(data));

        printf("Received %d bytes from UART\n", bytes);
        for(int i = 0; i < bytes; i++) 
        {
            printf("%02X ", data[i]);
        }
        printf("\n");

        // Process the command
        handle_command(g_state, data, bytes);
    }
}

int radio_sim_init(radio_sim_state_t* state)
{
    if (!state) return RADIO_SIM_ERROR;

    // Initialize state
    memset(state, 0, sizeof(radio_sim_state_t));

    // Set global state pointer
    g_state = state;

#ifdef STANDALONE_MODE
    // In standalone mode, we initialize our own Simulith client
    // Wait a second for the Simulith server to start up
    sleep(1);

    // Initialize Simulith client
    if (simulith_client_init(LOCAL_PUB_ADDR, LOCAL_REP_ADDR, "tryspace-comp-radio-sim", INTERVAL_NS) != 0) 
    {
        printf("Failed to initialize Simulith client\n");
        return RADIO_SIM_ERROR;
    }

    // Handshake with Simulith server
    if (simulith_client_handshake() != 0) 
    {
        printf("Failed to handshake with Simulith server\n");
        simulith_client_shutdown();
        return RADIO_SIM_ERROR;
    }
#endif

    // Initialize UART port struct for Simulith (server/bind)
    memset(&g_uart_port, 0, sizeof(g_uart_port));
    snprintf(g_uart_port.name, sizeof(g_uart_port.name), "radio_sim_uart");
    snprintf(g_uart_port.address, sizeof(g_uart_port.address), "tcp://*:%d", SIMULITH_UART_BASE_PORT + RADIO_CFG_HANDLE);
    g_uart_port.is_server = 1; // Always server/bind for the simulator

    int uart_result = simulith_uart_init(&g_uart_port);
    if (uart_result < 0) 
    {
        printf("Failed to initialize Simulith UART server\n");
#ifdef STANDALONE_MODE
        simulith_client_shutdown();
#endif
        return RADIO_SIM_ERROR;
    }

    // Initialize time provider
    state->time_handle = simulith_time_init();
    if (!state->time_handle) 
    {
        printf("Failed to initialize time provider\n");
        simulith_uart_close(&g_uart_port);
#ifdef STANDALONE_MODE
        simulith_client_shutdown();
#endif
        return RADIO_SIM_ERROR;
    }

    // Initialize default values
    state->hk.DeviceCounter = 0;
    state->hk.DeviceConfig = 0;
    state->data.Chan1 = 0;
    state->data.Chan2 = 0;
    state->data.Chan3 = 0;
    state->last_update_time = simulith_time_get(state->time_handle);

    printf("Radio simulator initialized successfully as UART server on %s\n", g_uart_port.address);
    printf("Waiting for commands...\n");
    return RADIO_SIM_SUCCESS;
}

void radio_sim_cleanup(radio_sim_state_t* state)
{
    if (!state) return;

    g_state = NULL;  // Clear global state pointer
    simulith_uart_close(&g_uart_port);

    if (state->time_handle) 
    {
        simulith_time_cleanup(state->time_handle);
        state->time_handle = NULL;
    }
    
#ifdef STANDALONE_MODE
    simulith_client_shutdown();
#endif
}

// Component interface implementation (used when loaded as shared library)
#ifndef STANDALONE_MODE

static int radio_sim_component_init(component_state_t** state)
{
    radio_sim_state_t* radio_state = malloc(sizeof(radio_sim_state_t));
    if (!radio_state) {
        return COMPONENT_ERROR;
    }
    
    int result = radio_sim_init(radio_state);
    if (result != RADIO_SIM_SUCCESS) {
        free(radio_state);
        return COMPONENT_ERROR;
    }
    
    *state = (component_state_t*)radio_state;
    return COMPONENT_SUCCESS;
}

static void radio_sim_component_tick(component_state_t* state, uint64_t tick_time_ns, const simulith_42_context_t* context_42)
{
    if (!state) return;
    
    radio_sim_state_t* radio_state = (radio_sim_state_t*)state;
    
    // Set global state for the tick callback
    radio_sim_state_t* old_state = g_state;
    g_state = radio_state;
    
    // Call the original tick function with 42 context
    radio_sim_on_tick(tick_time_ns, context_42);
    
    // Restore previous state
    g_state = old_state;
}

static void radio_sim_component_cleanup(component_state_t* state)
{
    if (!state) return;
    
    radio_sim_state_t* radio_state = (radio_sim_state_t*)state;
    radio_sim_cleanup(radio_state);
    free(radio_state);
}

static const component_interface_t radio_sim_interface = {
    .name = "radio_sim",
    .description = "Radio simulation component for testing",
    .init = radio_sim_component_init,
    .tick = radio_sim_component_tick,
    .cleanup = radio_sim_component_cleanup,
    .configure = NULL  // Not implemented yet
};

// Component registration function - exported for dynamic loading
REGISTER_COMPONENT(radio_sim)
{
    return &radio_sim_interface;
}

// Export the registration function with a standard name for dynamic loading
__attribute__((visibility("default")))
const component_interface_t* get_component_interface(void)
{
    return &radio_sim_interface;
}

#endif // !STANDALONE_MODE

// Standalone mode main function
#ifdef STANDALONE_MODE

// Wrapper function for standalone mode (no 42 context available)
static void radio_sim_standalone_tick(uint64_t tick_time_ns)
{
    radio_sim_on_tick(tick_time_ns, NULL); // Pass NULL for 42 context in standalone mode
}

int main(int argc, char* argv[])
{
    radio_sim_state_t state;
    
    if (radio_sim_init(&state) != RADIO_SIM_SUCCESS) 
    {
        printf("Failed to initialize radio simulator\n");
        return 1;
    }
    
    printf("Sample simulator running. Press Ctrl+C to exit.\n");
    
    // Run the client loop with our standalone tick callback
    simulith_client_run_loop(radio_sim_standalone_tick);
    
    radio_sim_cleanup(&state);
    return 0;
}
#endif // STANDALONE_MODE 