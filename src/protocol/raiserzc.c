#include "protocol/raiserzc.h"
#include "canbox.h"     // Access to common canbox functions if needed (e.g. debug print)
#include "car.h"        // Access to car_get_* functions
#include "conf.h"       // Access to conf_get_* for potential config checks
#include "utils.h"      // Access to scale() ? (Or implement locally if needed)
#include "hw_usart.h"   // Needed for hw_usart_write

#include <string.h>     // For memcpy, memset
#include <stdio.h>      // For snprintf (debugging command process)

#include <stdbool.h>
// --- RZC Protocol Constants ---
// #define RZC_HEADER 0xDF // Seen in logs, might be needed for some HUs
#define RZC_HEADER 0xFD // As per specification document
#define RZC_MAX_DATA_LEN 30 // Define a reasonable max data payload size
#define RZC_MAX_FRAME_LEN (1 + 1 + 1 + RZC_MAX_DATA_LEN + 1) // HD+LEN+TYPE+DATA+CS

// --- RZC DataType Definitions (Slave -> Host) ---
#define RZC_DTYPE_SLAVE_CONTROL     0x01 // Backlight etc. (Low priority)
#define RZC_DTYPE_BUTTON_CMD        0x02 // Buttons (Steering Wheel, Panel)
#define RZC_DTYPE_AC_INFO           0x21 // AC Info (14款408 specific?)
#define RZC_DTYPE_WHEEL_ANGLE       0x29 // Steering Wheel Angle
#define RZC_DTYPE_RADAR_ALL         0x30 // All-around Radar
#define RZC_DTYPE_RADAR_REVERSE     0x32 // Reverse Radar only
#define RZC_DTYPE_TRIP_PAGE0        0x33 // Trip Computer Inst + Range
#define RZC_DTYPE_TRIP_PAGE1        0x34 // Trip Computer Trip 1
#define RZC_DTYPE_TRIP_PAGE2        0x35 // Trip Computer Trip 2
#define RZC_DTYPE_OUTSIDE_TEMP      0x36 // Outside Temperature
#define RZC_DTYPE_ALERT_RECORDS     0x37 // Stored Alerts (Low priority)
#define RZC_DTYPE_VEHICLE_STATUS    0x38 // Doors, Lights, Settings Status
#define RZC_DTYPE_FUNCTION_STATUS   0x39 // Model-specific status (Low priority)
#define RZC_DTYPE_DIAGNOSTIC_INFO   0x3A // Diagnostics (Low priority)
#define RZC_DTYPE_TRIP_CLEAR_STATUS 0x3D // Trip Clear Status (Low priority)
#define RZC_DTYPE_MEM_SPEED         0x3B // Memorized Speed (Low priority)
// Note: 0x3D is listed twice in spec with different meanings, check context
//#define RZC_DTYPE_CRUISE_LIMIT      0x3D // Cruise/Limit Settings (Low priority)
#define RZC_DTYPE_CRUISE_POPUP      0x3F // Cruise/Limit Popup (Low priority)
#define RZC_DTYPE_VERSION_INFO      0x7F // CANbox Version

// --- RZC DataType Definitions (Host -> Slave) ---
#define RZC_DTYPE_VEHICLE_SET       0x80 // Vehicle Parameter Setting
#define RZC_DTYPE_TRIP_SET          0x82 // Trip Computer Setting
#define RZC_DTYPE_REQ_ALERTS        0x85 // Request Alert Records
#define RZC_DTYPE_REQ_FUNC_STATUS   0x86 // Request Function Status
#define RZC_DTYPE_REQ_DIAG          0x87 // Request Diagnostic Info
#define RZC_DTYPE_SET_MEM_SPEED     0x88 // Set Memorized Speed
#define RZC_DTYPE_SET_CRUISE_LIMIT  0x89 // Set Cruise/Limit Speed
#define RZC_DTYPE_AC_SET            0x8A // AC Setting Command
#define RZC_DTYPE_REQ_DISPLAY       0x8F // Request Display Info
#define RZC_DTYPE_SET_CRUISE_INSTR  0x99 // Cruise/Limit Instrument Setting
#define RZC_DTYPE_TIME_SET          0xA6 // Time Setting Command

