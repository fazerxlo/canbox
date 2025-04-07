# Raise VW MQB CANbox Protocol: Handling Host-to-Slave Commands

**Document Version:** 1.0

## 1. Introduction

This document details the recommended handling strategy for serial commands received **from** the Android Head Unit (Host) **by** the `canbox-renode` firmware (Slave) when using the **Raise VW MQB protocol** (`0x2E` Header, 38400 baud).

The focus is on implementing the command processing logic within the context of a **CAN Bus Proxy**. The primary function of the firmware in this role is to listen to the vehicle's CAN bus (MQB Comfort CAN, typically 500kbps, though the proxy listens on the *appropriate* bus interface) and translate relevant vehicle status into the Raise VW MQB serial protocol for the head unit. Actively controlling vehicle functions based on Head Unit requests is generally outside this scope due to complexity and safety considerations.

## 2. Protocol Recap

*   **Header:** `0x2E`
*   **Baud Rate:** 38400 bps
*   **Frame Structure:** `HEADER (0x2E) | DATATYPE (1) | LENGTH (1) | DATA (n) | CHECKSUM (1)`
*   **Length (`LEN`):** Specifies the number of **DATA bytes only** (`n`).
*   **Checksum (`CS`):** `(DataType + Length + SUM(DATA bytes)) XOR 0xFF` (8-bit calculation).

## 3. General Handling Approach for a Proxy

When receiving commands from the Host (Android HU), the `canbox-renode` firmware acting as a proxy should follow these steps:

1.  **Receive and Parse:** Implement the state machine (likely within `canbox_cmd_process` or a dedicated function called by it, e.g., `canbox_raise_vw_mqb_cmd_process`) to correctly receive frames starting with `0x2E`, validate the `LENGTH` byte against received data, and verify the `CHECKSUM`.
2.  **Identify DataType:** Determine the `DataType` (command ID) of the received valid frame.
3.  **Default Action: Ignore Payload:** For almost all commands initiated by the Host, the payload data should be **ignored**. The CANbox proxy typically lacks the logic and safety mechanisms to reliably translate these HU requests into CAN bus messages that modify vehicle behavior (e.g., changing settings, controlling AC, setting time).
4.  **Handling HU Status (`0x81`):** This message informs the CANbox about the HU's state. While the CANbox *could* use this (e.g., to reduce activity when the HU is off), for basic proxy functionality, ignoring it is the simplest approach.
5.  **Acknowledgement (ACK/NACK):** The Raise VW MQB protocol **does not use** a general ACK/NACK mechanism at the link layer for Host->Slave commands. The CANbox **should not** send any acknowledgement upon receiving these commands.

## 4. Detailed DataType Handling (Host -> Slave)

*(This logic would reside within the function handling received and validated Raise VW MQB frames)*

---

**DataType `0x81`: Host Status Info (`DATA_TYPE_HOST_STATUS_INFO`)**
*   **Purpose:** Sent by the HU to inform the CANbox about its current status (e.g., power state, mute status, navigation active, phone active).
*   **Payload:** Contains bit flags or values representing the HU's state. Common examples:
    *   Byte 0, Bit 0: Radio Power (1=On)
    *   Byte 0, Bit 1: Mute Status (1=Muted)
    *   Byte 1: Backlight Brightness Level?
    *   Other bytes/bits for Navi/Phone/Media status.
*   **Recommended Handling (Proxy):** **Ignore Payload.** While potentially useful for advanced power management or context switching within the CANbox, it's not necessary for the core proxy function of relaying vehicle CAN data. Log for debugging if needed.

---

**DataType `0x82`: Set Car Info (`DATA_TYPE_SET_CAR_INFO`)**
*   **Purpose:** Sent by the HU to request changes to vehicle settings (e.g., central locking behavior, lighting settings, wiper settings, unit preferences).
*   **Payload:** Varies greatly depending on the specific setting, often involves an identifier for the setting and the desired value.
*   **Recommended Handling (Proxy):** **Ignore Payload.** Translating these requests into the correct MQB CAN messages to configure ECUs (like the BCM/Gateway) is complex, vehicle-variant dependent, and carries risks if done incorrectly. This is beyond the proxy's scope.

---

**DataType `0x84`: Time Set (`DATA_TYPE_TIME_SET`)**
*   **Purpose:** Sent by the HU to synchronize the vehicle's instrument cluster clock.
*   **Payload:** Contains time and date information (Year, Month, Day, Hour, Minute, potentially Seconds).
*   **Recommended Handling (Proxy):** **Ignore Payload.** Setting the instrument cluster time requires sending specific CAN messages, often with precise formatting, which is outside the proxy's role. The vehicle clock usually syncs via GPS or its own internal mechanisms.

---

**(Other Potential Host->Slave DataTypes)**
*   If other `0x8x` or `0xAx` commands are identified (e.g., from Java code analysis or specific HU documentation), they should generally be handled by **Ignoring the Payload** for the same reasons outlined above (proxy scope, complexity, safety).

---

## 5. Implementation in Firmware

The command processing logic should reside within a function responsible for handling the Raise VW MQB protocol's incoming serial data stream.

