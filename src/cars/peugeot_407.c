#include <string.h>
#include <stdbool.h>
#include "car.h"
#include "hw_can.h"
#include "utils.h"  // Make sure utils.h includes the scale function

// Peugeot 407 CAN message handlers (using data from fazerxlo/simulator)

// --- Helper Function (Inline for Efficiency) ---
static inline uint16_t get_be16(const uint8_t *buf) {
    return ((uint16_t)buf[0] << 8) | buf[1];
}

static inline int16_t get_be16_signed(const uint8_t *buf) {
    return (int16_t)get_be16(buf);
}

static inline uint32_t get_be32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
}

static inline uint32_t get_be24(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
}

// --- Defines based on PSACANBridge analysis for 0x0B6 ---
#define ID_0x0B6_RPM_BYTE_MSB       0
#define ID_0x0B6_SPEED_BYTE_MSB     2
#define ID_0x0B6_COOLANT_BYTE       5 // From YAML, matches 0x0F6 coolant byte in PSACANBridge
#define ID_0x0B6_COOLANT_INVALID    0xFF // Assume FF is invalid like oil temp
#define ID_0x0B6_COOLANT_COLD_RAW   0x00 // Value seen in logs when likely cold

static void peugeot_407_ms_0B6_engine_status_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        return;
    }

    uint16_t rpm_raw = get_be16(&msg[ID_0x0B6_RPM_BYTE_MSB]);

    // Apply the experimentally determined factor of 8
    carstate.taho = rpm_raw / 8;

    // Update engine running state based on RPM
    // Threshold might need adjustment based on idle speed. 500 is common.
    if (carstate.taho > 400) { // Engine considered running if RPM > ~400
        carstate.engine = 1;
    } else {
        carstate.engine = 0;
    }

    // --- Decode Engine Coolant Temperature ---
    uint8_t coolant_raw = msg[ID_0x0B6_COOLANT_BYTE]; // Using Byte 5 as per YAML

    // Check for potentially invalid readings (like 0xFF or the 0x00 seen in logs)
    if (coolant_raw == ID_0x0B6_COOLANT_INVALID || coolant_raw == ID_0x0B6_COOLANT_COLD_RAW) {
         // Keep previous value or set to invalid marker? Let's use -48 (min possible)
         // Only reset fully if it hasn't received a valid value yet
        if (carstate.engine_temp == 0) { // Check if still initial value
             carstate.engine_temp = -48;
         }
         // Otherwise, hold the last known good temperature during brief invalid readings
    } else {
        carstate.engine_temp = (int16_t)(((float)coolant_raw * 0.75f) - 48);
    }

    //Speed, Ignition, and Illumination from this message ID based on analysis.

    uint16_t speed_raw = get_be16(&msg[ID_0x0B6_SPEED_BYTE_MSB]);
    carstate.speed = (uint16_t)((float)speed_raw * 0.01f);

}


// --- Defines based on verified 0x36 log data ---
#define ID_0x036_IGN_STATE_BYTE         4
#define ID_0x036_IGN_STATE_MASK         0x03 // Bits 0-1 seem sufficient based on values 0,1,2,3
#define ID_0x036_IGN_STATE_OFF          0x00
#define ID_0x036_IGN_STATE_ON           0x01
#define ID_0x036_IGN_STATE_CRANKING     0x02 // State during engine start
#define ID_0x036_IGN_STATE_ACC          0x03

#define ID_0x036_LIGHT_BYTE             3
#define ID_0x036_LIGHT_ENABLE_MASK      0x20 // Bit 5: Dashboard lighting enabled
#define ID_0x036_LIGHT_BRIGHTNESS_MASK  0x0F // Bits 0-3: Brightness level (0-15)

static void peugeot_407_ms_036_ign_light_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        return;
    }

    // Decode current message
    uint8_t raw_ign_state = msg[ID_0x036_IGN_STATE_BYTE] & ID_0x036_IGN_STATE_MASK;
    uint8_t brightness = msg[ID_0x036_LIGHT_BYTE] & ID_0x036_LIGHT_BRIGHTNESS_MASK;

    // --- Update Ignition/ACC State ---
    switch (raw_ign_state) {
        case ID_0x036_IGN_STATE_ON:
            carstate.ign = 1;
            carstate.acc = 1;
            break;
        case ID_0x036_IGN_STATE_ACC:
        case ID_0x036_IGN_STATE_CRANKING:
            carstate.ign = 0;
            carstate.acc = 1;
            break;
        case ID_0x036_IGN_STATE_OFF:
        default: // Treat unknown states as OFF
            carstate.ign = 0;
            carstate.acc = 0;
            break;
    }

    // --- Update Illumination State ---
    // Store the brightness level (0-15)
    carstate.illum = brightness;
}



// --- Defines based on verified 0x0F6 log data ---
#define ID_0x0F6_REVERSE_BYTE           7
#define ID_0x0F6_REVERSE_MASK           0x80

#define ID_0x0F6_COOLANT_BYTE           1

#define ID_0x0F6_ODOMETER_BYTE      2
#define ID_0x0F6_AMBIENT_TEMP_BYTE      6