// --- Internal Helper Functions ---

/**
 * @brief Calculates the RZC protocol checksum.
 * @param data_type The DataType byte.
 * @param length The Length byte (DataType + Data size).
 * @param data Pointer to the Data payload buffer.
 * @param data_size Size of the Data payload.
 * @return The calculated 8-bit checksum.
 */
static uint8_t raise_rcz_checksum(uint8_t data_type, uint8_t length, const uint8_t *data, uint8_t data_size) {
    uint8_t sum = 0;
    sum += length;
    sum += data_type;
    for (uint8_t i = 0; i < data_size; i++) {
        sum += data[i];
    }
    return sum;
}

/**
 * @brief Sends a correctly formatted RZC protocol message over UART.
 * @param data_type The DataType byte.
 * @param msg Pointer to the Data payload buffer.
 * @param size Size of the Data payload (n).
 */
static void snd_raise_rcz_msg(uint8_t data_type, const uint8_t *msg, uint8_t size) {
    if (size > RZC_MAX_DATA_LEN) {
        // Handle error: data payload too large
        #ifdef DEBUG
        char dbg_buf[64];
        snprintf(dbg_buf, sizeof(dbg_buf), "RZC TX ERR: Payload too large (%d > %d) for DType 0x%02X\r\n", size, RZC_MAX_DATA_LEN, data_type);
        hw_usart_write(hw_usart_get(), (uint8_t*)dbg_buf, strlen(dbg_buf));
        #endif
        return;
    }

    uint8_t frame[RZC_MAX_FRAME_LEN];
    uint8_t length = 1 + size; // Length = DataType byte + Data bytes

    // Basic check for valid lengths based on known types
    // This helps catch errors early during development
    bool length_ok = true;
    switch (data_type) {
        case RZC_DTYPE_BUTTON_CMD:     length_ok = (length == 4); break;
        case RZC_DTYPE_WHEEL_ANGLE:    length_ok = (length == 3); break;
        case RZC_DTYPE_RADAR_REVERSE:  length_ok = (length == 8); break;
        case RZC_DTYPE_TRIP_PAGE0:     length_ok = (length == 12); break; // 1+11
        case RZC_DTYPE_TRIP_PAGE1:     length_ok = (length == 7); break;  // 1+6
        case RZC_DTYPE_TRIP_PAGE2:     length_ok = (length == 7); break;  // 1+6
        case RZC_DTYPE_OUTSIDE_TEMP:   length_ok = (length == 2); break;  // 1+1
        case RZC_DTYPE_VEHICLE_STATUS: length_ok = (length == 7); break;  // 1+6
        // Add other types if their lengths are fixed and known
    }
    if (!length_ok) {
        #ifdef DEBUG
        char dbg_buf[64];
        snprintf(dbg_buf, sizeof(dbg_buf), "RZC TX WARN: Incorrect LEN (%d) for DType 0x%02X\r\n", length, data_type);
        hw_usart_write(hw_usart_get(), (uint8_t*)dbg_buf, strlen(dbg_buf));
        #endif
        // Optionally return here to prevent sending incorrect frames, or proceed cautiously
        // return;
    }


    frame[0] = RZC_HEADER;       // Header
    frame[1] = length;           // Length
    frame[2] = data_type;        // DataType

    if (msg && size > 0) {
        memcpy(&frame[3], msg, size); // Copy Data payload
    }

    // Calculate Checksum (covers Length, DataType, and Data)
    frame[3 + size] = raise_rcz_checksum(data_type, length, msg, size);

    // Send the complete frame
    hw_usart_write(hw_usart_get(), frame, 3 + size + 1); // HD+LEN+TYPE+DATA+CS
}