```c
// Example structure within protocol/raisemqb.c or similar

// State machine definitions (similar to RZC/Hiworld example)
enum raise_mqb_rx_state {
    RX_RAISEMQB_WAIT_START,
    RX_RAISEMQB_DTYPE,
    RX_RAISEMQB_LEN,
    RX_RAISEMQB_DATA,
    RX_RAISEMQB_CS
};
#define RX_RAISEMQB_BUFFER_SIZE 64 // Adjust as needed

// Function to process a fully received and checksum-validated frame
static void process_received_raise_mqb_command(uint8_t data_type, uint8_t *data_payload, uint8_t data_len) {
    // Optional: Add debug print for received command
    // char dbg_buf[64];
    // snprintf(dbg_buf, sizeof(dbg_buf), "RaiseMQB RX: DType=0x%02X Len=%d\r\n", data_type, data_len);
    // hw_usart_write(hw_usart_get(), (uint8_t*)dbg_buf, strlen(dbg_buf));

    switch (data_type) {
        case 0x81: // DATA_TYPE_HOST_STATUS_INFO
            // Ignore payload for basic proxy.
            // Optionally log received data for debugging.
            break;

        case 0x82: // DATA_TYPE_SET_CAR_INFO
            // Ignore payload. Do NOT attempt to modify car settings.
            break;

        case 0x84: // DATA_TYPE_TIME_SET
            // Ignore payload. Do NOT attempt to set vehicle time.
            break;

        // Add cases for any other identified Host->Slave DataTypes
        // default:
        //     // Optional: Log unknown/unhandled command DataTypes
        //     char unk_buf[64];
        //     snprintf(unk_buf, sizeof(unk_buf), "RaiseMQB RX Unknown DType=0x%02X\r\n", data_type);
        //     hw_usart_write(hw_usart_get(), (uint8_t*)unk_buf, strlen(unk_buf));
        //     break;
    }
    // No ACK/NACK is sent
}


// Main command processing function called from canbox.c or main loop's usart_process
void canbox_raise_vw_mqb_cmd_process(uint8_t ch) {
    static enum raise_mqb_rx_state rx_state = RX_RAISEMQB_WAIT_START;
    static uint8_t rx_buffer[RX_RAISEMQB_BUFFER_SIZE]; // Buffer for DTYPE, LEN, DATA
    static uint8_t rx_dtype = 0;
    static uint8_t rx_len = 0;   // Expected DATA length
    static uint8_t rx_idx = 0;   // Index for DATA part only

    switch (rx_state) {
        case RX_RAISEMQB_WAIT_START:
            if (ch == 0x2E) {
                rx_idx = 0; // Reset buffer index
                rx_state = RX_RAISEMQB_DTYPE;
            }
            break;

        case RX_RAISEMQB_DTYPE:
            rx_dtype = ch;
            rx_buffer[0] = ch; // Store DTYPE for checksum calc later
            rx_state = RX_RAISEMQB_LEN;
            break;

        case RX_RAISEMQB_LEN:
            // Check if length is within reasonable bounds for the buffer
            if (ch > (RX_RAISEMQB_BUFFER_SIZE - 3)) { // -3 for DTYPE, LEN, CS
                 rx_state = RX_RAISEMQB_WAIT_START; // Invalid length
                 break;
            }
            rx_len = ch; // This is the length of DATA payload
            rx_buffer[1] = ch; // Store LEN for checksum calc
            rx_idx = 0; // Reset index for DATA payload
            if (rx_len == 0) { // No data bytes
                rx_state = RX_RAISEMQB_CS;
            } else {
                rx_state = RX_RAISEMQB_DATA;
            }
            break;

        case RX_RAISEMQB_DATA:
            // Store data bytes starting from index 2 (after DTYPE, LEN)
            if (rx_idx < rx_len) {
                 rx_buffer[2 + rx_idx] = ch;
                 rx_idx++;
            }
            if (rx_idx >= rx_len) { // Received all expected data bytes
                 rx_state = RX_RAISEMQB_CS;
            }
            break;

        case RX_RAISEMQB_CS:
            {
                uint8_t received_checksum = ch;
                // Checksum includes DTYPE, LEN, and all DATA bytes
                uint8_t calculated_checksum = 0;
                uint8_t sum = 0;
                sum += rx_buffer[0]; // DataType
                sum += rx_buffer[1]; // Length
                for (uint8_t i = 0; i < rx_len; i++) {
                    sum += rx_buffer[2 + i]; // Data bytes
                }
                calculated_checksum = sum ^ 0xFF;

                if (calculated_checksum == received_checksum) {
                    // Checksum OK - Process command
                    process_received_raise_mqb_command(rx_dtype, &rx_buffer[2], rx_len);
                } else {
                    // Checksum Failed - Optionally log error
                    // char cs_buf[64];
                    // snprintf(cs_buf, sizeof(cs_buf), "RaiseMQB CS Fail: DT=0x%02X L=%d Got=0x%02X Exp=0x%02X\r\n",
                    //          rx_dtype, rx_len, received_checksum, calculated_checksum);
                    // hw_usart_write(hw_usart_get(), (uint8_t*)cs_buf, strlen(cs_buf));
                }
            }
            rx_state = RX_RAISEMQB_WAIT_START; // Reset state machine
            break;

        default:
            rx_state = RX_RAISEMQB_WAIT_START; // Should not happen
            break;
    }
}

```

This document provides a clear strategy and implementation guidance for handling commands *from* the Android Head Unit within the Raise VW MQB protocol, aligning with the safe and practical scope of a CAN bus proxy firmware.