static void peugeot_407_ms_0F6_status_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        return;
    }

    // --- Decode Reverse Gear Status (VERIFY MASK!) ---
    if (msg[ID_0x0F6_REVERSE_BYTE] & ID_0x0F6_REVERSE_MASK) {
        carstate.selector = e_selector_r;
    } else {
        carstate.selector = e_selector_n;
    }

    // --- Decode Ambient Temperature ---
    // Using formula from PSACANBridge: (RAW * 0.5) - 40.0
    uint8_t ambient_temp_raw = msg[ID_0x0F6_AMBIENT_TEMP_BYTE];
    if (ambient_temp_raw != 0xFF) { // Check for invalid value
       // Cast to float for calculation, then back to int16_t for storage
       carstate.temp = (int16_t)(((float)ambient_temp_raw * 0.5f) - 40.0f);
    } else {
       carstate.temp = -40; // Or some other indicator of invalid data
    }


    //Odometer

    uint32_t odometer_raw = get_be24(&msg[ID_0x0F6_ODOMETER_BYTE]);
    carstate.odometer = odometer_raw/10;

    uint8_t coolant_temp_raw = msg[ID_0x0F6_COOLANT_BYTE];
    carstate.engine_temp = (int16_t)(((float)coolant_temp_raw) - 40.0f);


    

}

// --- Defines based on PSACANBridge code for 0x128 ---
#define ID_0x128_BYTE0    0
#define ID_0x128_PARK_LIGHT_MASK    0x20 // Bit 8: Sidelights/Parking Lights
#define ID_0x128_SEATBELT_MASK      0x40 // Bit 8: Driver Seatbelt Warning Light (1 = Warning/Unfastened?)
#define ID_0x128_LOW_FUEL_MASK      0x10 // Bit 4: Low Fuel Warning Light

#define ID_0x128_NEAR_LIGHT_BYTE    4
#define ID_0x128_NEAR_LIGHT_MASK    0x40 // Bit 6: Low Beam/Near Lights



// This handler primarily updates light statuses and potentially warning light statuses.
// Prefer dedicated messages for primary state of Park Brake if available.
static void peugeot_407_ms_128_lights_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        return;
    }

    // --- Decode Light Status ---
    carstate.park_lights = (msg[ID_0x128_BYTE0] & ID_0x128_PARK_LIGHT_MASK) ? 1 : 0;
    carstate.near_lights = (msg[ID_0x128_NEAR_LIGHT_BYTE] & ID_0x128_NEAR_LIGHT_MASK) ? 1 : 0;
    carstate.park_break = (msg[ID_0x128_BYTE0] & ID_0x128_PARK_LIGHT_MASK) ? 1 : 0;
    carstate.ds_belt = (msg[ID_0x128_BYTE0] & ID_0x128_SEATBELT_MASK) ? 1 : 0;
    carstate.low_fuel_lvl = (msg[ID_0x128_BYTE0] & ID_0x128_LOW_FUEL_MASK) ? 1 : 0;

}


// --- Defines based on PSACANBridge code for 0x21F ---
#define ID_0x21F_VOL_BYTE       3
#define ID_0x21F_VOL_UP_MASK    0x08 // Bit 3
#define ID_0x21F_VOL_DOWN_MASK  0x04 // Bit 2

#define ID_0x21F_SEEK_BYTE      7
#define ID_0x21F_SEEK_UP_MASK   0x80 // Bit 7 (Next)
#define ID_0x21F_SEEK_DOWN_MASK 0x40 // Bit 6 (Previous)

#define ID_0x21F_SRC_BYTE       1
#define ID_0x21F_SRC_MASK       0x02 // Bit 1 (Mode/Source)

// TODO: Define masks/logic for other buttons if identified (e.g., Phone/Cont, Mici/VR)
// They might be on this ID or another one (like 0x1B0 from RCZ).

static void peugeot_407_ms_21F_swc_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        // Reset previous key states if message stops
        key_state.key_volume = STATE_UNDEF;
        key_state.key_prev = 0; // Assume released on timeout
        key_state.key_next = 0;
        key_state.key_mode = 0;
        // Reset others if added
        return;
    }

    // --- Volume Handling ---
    uint8_t current_vol_up = (msg[ID_0x21F_VOL_BYTE] & ID_0x21F_VOL_UP_MASK);
    uint8_t current_vol_down = (msg[ID_0x21F_VOL_BYTE] & ID_0x21F_VOL_DOWN_MASK);

    // Check Volume Up press (transition from not pressed to pressed)
    if (current_vol_up && (key_state.key_volume != 1)) {
        if (key_state.key_cb && key_state.key_cb->inc_volume) {
            key_state.key_cb->inc_volume(1); // Argument '1' might indicate steps, adjust if needed
        }
        key_state.key_volume = 1; // Store state as 'Up pressed'
    }
    // Check Volume Down press
    else if (current_vol_down && (key_state.key_volume != 0)) {
        if (key_state.key_cb && key_state.key_cb->dec_volume) {
            key_state.key_cb->dec_volume(1);
        }
        key_state.key_volume = 0; // Store state as 'Down pressed'
    }
    // Check for release (neither button pressed)
    else if (!current_vol_up && !current_vol_down && key_state.key_volume != STATE_UNDEF) {
        key_state.key_volume = STATE_UNDEF; // Reset state to released/unknown
    }

    // --- Seek/Track Handling ---
    uint8_t current_seek_down = (msg[ID_0x21F_SEEK_BYTE] & ID_0x21F_SEEK_DOWN_MASK);
    uint8_t current_seek_up = (msg[ID_0x21F_SEEK_BYTE] & ID_0x21F_SEEK_UP_MASK);

    // Check Previous Track press (transition from 0 to 1)
    if (current_seek_down && !key_state.key_prev) {
         if (key_state.key_cb && key_state.key_cb->prev) {
            key_state.key_cb->prev();
        }
    }
    key_state.key_prev = current_seek_down ? 1 : 0; // Update previous state

    // Check Next Track press (transition from 0 to 1)
    if (current_seek_up && !key_state.key_next) {
         if (key_state.key_cb && key_state.key_cb->next) {
            key_state.key_cb->next();
        }
    }
    key_state.key_next = current_seek_up ? 1 : 0; // Update previous state

    // --- Source/Mode Button Handling ---
    uint8_t current_mode = (msg[ID_0x21F_SRC_BYTE] & ID_0x21F_SRC_MASK);
    // Check Mode press (transition from 0 to 1)
    if (current_mode && !key_state.key_mode) {
        if (key_state.key_cb && key_state.key_cb->mode) {
            key_state.key_cb->mode();
        }
    }
    key_state.key_mode = current_mode ? 1 : 0; // Update previous state


    // --- TODO: Handle Scroll Wheel (Byte 0) ---
    // Needs analysis of how the value changes. Is it a delta? Absolute?
    // Map to appropriate key_cb functions if needed (e.g., could potentially map to vol up/down or list scroll).

    // --- TODO: Handle other buttons (Phone/Cont, Mici/VR) ---
    // Determine their CAN ID and data bits via sniffing and add logic here or in a separate handler.
}