// Helper to map temperature (°C) to RZC format (Temp + 68)
static uint8_t map_temp_to_rzc(int16_t temp_c) {
    uint8_t value = 0;
    // New Mapping: Temp + 68
    int16_t mapped_temp = temp_c + 68;

    // Clamp to 0-255 range
    if (mapped_temp < 0) {
        value = 0;
    } else if (mapped_temp > 255) {
        value = 255;
    } else {
        value = (uint8_t)mapped_temp;
    }
    return value;
}

// Helper to map radar distance (0-7) to RZC radar distance (0-5)
static uint8_t map_radar_dist_to_rzc(uint8_t car_dist_0_7) {
    // RZC uses 0=Closest(5 bars), 1=4 bars, ..., 4=1 bar, 5=Inactive
    // CAR uses 0=Closest, ..., 6=Far, 7=Inactive
    if (car_dist_0_7 >= 7) return 0x05; // Inactive/Farthest
    if (car_dist_0_7 == 0) return 0x00; // Closest
    if (car_dist_0_7 == 1) return 0x01;
    if (car_dist_0_7 == 2) return 0x01; // Map 1&2 to 4 bars
    if (car_dist_0_7 == 3) return 0x02; // 3 bars
    if (car_dist_0_7 == 4) return 0x03; // 2 bars
    if (car_dist_0_7 == 5) return 0x04; // 1 bar
    if (car_dist_0_7 == 6) return 0x04; // Map 5&6 to 1 bar

    return 0x05; // Default to inactive
}

// --- Protocol Implementation Functions ---

// DataType 0x38: Vehicle Status (Doors, Lights, Settings subset)
// This seems like the most comprehensive single status message.
static void raise_rcz_vehicle_status_process(void) {
    uint8_t data[6]; // Payload size is 6 for DataType 0x38 (LEN=7 -> 1 DType + 6 Data)

    // Check the primary enabling state (ACC) first
    if (car_get_acc() == 0) {
        // --- ACC is OFF: Send the explicit "All OFF" status ---
        memset(data, 0x00, sizeof(data));
        // Ensure all relevant bits are 0. memset already does this.
    } else {
        // --- ACC is ON: Build payload based on current states ---
        memset(data, 0x00, sizeof(data)); // Start with a clean slate

        // Data0 (Doors)
        if (car_get_door_fl()) data[0] |= 0x80;
        if (car_get_door_fr()) data[0] |= 0x40;
        if (car_get_door_rl()) data[0] |= 0x20;
        if (car_get_door_rr()) data[0] |= 0x10;
        if (car_get_tailgate()) data[0] |= 0x08;
        // Bonnet? Not in spec for 0x38 Data0

        // Data1 (Settings/Status 1)
        struct radar_t radar; car_get_radar(&radar);
        // Park Assist System bit (Bit 3) - Set only if ACC is ON *and* radar is available/active
        if (radar.state != e_radar_off && radar.state != e_radar_undef) data[1] |= 0x08;

        // Data3 (Settings/Status 3)
        // Reverse Status (Bit 2) - Only possible if ACC is ON
        if (get_rear_delay_state()) data[3] |= 0x04;
        // Park Brake Status (Bit 1) - Can be ON even if ACC is OFF, but protocol might expect 0 if ACC=0
        if (car_get_park_break()) data[3] |= 0x02;
        // Park Light Status (Bit 0) - Can be ON even if ACC is OFF, but protocol might expect 0 if ACC=0
        if (car_get_park_lights()) data[3] |= 0x01;

        // Data 2, 4, 5 remain 0
    }

    // Send the message (payload is now correct for ON or OFF state)
    snd_raise_rcz_msg(RZC_DTYPE_VEHICLE_STATUS, data, sizeof(data));
}