// --- Handler Functions ---

static void peugeot_407_ms_vin_336_handler(const uint8_t *msg, struct msg_desc_t *desc)
{
	if (is_timeout(desc)) {
		// Handle timeout (maybe set a flag indicating VIN not fully received)
        memset(carstate.vin, 0, sizeof(carstate.vin)); // Clear the VIN
        carstate.vin[0] = 'n'; //"na"
		carstate.vin[1] = 'a';
		return;
	}
	// First 3 chars of VIN
    memcpy(carstate.vin, msg, 3);

}

static void peugeot_407_ms_vin_3B6_handler(const uint8_t *msg, struct msg_desc_t *desc)
{
    if (is_timeout(desc)) {
       // Handle timeout
		memset(carstate.vin, 0, sizeof(carstate.vin)); // Clear the VIN
        carstate.vin[0] = 'n'; //"na"
		carstate.vin[1] = 'a';
		return;
	}

	// Chars 4-9 of VIN
    memcpy(carstate.vin + 3, msg, 6);
}

static void peugeot_407_ms_vin_2B6_handler(const uint8_t *msg, struct msg_desc_t *desc)
{
	if (is_timeout(desc)) {
		// Handle timeout
        memset(carstate.vin, 0, sizeof(carstate.vin)); // Clear the VIN
        carstate.vin[0] = 'n'; //"na"
		carstate.vin[1] = 'a';
		return;
	}

	// Last 8 chars of VIN
    memcpy(carstate.vin + 9, msg, 8);
    carstate.vin[17] = '\0'; // Null-terminate the VIN
}

// --- Defines based on PSACANBridge/YAML for 0x14C/0x28C ---
// Using 0x28C as the source based on log analysis
#define ID_0x28C_SPEED_BYTE_MSB     0
#define ID_0x28C_SPEED_BYTE_LSB     1
#define ID_0x28C_ODO_BYTE_1         1 // MSB of Odometer
#define ID_0x28C_ODO_BYTE_2         2 // Mid Byte
#define ID_0x28C_ODO_BYTE_3         3 // LSB of Odometer


// Handler for 0x28C (Primary source for Speed, potential Odometer)
static void peugeot_407_ms_14C_speed_odo_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        // Reset states on timeout
        carstate.speed = 0;
        // Keep last known odometer? Or reset? Resetting might be confusing.
        // Let's keep the last known value unless it was never valid.
        // if (carstate.odometer == 0) carstate.odometer = 0; // Or some invalid marker
        return;
    }

    // --- Decode Vehicle Speed ---
    // Reads msg[0] (MSB) and msg[1] (LSB)
    uint16_t speed_raw = get_be16(&msg[ID_0x28C_SPEED_BYTE_MSB]);
    // Apply scaling factor 0.01
    carstate.speed = (uint16_t)((float)speed_raw * 0.01f);

    // --- Decode Odometer ---
    // Reads msg[1] (MSB), msg[2] (Mid), msg[3] (LSB)
    // Note: Requires confirmation with logs showing odometer changes.
    carstate.odometer = get_be24(&msg[ID_0x28C_ODO_BYTE_1]);
}

static void peugeot_407_ms_131_doors_fuel_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        return;
    }

	carstate.fl_door = (msg[1] & 0x01) ? 1 : 0; // Example: Bit 0 of Byte 1 for FL door
    carstate.fr_door = (msg[1] & 0x02) ? 1 : 0; // Example: Bit 1 of Byte 1 for FR door
    carstate.rl_door = (msg[1] & 0x04) ? 1 : 0; // Example: Bit 2 of Byte 1 for RL door
    carstate.rr_door = (msg[1] & 0x08) ? 1 : 0; // Example: Bit 3 of Byte 1 for RR door
}


// --- Defines based on PSACANBridge analysis for 0x168 ---
#define ID_0x168_TEMP_BYTE          0
#define ID_0x168_VOLTAGE_BYTE       1
#define ID_0x168_INVALID_TEMP_RAW   0xFF // Common invalid value, check logs if different
#define ID_0x168_INVALID_VOLT_RAW   0xFF // Common invalid value

// Ambient temp often has a wider valid range than coolant/oil
#define AMBIENT_TEMP_MIN -40
#define AMBIENT_TEMP_MAX 87 // Based on Hiworld protocol range derived from +40 offset

// Voltage range check
#define VOLTAGE_MIN_V   5.0f // Minimum plausible voltage from formula (RAW=0)
#define VOLTAGE_MAX_V  17.75f // Maximum plausible voltage from formula (RAW=255)

static void peugeot_407_ms_168_temp_battery_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        return;
    }

    // --- Decode Battery Voltage ---
    // Using formula from PSACANBridge: (RAW * 0.05) + 5.0
    uint8_t voltage_raw = msg[ID_0x168_VOLTAGE_BYTE];
    if (voltage_raw != ID_0x168_INVALID_VOLT_RAW) { // Check for invalid raw value
        float voltage_calculated = ((float)voltage_raw * 0.05f) + 5.0f;

        // Optional: Range check
        // if (voltage_calculated >= VOLTAGE_MIN_V && voltage_calculated <= VOLTAGE_MAX_V) {
            // Store voltage scaled * 100 (e.g., 12.5V stored as 1250)
            carstate.voltage = (uint32_t)(voltage_calculated * 100.0f);

            // Update low_voltage warning state (example threshold 11.8V)
            // if (voltage_calculated < 11.8f) {
            //     carstate.low_voltage = 1;
            // } else {
            //     carstate.low_voltage = 0;
            // }
        // } else { // Handle out-of-range calculated value }
    } else {
        // Handle invalid raw reading - keep last known value? Or set to 0?
         if (carstate.voltage == 0) { // If still initial value
             carstate.voltage = (uint32_t)(VOLTAGE_MIN_V * 100.0f); // Set to min plausible
         }
        // carstate.low_voltage = STATE_UNDEF; // Status unknown if reading invalid
    }
}


// --- Defines based on PSACANBridge code for 0x1D0 ---
#define ID_0x1D0_FAN_BYTE           2
#define ID_0x1D0_FAN_MASK           0x07 // Bits 2-0 for speed 0-7

#define ID_0x1D0_AIRFLOW_BYTE       3
#define ID_0x1D0_AIRFLOW_WIND_MASK  0x04 // Bit 3: Windshield
#define ID_0x1D0_AIRFLOW_MID_MASK   0x01 // Bit 5: Face/Middle
#define ID_0x1D0_AIRFLOW_FLOOR_MASK 0x02 // Bit 6: Floor // Corrected based on typical PSA mapping

#define ID_0x1D0_STATUS_BYTE        4
#define ID_0x1D0_RECIRC_MASK        0x20 // Bit 7: Recirculation
#define ID_0x1D0_AC_AUTO_MASK       0x08 // Bit 3: Auto mode (Needs verification if reliable)

#define ID_0x1D0_TEMP_L_BYTE        5
#define ID_0x1D0_TEMP_R_BYTE        6
#define ID_0x1D0_TEMP_RAW_INVALID   0xFF
#define ID_0x1D0_TEMP_RAW_LO        0x00
#define ID_0x1D0_TEMP_RAW_HI        0x1F // Value corresponding to "HI"

// Helper to decode temperature (matches Hiworld mapping closely, but returns float)
static float decode_psa_temp(uint8_t raw_temp) {
    if (raw_temp == ID_0x1D0_TEMP_RAW_INVALID) return -100.0f; // Indicate invalid
    if (raw_temp == ID_0x1D0_TEMP_RAW_LO) return 13.5f; // Represent LO as slightly below min numeric
    if (raw_temp == ID_0x1D0_TEMP_RAW_HI) return 30.5f; // Represent HI as slightly above max numeric
    // Map 0x01 (14.0) to 0x1E (30.0) - Assuming 0.5 deg steps
    return 14.0f + ((float)(raw_temp - 1) * 0.5f);
}


static void peugeot_407_ms_1D0_climate_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        return;
    }

    // --- Decode Fan Speed ---
    car_air_state.fanspeed = msg[ID_0x1D0_FAN_BYTE] & ID_0x1D0_FAN_MASK; // 0-7

    // --- Decode Airflow Direction ---
    car_air_state.wind   = (msg[ID_0x1D0_AIRFLOW_BYTE] & ID_0x1D0_AIRFLOW_WIND_MASK) ? 1 : 0;
    car_air_state.middle = (msg[ID_0x1D0_AIRFLOW_BYTE] & ID_0x1D0_AIRFLOW_MID_MASK)  ? 1 : 0;
    car_air_state.floor  = (msg[ID_0x1D0_AIRFLOW_BYTE] & ID_0x1D0_AIRFLOW_FLOOR_MASK) ? 1 : 0;

    // --- Decode Status Bits ---
    car_air_state.recycling = (msg[ID_0x1D0_STATUS_BYTE] & ID_0x1D0_RECIRC_MASK)   ? 1 : 0;
    // now use 0x1E3 for that
    //car_air_state.auto_mode = (msg[ID_0x1D0_STATUS_BYTE] & ID_0x1D0_AC_AUTO_MASK)  ? 1 : 0;

    // --- Decode Temperatures ---
    uint8_t temp_l_raw = msg[ID_0x1D0_TEMP_L_BYTE];
    uint8_t temp_r_raw = msg[ID_0x1D0_TEMP_R_BYTE];

    float temp_l_c = decode_psa_temp(temp_l_raw);
    float temp_r_c = decode_psa_temp(temp_r_raw);

    // Store as uint8_t representing the raw value or mapped value for Hiworld?
    // Hiworld mapping (0=LO, 1=14.0,... 32=30.0, 33=HI) seems different from PSA raw.
    // Let's store the RAW PSA value for now, and handle mapping in hiworldpsa.c
     car_air_state.l_temp = (temp_l_raw == ID_0x1D0_TEMP_RAW_INVALID) ? STATE_UNDEF : temp_l_raw;
     car_air_state.r_temp = (temp_r_raw == ID_0x1D0_TEMP_RAW_INVALID) ? STATE_UNDEF : temp_r_raw;

    // Update other potentially missing car_air_state fields based on other messages or defaults if necessary
    // e.g., Dual mode might be from another message or inferred if L/R temps differ significantly.
    // For now, we only decode what's directly in 0x1D0 according to PSACANBridge.
}