// DataType 0x29: Steering Wheel Angle
static void raise_rcz_wheel_process(uint8_t type, int16_t min, int16_t max) {
    (void)type; // RZC uses fixed DataType 0x29

    int8_t wheel_percent = 0;
    if (!car_get_wheel(&wheel_percent)) return; // Only send if valid data exists

    // Scale -100..+100 percent to RZC's -5450..+5450 range
    // Note: RZC spec says <0 is RIGHT, >0 is LEFT. Our carstate is <0 LEFT, >0 RIGHT. Need to invert.
    int16_t rcz_angle = (int16_t)scale((float)(-wheel_percent), -100.0f, 100.0f, (float)min, (float)max);

    uint8_t data[2]; // LEN=3 -> 2 data bytes
    data[0] = rcz_angle & 0xFF;        // Low Byte
    data[1] = (rcz_angle >> 8) & 0xFF; // High Byte

    snd_raise_rcz_msg(RZC_DTYPE_WHEEL_ANGLE, data, sizeof(data));
}


// DataType 0x32: Reverse Radar Info (Using this as primary for simplicity)
static void raise_rcz_radar_process(uint8_t fmax[4], uint8_t rmax[4]) {
    (void)fmax; // Not used in this protocol format
    (void)rmax;

    struct radar_t radar;
    car_get_radar(&radar);

    uint8_t data[7]; // LEN = 0x08 -> 7 DATA bytes for 0x32

    // Data0: Radar Status
    if (radar.state == e_radar_off || radar.state == e_radar_undef) {
        data[0] = 0x03; // Disabled
        // Set all distances to inactive when disabled
        memset(&data[1], 0x05, 6); // Fill Distances (Data1-6) with '5' (Inactive)
    } else {
        data[0] = 0x02; // Enabled and Display Info
        // Map distances (0-7 -> 0-5)
        data[1] = map_radar_dist_to_rzc(radar.rl);  // Rear Left
        data[2] = map_radar_dist_to_rzc(radar.rlm); // Rear Middle (Use RLM for single middle)
        data[3] = map_radar_dist_to_rzc(radar.rr);  // Rear Right
        data[4] = map_radar_dist_to_rzc(radar.fl);  // Front Left
        data[5] = map_radar_dist_to_rzc(radar.flm); // Front Middle (Use FLM for single middle)
        data[6] = map_radar_dist_to_rzc(radar.fr);  // Front Right
    }

    snd_raise_rcz_msg(RZC_DTYPE_RADAR_REVERSE, data, sizeof(data));
    // Note: Sending 0x30 (All-around) might be needed if HU expects it, structure is different.
}


// DataType 0x36: Outside Temperature
static void raise_rcz_temperature_process() {
    int16_t temp_c = car_get_temp();
    uint8_t data[1]; // LEN=2 -> 1 data byte
    data[0] = map_temp_to_rzc(temp_c);
    snd_raise_rcz_msg(RZC_DTYPE_OUTSIDE_TEMP, data, sizeof(data));
}

// DataType 0x33: Trip Page 0
static void raise_rcz_trip0_process() {
    uint8_t data[11]; // LEN=12 -> 11 data bytes
    memset(data, 0xFF, sizeof(data)); // Initialize all to invalid

    // Scale and format instantaneous consumption (L/100km * 10?) -> VERIFY TARGET UNIT/SCALING
    uint16_t inst_cons_raw = car_get_inst_consumption_raw();
    uint16_t cons_scaled = 0xFFFF; // Default to invalid
    if (inst_cons_raw != 0xFFFF) { // Assuming 0xFFFF indicates invalid raw value
        // ASSUMPTION: Raw value from CAN (0x221) is L/100km * 100 or similar.
        // We need L/100km * 10 for RZC. Divide by 10. Verify this assumption!
        cons_scaled = inst_cons_raw / 10;
        if (cons_scaled > 999) cons_scaled = 999; // Clamp to max reasonable value (99.9 L/100km)
    }
    data[0] = (cons_scaled >> 8) & 0xFF;
    data[1] = cons_scaled & 0xFF;

    // Format range (KM)
    uint16_t range_km = car_get_range_km();
    if (range_km != 0xFFFF) { // Assuming 0xFFFF indicates invalid range
        data[2] = (range_km >> 8) & 0xFF;
        data[3] = range_km & 0xFF;
    } // else leave as FF FF

    // Data 4-5 (Set Destination) - Usually not sent by CANbox
    // Data 6-8 (Start/Stop Time) - Likely not applicable
    // Data 9-10 - Unknown/Reserved

    snd_raise_rcz_msg(RZC_DTYPE_TRIP_PAGE0, data, sizeof(data));
}