#define ID_0x1E3_BYTE0              0
#define ID_0x1E3_AIRFLOW_L_BYTE     4
#define ID_0x1E3_AIRFLOW_R_BYTE     5
// self.dir[0]<<4 # 4 bits: direction, 4 bits unknown // left seat + dual
#define ID_0x1E3_AIRFLOW_WIND_MASK  0x40 // Bit 3: Wind
#define ID_0x1E3_AIRFLOW_MID_MASK   0x10 // Bit 5: Face/Middle
#define ID_0x1E3_AIRFLOW_FLOOR_MASK 0x20 // Bit 6: Floor

#define ID_0x1E3_AC_ON_MASK     0x20
#define ID_0x1E3_AQS_ON_MASK    0x10
#define ID_0x1E3_AUTO_ON_MASK   0x08
#define ID_0x1E3_DUAL_ON_MASK   0x01



// Handler for CAN ID 0x1E3 (Climate Status for EMF Display?)
// Decodes climate settings based on analysis of python simulator code.
// NOTE: This overlaps significantly with 0x1D0. Need to verify which ID
// is the primary source on the target Peugeot 407 and potentially merge logic
// or prioritize one over the other. Raw temperature/direction values might differ.
static void peugeot_407_ms_1E3_climate_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        // Reset relevant car_air_state fields if this message times out
        // Only reset fields definitively sourced ONLY from 0x1E3 if it differs from 0x1D0.
        // For now, let's assume 0x1D0 is primary and don't reset everything here.
        // If 0x1E3 becomes primary, move the resets from 0x1D0's timeout here.
        return;
    }

    // Decode Byte 0 (b1 in python code) - Various Status Flags
    // Python: (recycle<<7 | fan_off<<6 | ac_off<<5 | auto_air<<4 | auto<<3 | hide_fan<<2 | ext_air<<1 | dual)
    // car_air_state.recycling = (msg[0] & 0x80) ? 1 : 0; // Bit 7: Recycle
    // Bit 6: 'fan off' state? (Inferred from self.fan&True<<6) - Redundant if we decode fan speed from byte 6
    // Bit 5: 'ac off' state? (Inferred from ~self.options['auto']&True<<5)
    car_air_state.ac = (msg[ID_0x1E3_BYTE0] & ID_0x1E3_AC_ON_MASK) ? 0 : 1;
    car_air_state.aqs = (msg[ID_0x1E3_BYTE0] & ID_0x1E3_AQS_ON_MASK) ? 1 : 0;
    // Bit 4: 'auto air'? (From self.options['auto']<<4) - Need definition of 'auto air'
    car_air_state.auto_mode = (msg[ID_0x1E3_BYTE0] & ID_0x1E3_AUTO_ON_MASK) ? 1 : 0; // Bit 3: 'auto' (text)
    // Bit 2: 'hide fan'? (Inferred from self.fan<<2) - Likely display logic, not state.
    // Bit 1: 'ext air'? (From self.options['auto']<<1) - Opposite of recycle? Redundant.
    car_air_state.dual = (msg[ID_0x1E3_BYTE0] & ID_0x1E3_DUAL_ON_MASK) ? 1 : 0; // Bit 0: Dual mode

    // Decode Byte 1 (b2 in python code) - Front Defrost
    // Python: self.options['unfrost_front']<<7
    // Assuming 'unfrost_front' corresponds to airflow windshield direction:

    // --- Decode Airflow Direction ---
    car_air_state.l_wind   = (msg[ID_0x1E3_AIRFLOW_L_BYTE] & ID_0x1E3_AIRFLOW_WIND_MASK) ? 1 : 0;
    car_air_state.l_middle = (msg[ID_0x1E3_AIRFLOW_L_BYTE] & ID_0x1E3_AIRFLOW_MID_MASK)  ? 1 : 0;
    car_air_state.l_floor  = (msg[ID_0x1E3_AIRFLOW_L_BYTE] & ID_0x1E3_AIRFLOW_FLOOR_MASK) ? 1 : 0;


    car_air_state.r_wind   = (msg[ID_0x1E3_AIRFLOW_R_BYTE] & ID_0x1E3_AIRFLOW_WIND_MASK) ? 1 : 0;
    car_air_state.r_middle = (msg[ID_0x1E3_AIRFLOW_R_BYTE] & ID_0x1E3_AIRFLOW_MID_MASK)  ? 1 : 0;
    car_air_state.r_floor  = (msg[ID_0x1E3_AIRFLOW_R_BYTE] & ID_0x1E3_AIRFLOW_FLOOR_MASK) ? 1 : 0;
    
    // Decode Byte 2 (b3 in python code) - Left Temp + Unknown bits
    // Python: self.bits | self.temps[0]
    // Unknown bits: msg[2] & 0xC0 (Python self.bits) - Purpose unclear, ignore for now.
    car_air_state.l_temp = msg[2] & 0x1F; // Lower 5 bits: Left temperature raw value (Needs mapping like 0x1D0?) - Store raw for now.

    // Decode Byte 3 (b4 in python code) - Right Temp
    // Python: self.temps[1]
    car_air_state.r_temp = msg[3] & 0x1F; // Lower 5 bits?: Right temperature raw value - Store raw for now.

    // Decode Byte 4 (b5 in python code) - Left Air Direction
    // Python: self.dir[0]<<4
    // Assuming upper 4 bits = direction flags (Wind, Mid, Floor)
    //uint8_t left_dir_bits = (msg[4] >> 4) & 0x0F; // Extract upper nibble
    // Map bits to directions (This mapping is a GUESS based on common patterns, VERIFY!)
    // car_air_state.wind = (left_dir_bits & 0x01) ? 1 : 0; // Example: Bit 0 = Wind
    // car_air_state.middle = (left_dir_bits & 0x02) ? 1 : 0; // Example: Bit 1 = Middle
    // car_air_state.floor = (left_dir_bits & 0x04) ? 1 : 0; // Example: Bit 2 = Floor
    // Let's stick with 0x1D0 for direction for now as it's clearer.

    // Decode Byte 6 (b7 in python code) - Fan Speed
    // Python: self.fan
    // Assuming lower 4 bits = fan speed (0-7 or similar?)
    car_air_state.fanspeed = msg[6] & 0x0F; // Lower 4 bits: Fan speed (Needs scaling/mapping if not 0-7)
    // Clamp to 0-7 if our state expects that range
    if (car_air_state.fanspeed > 7) {
        car_air_state.fanspeed = 7;
    }
}


// --- Defines based on PSACANBridge/PSACAN.md code for 0x0E1 ---
#define ID_0x0E1_DISPLAY_ACTIVE_BYTE    5
#define ID_0x0E1_DISPLAY_ACTIVE_MASK    0x02 // Bit 0

#define ID_0x0E1_ZONE_ACTIVE_BYTE       1
#define ID_0x0E1_FRONT_ACTIVE_MASK      0x10 // Bit 4
#define ID_0x0E1_REAR_ACTIVE_MASK       0x40 // Bit 6
// #define ID_0x0E1_BOTH_ACTIVE_MASK    0x20 // Bit 5 - Verify if needed

#define ID_0x0E1_SENSORS_BYTE_A    3
#define ID_0x0E1_SENSORS_BYTE_B    4
#define ID_0x0E1_SENSORS_BYTE_C    5


uint8_t get_radar_center(uint8_t input) {
    // Define the lookup table (LUT) based on the provided mapping
    // Index corresponds to the decimal input value (0-7)
    static const uint8_t radar_center_lut[] = {
        4, // Input 0 (000)
        3, // Input 1 (001)
        3, // Input 2 (010)
        2, // Input 3 (011)
        2, // Input 4 (100)
        1, // Input 5 (101)
        1, // Input 6 (110)
        0  // Input 7 (111)
    };

    // Return the value from the lookup table using the input as the index
    return radar_center_lut[input & 0x07];
}

uint8_t get_radar_side(uint8_t input) {
    // Define the lookup table (LUT) based on the provided mapping
    // Index corresponds to the decimal input value (0-7)
    static const uint8_t radar_side_lut[] = {
        3, // Input 0 (000)
        2, // Input 1 (001)
        2, // Input 2 (010)
        1, // Input 3 (011)
        1, // Input 4 (100)
        0, // Input 5 (101)
        0, // Input 6 (110)
        0  // Input 7 (111)
    };

    // Return the value from the lookup table using the input as the index
    return radar_side_lut[input & 0x07];
}