// Function for Trip 1 & 2
static void raise_rcz_trip1_process() {
    uint8_t data[6]; // LEN=7 -> 6 data bytes
    memset(data, 0xFF, sizeof(data)); // Initialize all to invalid

    // Avg Cons 1 (L/100km * 10) - Assuming raw is *100
    uint16_t avg_cons1_raw = car_get_avg_consumption1_raw();
    uint16_t avg_cons1_scaled = (avg_cons1_raw == 0xFFFF) ? 0xFFFF : avg_cons1_raw / 10;
    if (avg_cons1_scaled > 999) avg_cons1_scaled = 999; // Clamp
    data[0] = (avg_cons1_scaled >> 8) & 0xFF;
    data[1] = avg_cons1_scaled & 0xFF;

    // Avg Speed 1 (km/h)
    uint16_t avg_speed1 = car_get_avg_speed1(); // Already km/h
    if (avg_speed1 != 0xFFFF) {
        data[2] = (avg_speed1 >> 8) & 0xFF;
        data[3] = avg_speed1 & 0xFF;
    }

    // Distance 1 (km)
    uint32_t dist1_32 = car_get_trip_distance1(); // Already km (32-bit source)
    uint16_t dist1 = 0xFFFF; // Default invalid
    if (dist1_32 != 0xFFFFFFFF) { // Check if source is valid
        dist1 = (dist1_32 > 0xFFFF) ? 0xFFFF : (uint16_t)dist1_32; // Clamp to 16-bit
    }
    data[4] = (dist1 >> 8) & 0xFF;
    data[5] = dist1 & 0xFF;

    snd_raise_rcz_msg(RZC_DTYPE_TRIP_PAGE1, data, sizeof(data));
}

static void raise_rcz_trip2_process() {
    uint8_t data[6]; // LEN=7 -> 6 data bytes
    memset(data, 0xFF, sizeof(data)); // Initialize all to invalid

    // Avg Cons 2 (L/100km * 10) - Assuming raw is *100
    uint16_t avg_cons2_raw = car_get_avg_consumption2_raw();
    uint16_t avg_cons2_scaled = (avg_cons2_raw == 0xFFFF) ? 0xFFFF : avg_cons2_raw / 10;
    if (avg_cons2_scaled > 999) avg_cons2_scaled = 999; // Clamp
    data[0] = (avg_cons2_scaled >> 8) & 0xFF;
    data[1] = avg_cons2_scaled & 0xFF;

    // Avg Speed 2 (km/h)
    uint16_t avg_speed2 = car_get_avg_speed2(); // Already km/h
    if (avg_speed2 != 0xFFFF) {
        data[2] = (avg_speed2 >> 8) & 0xFF;
        data[3] = avg_speed2 & 0xFF;
    }

    // Distance 2 (km)
    uint32_t dist2_32 = car_get_trip_distance2(); // Already km (32-bit source)
    uint16_t dist2 = 0xFFFF; // Default invalid
    if (dist2_32 != 0xFFFFFFFF) { // Check if source is valid
        dist2 = (dist2_32 > 0xFFFF) ? 0xFFFF : (uint16_t)dist2_32; // Clamp to 16-bit
    }
    data[4] = (dist2 >> 8) & 0xFF;
    data[5] = dist2 & 0xFF;

    snd_raise_rcz_msg(RZC_DTYPE_TRIP_PAGE2, data, sizeof(data));
}