static void peugeot_407_ms_0E1_parktronic_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    // Define a default inactive state
    const radar_t inactive_radar = {
        .state = e_radar_off,
        .fl = 7, .flm = 7, .frm = 7, .fr = 7, // Use '7' for farthest/inactive
        .rl = 7, .rlm = 7, .rrm = 7, .rr = 7
    };

    if (is_timeout(desc)) {
        return;
    }

    // Check if the Parktronic display should be active
    bool display_active = (msg[ID_0x0E1_DISPLAY_ACTIVE_BYTE] & ID_0x0E1_DISPLAY_ACTIVE_MASK);

    if (!display_active) {
        // If display isn't active, ensure state is off and distances are inactive
        if (carstate.radar.state != e_radar_off) {
             memcpy(&carstate.radar, &inactive_radar, sizeof(radar_t));
        }
        return;
    }

    // --- Decode Sensor Distances (0=Closest, 7=Farthest) ---
    // Rear Sensors
    carstate.radar.rl  = get_radar_side((msg[ID_0x0E1_SENSORS_BYTE_A] >> 5));
    carstate.radar.rlm = get_radar_center((msg[ID_0x0E1_SENSORS_BYTE_A] >> 2));
    carstate.radar.rrm = carstate.radar.rlm;
    carstate.radar.rr  = get_radar_side((msg[ID_0x0E1_SENSORS_BYTE_B] >> 5));

    // Front Sensors
    carstate.radar.fl  = get_radar_side((msg[ID_0x0E1_SENSORS_BYTE_B] >> 2));
    carstate.radar.flm = get_radar_center((msg[ID_0x0E1_SENSORS_BYTE_C] >> 5));
    carstate.radar.frm = carstate.radar.flm;
    carstate.radar.fr  = get_radar_side((msg[ID_0x0E1_SENSORS_BYTE_C] >> 2));


    // --- Determine Active Zones ---
    bool front_active = (
        carstate.radar.fl != 7 || 
        carstate.radar.flm != 7 || 
        carstate.radar.frm != 7 || 
        carstate.radar.fr != 7);
    bool rear_active = (
        carstate.radar.rl != 7 || 
        carstate.radar.rlm != 7 || 
        carstate.radar.rrm != 7 || 
        carstate.radar.rr != 7);

    if (front_active && rear_active) {
        carstate.radar.state = e_radar_on; // Both active
    } else if (front_active) {
        carstate.radar.state = e_radar_on_front;
    } else if (rear_active) {
        carstate.radar.state = e_radar_on_rear;
    } else {
        // If display is active but neither zone flag is set? Default to OFF or a specific state?
        // Let's assume OFF if no zone is explicitly active, even if display_active is true.
            memcpy(&carstate.radar, &inactive_radar, sizeof(radar_t));
            return; // Exit if no zones active
    }


}

// --- Defines based on PSACANBridge code for 0x161 ---
#define ID_0x161_OIL_TEMP_BYTE      2
#define ID_0x161_FUEL_BYTE     3
#define ID_0x161_OIL_TEMP_INVALID   0xFF // Value observed indicating invalid/not ready temp

static void peugeot_407_ms_161_temp_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        return;
    }

    // --- Decode Oil Temperature ---
    uint8_t oil_temp_raw = msg[ID_0x161_OIL_TEMP_BYTE];
    if (oil_temp_raw != ID_0x161_OIL_TEMP_INVALID) {
        // Apply offset: Temp = RAW + 40
        carstate.oil_temp = (int16_t)oil_temp_raw - 40;
    } else {
        // Keep previous value or set to a specific "invalid" marker?
        // Let's keep the previous valid value unless it's the initial state.
        if (carstate.oil_temp == 0) { // Check if it's still the initial value
             carstate.oil_temp = -40; // Set to minimum possible value if still initial
        }
        // If it had a valid value before, just keep it during brief invalid readings.
    }

    // --- Calculate Fuel Level Percentage ---
    uint8_t fuel_raw = msg[ID_0x161_FUEL_BYTE];
    carstate.fuel_lvl = fuel_raw;
}

#define ID_0x_220_DOOR_BYTE     0
#define ID_0x_220_FL_DOOR_MASK  0x80 // Bit 7
#define ID_0x_220_FR_DOOR_MASK  0x40 // Bit 6
#define ID_0x_220_RL_DOOR_MASK  0x20 // Bit 5
#define ID_0x_220_RR_DOOR_MASK  0x10 // Bit 4
#define ID_0x_220_TAILGATE_MASK 0x8  // Bit 2

// Handler for CAN ID 0x220 (Alternative Door Status?)
// NOTE: Documentation for 0x220 bit layout is ambiguous (PSACAN.md).
// This implementation ASSUMES a layout similar to 0x131 (Byte 1, Bits 0-5)
// for FL, FR, RL, RR, Bonnet, Tailgate respectively, as this is a common PSA pattern.
// THIS REQUIRES VERIFICATION WITH ACTUAL CAN LOGS for the Peugeot 407.
// The 0x131 handler is currently considered the primary verified source for door status.
static void peugeot_407_ms_220_door_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        return;
    }

    //Byte 0 holds the door status bits
    uint8_t door_byte = msg[ID_0x_220_DOOR_BYTE];

    // Update carstate. Since 0x131 is already doing this reliably,
    // you might comment these out unless logs show 0x220 is needed or
    // provides different information (e.g., locking status in other bits).
    // For now, we'll update, potentially overwriting 0x131's update if 0x220 arrives later.
    carstate.fl_door = (door_byte & ID_0x_220_FL_DOOR_MASK) ? 1 : 0;
    carstate.fr_door = (door_byte & ID_0x_220_FR_DOOR_MASK) ? 1 : 0;
    carstate.rl_door = (door_byte & ID_0x_220_RL_DOOR_MASK) ? 1 : 0;
    carstate.rr_door = (door_byte & ID_0x_220_RR_DOOR_MASK) ? 1 : 0;
    // Bonnet may be read from warning messages
    //carstate.bonnet = (door_byte & 0x8) ? 1 : 0;   // ASSUMPTION for bit 4 - VERIFY
    carstate.tailgate = (door_byte & ID_0x_220_TAILGATE_MASK) ? 1 : 0; // ASSUMPTION for bit 5 - VERIFY
}

// --- Defines based on PSACAN.md/PSACANBridge for 0x221 ---
#define ID_0x221_CONS_BYTE_MSB       1
#define ID_0x221_RANGE_BYTE_MSB      3
#define ID_0x221_INV_BYTE       0
#define ID_0x221_INV_CONS_MASK      0x80 // Bit 0
#define ID_0x221_INV_RANGE_MASK     0x40 // Bit 1

static void peugeot_407_ms_221_trip_inst_handler(const uint8_t * msg, struct msg_desc_t * desc)
{
    if (is_timeout(desc)) {
        carstate.inst_consumption_raw = 0;
        carstate.range_km = 0;
        return;
    }

    // --- Decode Instantaneous Consumption ---
    uint8_t  inst_consumption_invalid = (msg[ID_0x221_INV_BYTE] & ID_0x221_INV_CONS_MASK);
    if (!inst_consumption_invalid) {
        // Read raw Big Endian value. Scaling factor needs verification (e.g., /10 for L/100km?)
        carstate.inst_consumption_raw = get_be16(&msg[ID_0x221_CONS_BYTE_MSB]);
        // Example scaling (VERIFY THIS!): carstate.inst_consumption_L100km = (float)carstate.inst_consumption_raw / 10.0f;
    } else {
        carstate.inst_consumption_raw = 0xFFFF; // Indicate invalid with max value?
    }

    // --- Decode Range (DTE) ---
    uint8_t  range_invalid = (msg[ID_0x221_INV_BYTE] & ID_0x221_INV_RANGE_MASK);
    if (!range_invalid) {
        // Read raw Big Endian value. Units likely km.
        carstate.range_km = get_be16(&msg[ID_0x221_RANGE_BYTE_MSB]);
    } else {
        carstate.range_km = 0xFFFF; // Indicate invalid
    }
}


// --- Confirmed Byte Indices (based on C4 B7 structure) ---
#define TRIP_AVG_SPEED_BYTE      0 // km/h
#define TRIP_DIST_BYTE_MSB       1 // km (High Byte)
#define TRIP_AVG_CONS_BYTE_MSB   3 // L/100km * 10 (High Byte)

// Handler for 0x2A1 (Trip 1 Data)
static void peugeot_407_ms_2A1_trip1_handler(const uint8_t * msg, struct msg_desc_t * desc) {
    if (is_timeout(desc)) {
        // Reset Trip 1 values if message stops
        carstate.avg_speed1 = 0;
        carstate.avg_consumption1_raw = 0xFFFF; // Indicate invalid
        carstate.trip_distance1 = 0;
        return;
    }

    // Decode values based on confirmed structure
    carstate.avg_speed1 = msg[TRIP_AVG_SPEED_BYTE]; // km/h

    // Distance skip first 2 bytes
    carstate.trip_distance1 = get_be16(&msg[TRIP_DIST_BYTE_MSB]); 

    // Average Consumption (Raw L/100km * 10) - Assuming Big Endian
    carstate.avg_consumption1_raw = get_be16(&msg[TRIP_AVG_CONS_BYTE_MSB]); 
    
}

// Handler for 0x261 (Trip 2 Data)
static void peugeot_407_ms_261_trip2_handler(const uint8_t * msg, struct msg_desc_t * desc) {
    if (is_timeout(desc)) {
        // Reset Trip 2 values if message stops
        carstate.avg_speed2 = 0;
        carstate.avg_consumption2_raw = 0xFFFF; // Indicate invalid
        carstate.trip_distance2 = 0;
        return;
    }

    // Decode values based on confirmed structure
    carstate.avg_speed2 = msg[TRIP_AVG_SPEED_BYTE]; // km/h

    // Distance skip first 2 bytes
    carstate.trip_distance2 = get_be16(&msg[TRIP_DIST_BYTE_MSB]); 

    // Average Consumption (Raw L/100km * 10) - Assuming Big Endian
    carstate.avg_consumption2_raw = get_be16(&msg[TRIP_AVG_CONS_BYTE_MSB]); 
}


static struct msg_desc_t peugeot_407_ms[] =
{
    { 0x36,    100, 0, 0, peugeot_407_ms_036_ign_light_handler },
    { 0x0B6,    50, 0, 0, peugeot_407_ms_0B6_engine_status_handler },
    { 0x0F6,    100, 0, 0, peugeot_407_ms_0F6_status_handler }, 
    { 0x128,    100, 0, 0, peugeot_407_ms_128_lights_handler },
    { 0x220,    100, 0, 0, peugeot_407_ms_220_door_handler },
    { 0x21F,    100, 0, 0, peugeot_407_ms_21F_swc_handler },

    { 0x336,   1000, 0, 0, peugeot_407_ms_vin_336_handler },
    { 0x3B6,   1000, 0, 0, peugeot_407_ms_vin_3B6_handler },
    { 0x2B6,   1000, 0, 0, peugeot_407_ms_vin_2B6_handler },

    { 0x0E1,    100, 0, 0, peugeot_407_ms_0E1_parktronic_handler },
    { 0x161,    100, 0, 0, peugeot_407_ms_161_temp_handler },


    //{ 0x14C,    100, 0, 0, peugeot_407_ms_14C_speed_odo_handler },
    //{ 0x168,   1000, 0, 0, peugeot_407_ms_168_temp_battery_handler },

    { 0x1D0,    100, 0, 0, peugeot_407_ms_1D0_climate_handler },
    { 0x1E3,    100, 0, 0, peugeot_407_ms_1E3_climate_handler },

    { 0x221,    500, 0, 0, peugeot_407_ms_221_trip_inst_handler },
    { 0x2A1,    500, 0, 0, peugeot_407_ms_2A1_trip1_handler }, 
    { 0x261,    500, 0, 0, peugeot_407_ms_261_trip2_handler },
    
    // Add more message descriptors here as you identify them
};