// Combine multiple sends into logical groups for canbox_process/park_process
static void raise_rcz_send_main_status() {
    raise_rcz_vehicle_status_process(); // Sends 0x38
    raise_rcz_temperature_process();    // Sends 0x36
}

static void raise_rcz_send_trip_info() {
    raise_rcz_trip0_process(); // Sends 0x33
    raise_rcz_trip1_process(); // Sends 0x34
    raise_rcz_trip2_process(); // Sends 0x35
}


// --- Button Handling ---

// Helper to send RZC 0x02 Button Command
static void send_raise_rcz_key(uint8_t rzc_key_code, uint8_t status) {
    uint8_t data[3]; // LEN=4 -> 3 data bytes
    data[0] = rzc_key_code;
    data[1] = status; // 0x01 = Press, 0x00 = Release
    data[2] = 0x00;   // Reserved
    snd_raise_rcz_msg(RZC_DTYPE_BUTTON_CMD, data, sizeof(data));
}

// Mappings from generic callbacks to RZC key codes (verify these from the spec table!)
static void raise_rcz_inc_volume(uint8_t val) { (void)val; send_raise_rcz_key(0x14, 1); send_raise_rcz_key(0x14, 0); }
static void raise_rcz_dec_volume(uint8_t val) { (void)val; send_raise_rcz_key(0x15, 1); send_raise_rcz_key(0x15, 0); }
static void raise_rcz_prev(void) { send_raise_rcz_key(0x13, 1); send_raise_rcz_key(0x13, 0); } // Seek-
static void raise_rcz_next(void) { send_raise_rcz_key(0x12, 1); send_raise_rcz_key(0x12, 0); } // Seek+
static void raise_rcz_mode(void) { send_raise_rcz_key(0x11, 1); send_raise_rcz_key(0x11, 0); } // Source/Phone
static void raise_rcz_cont(void) { send_raise_rcz_key(0x30, 1); send_raise_rcz_key(0x30, 0); } // Tel On (Answer) - Map 'cont' here?
static void raise_rcz_mici(void) { send_raise_rcz_key(0x29, 1); send_raise_rcz_key(0x29, 0); } // Push To Talk (Voice)

// --- Command Processing (Host -> Slave) ---

// RX State machine structure for RZC protocol
enum rzc_rx_state {
    RX_RZC_WAIT_START,
    RX_RZC_LEN,
    RX_RZC_DTYPE,
    RX_RZC_DATA,
    RX_RZC_CS
};

#define RX_RZC_BUFFER_SIZE 32 // Max expected frame size

static void raise_rcz_cmd_process(uint8_t ch) {
    static enum rzc_rx_state rx_state = RX_RZC_WAIT_START;
    static uint8_t rx_buffer[RX_RZC_BUFFER_SIZE]; // Buffer to hold LEN, DTYPE, DATA
    static uint8_t rx_len = 0; // Expected LEN value from frame
    static uint8_t rx_idx = 0; // Current index into rx_buffer
    static uint8_t rx_dtype = 0;

    switch (rx_state) {
        case RX_RZC_WAIT_START:
            if (ch == RZC_HEADER) {
                rx_idx = 0; // Reset buffer index
                rx_state = RX_RZC_LEN;
            }
            break;

        case RX_RZC_LEN:
            // Check buffer bounds: LEN=1 (DTYPE only) + 3 (HD,LEN,CS) = 4 bytes min frame.
            // Max LEN = 1 + RZC_MAX_DATA_LEN. Max Frame = HD+LEN+DTYPE+DATA+CS = 3 + LEN.
            if (ch >= 1 && (3 + ch) <= RX_RZC_BUFFER_SIZE) {
                rx_len = ch;
                rx_buffer[rx_idx++] = ch; // Store LEN (at index 0 of payload buffer)
                rx_state = RX_RZC_DTYPE;
            } else {
                rx_state = RX_RZC_WAIT_START; // Invalid length
            }
            break;

        case RX_RZC_DTYPE:
            rx_dtype = ch;
            rx_buffer[rx_idx++] = ch; // Store DTYPE (at index 1 of payload buffer)
            if (rx_len == 1) { // Only DataType, no Data payload
                rx_state = RX_RZC_CS;
            } else {
                rx_state = RX_RZC_DATA;
            }
            break;

        case RX_RZC_DATA:
            rx_buffer[rx_idx++] = ch; // Store Data byte (starts at index 2 of payload buffer)
            // Check if we have received all expected bytes (LEN + CS byte placeholder)
            // We need LEN byte + DType byte + (LEN-1) Data bytes = LEN+1 bytes in buffer so far.
            if (rx_idx >= (rx_len + 1)) {
                rx_state = RX_RZC_CS;
            }
            break;

        case RX_RZC_CS:
            {
                uint8_t received_checksum = ch;
                // Checksum includes LEN (rx_buffer[0]), DTYPE (rx_buffer[1]), and DATA (rx_buffer[2] to rx_buffer[rx_len])
                // Data size = rx_len - 1
                uint8_t calculated_checksum = raise_rcz_checksum(rx_dtype, rx_len, &rx_buffer[2], rx_len - 1);

                if (calculated_checksum == received_checksum) {
                    // Checksum OK - Process command
                     #ifdef DEBUG // Use the standard DEBUG definition
                     char dbg_buf[64];
                     snprintf(dbg_buf, sizeof(dbg_buf), "RZC RX OK: DType=0x%02X Len=%d\r\n", rx_dtype, rx_len);
                     hw_usart_write(hw_usart_get(), (uint8_t*)dbg_buf, strlen(dbg_buf));
                     #endif
                    // TODO: Add actual command handling based on rx_dtype
                    // For now, ignore payload as per strategy.
                } else {
                    // Checksum Failed
                     #ifdef DEBUG
                     char dbg_buf[64];
                     snprintf(dbg_buf, sizeof(dbg_buf), "RZC RX CS FAIL: DType=0x%02X Got=0x%02X Exp=0x%02X\r\n",
                              rx_dtype, received_checksum, calculated_checksum);
                     hw_usart_write(hw_usart_get(), (uint8_t*)dbg_buf, strlen(dbg_buf));
                     #endif
                }
            }
            rx_state = RX_RZC_WAIT_START; // Reset state machine
            break;

        default:
            rx_state = RX_RZC_WAIT_START; // Reset on error
            break;
    }
}


// --- Protocol Operations Structure Instance ---

const protocol_ops_t raise_rcz_protocol_ops = {
    .radar_process = raise_rcz_radar_process, // Use 0x32 for simplicity
    .wheel_process = raise_rcz_wheel_process, // Use 0x29
    .door_process = raise_rcz_send_main_status, // Send 0x38 (includes doors) and 0x36 (temp)
    .vehicle_info_process = raise_rcz_send_trip_info, // Send 0x33, 0x34, 0x35
    .ac_process = NULL, // No dedicated AC message function defined yet (0x21 is model specific) - could add to vehicle_status or implement 0x21
    .inc_volume = raise_rcz_inc_volume,       // Map to 0x02 + key code
    .dec_volume = raise_rcz_dec_volume,       // Map to 0x02 + key code
    .prev = raise_rcz_prev,                   // Map to 0x02 + key code
    .next = raise_rcz_next,                   // Map to 0x02 + key code
    .mode = raise_rcz_mode,                   // Map to 0x02 + key code
    .cont = raise_rcz_cont,                   // Map to 0x02 + key code
    .mici = raise_rcz_mici,                   // Map to 0x02 + key code
    .cmd_process = raise_rcz_cmd_process,       // Handle incoming commands from HU
    .park_process = raise_rcz_radar_process,    // Reuse radar function for parking
};
