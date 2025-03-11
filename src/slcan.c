#include "canbox.h"
#include "car.h"
#include "hw_usart.h"  // Assuming your USART functions are here
#include <stdio.h>    // For snprintf (debugging)
#include <string.h>   // For strlen, memcpy
#include "slcan.h"    // Include the slcan header
#include "hw_can.h" // Include to have msg_can_t

// --- SLCAN Protocol Definitions ---
#define SLCAN_START_CHAR 't' // Standard frame start
#define SLCAN_EXT_START_CHAR 'T' // Extended frame start
#define SLCAN_RTR_START_CHAR 'r' //Standard RTR
#define SLCAN_RTR_EXT_START_CHAR 'R' //Extended RTR
#define SLCAN_CR '\r'        // Carriage return (end of command)


#ifdef USE_PEUGEOT_407
#include "cars/peugeot_407.c"
#endif

// --- States for the SLCAN State Machine ---
typedef enum {
    SLCAN_STATE_IDLE,
    SLCAN_STATE_GET_ID,
    SLCAN_STATE_GET_DLC,
    SLCAN_STATE_GET_DATA,
} slcan_rx_state_t;


// --- Function to Process a Complete CAN Message ---
// This function takes a fully decoded CAN message and processes it as if
// it came from the hardware CAN interface.  It's *crucial* that this
// function is called *after* you've fully decoded an SLCAN command.

static void can_process(struct msg_can_t *msg)
{
	// Find the correct handler function
	for (uint32_t j = 0; j < sizeof(peugeot_407_ms) / sizeof(peugeot_407_ms[0]); j++) {
		struct msg_desc_t *desc = &peugeot_407_ms[j];
		if (msg->id == desc->id) {
			if (desc->in_handler) {
				desc->in_handler(msg->data, desc); // Call the handler.
			}
			break;
		}
	}
}

// --- SLCAN Decoder ---
void slcan_process_char(uint8_t ch) {
    static slcan_rx_state_t state = SLCAN_STATE_IDLE;
    static uint8_t rx_buffer[32]; // Buffer for incoming characters
    static uint8_t rx_idx = 0;
    static uint32_t can_id;
    static uint8_t dlc;
    static uint8_t data[8];
    static uint8_t data_idx;
    static uint8_t frame_type; // 't' for standard, 'T' for extended.

    switch (state) {
        case SLCAN_STATE_IDLE:
            if (ch == SLCAN_START_CHAR || ch == SLCAN_EXT_START_CHAR || ch == SLCAN_RTR_START_CHAR || ch == SLCAN_RTR_EXT_START_CHAR) {
                frame_type = ch; // remember is standard or extended frame.
                rx_idx = 0;
                can_id = 0;
                dlc = 0;
                data_idx = 0;
                state = SLCAN_STATE_GET_ID;

            }
            break;

        case SLCAN_STATE_GET_ID:
            // Convert hex char to integer. This is simplified, doesn't handle errors.
            if (ch >= '0' && ch <= '9') {
                can_id = (can_id << 4) | (ch - '0');
            } else if (ch >= 'A' && ch <= 'F') {
                can_id = (can_id << 4) | (ch - 'A' + 10);
            } else if (ch >= 'a' && ch <= 'f') {
                can_id = (can_id << 4) | (ch - 'a' + 10);
            } else {
                // Invalid character, reset.
                state = SLCAN_STATE_IDLE;
                break;
            }

            rx_idx++;
            if ((frame_type == SLCAN_START_CHAR && rx_idx == 3) || (frame_type == SLCAN_EXT_START_CHAR && rx_idx == 8)
                || (frame_type == SLCAN_RTR_START_CHAR && rx_idx == 3) || (frame_type == SLCAN_RTR_EXT_START_CHAR && rx_idx == 8))
             {
                state = SLCAN_STATE_GET_DLC;
                rx_idx = 0; // Reset for next phase.
            }
            break;

        case SLCAN_STATE_GET_DLC:
             if (ch >= '0' && ch <= '8') { // DLC should be between 0 to 8
                  dlc = ch - '0';
                  state = SLCAN_STATE_GET_DATA; // Go to the next state
                  data_idx = 0;
              } else {
                   state = SLCAN_STATE_IDLE; // Error, go back
              }

              break;


        case SLCAN_STATE_GET_DATA:
            // Convert hex chars to integer
            uint8_t cdata; // Variable to keep converted value
            if (ch >= '0' && ch <= '9') {
                cdata = ch - '0';
            } else if (ch >= 'A' && ch <= 'F') {
                cdata = ch - 'A' + 10;
            } else if (ch >= 'a' && ch <= 'f') {
                cdata = ch - 'a' + 10;
            }
			else if(ch == '\r') { //End of frame.
              if(rx_idx != (dlc*2) && frame_type !=  SLCAN_RTR_START_CHAR && frame_type !=  SLCAN_RTR_EXT_START_CHAR ) { //Check length.
                    state = SLCAN_STATE_IDLE;
                    break;
                }

                msg_can_t msg;
                msg.id = can_id;
                msg.len = dlc;
                msg.type = 0; //Set can type.
                if(frame_type == SLCAN_EXT_START_CHAR || frame_type == SLCAN_RTR_EXT_START_CHAR)
                	msg.type = 0x40; // Extended frame.
                if(frame_type == SLCAN_RTR_START_CHAR || frame_type == SLCAN_RTR_EXT_START_CHAR)
                    msg.type |= 0x80; // Set RTR bit if it's rtr frame

                for(int i=0; i<dlc; i++)
                    msg.data[i] = data[i]; // Fill data.

                can_process(&msg); // Send Message
                state = SLCAN_STATE_IDLE; // Go back, ready for the next message.
                break;
            }
            else {
              // Invalid character
               state = SLCAN_STATE_IDLE;
               break;
            }

			//construct the data:
            if (rx_idx % 2 == 0) {
                data[data_idx] = cdata << 4; // High nibble.
            }
            else {
                data[data_idx] |= cdata; // Low nibble
                data_idx++;
            }
            rx_idx++;

            if(data_idx >= dlc && frame_type !=  SLCAN_RTR_START_CHAR && frame_type !=  SLCAN_RTR_EXT_START_CHAR) //Received all data bytes
                state = SLCAN_STATE_IDLE;

            break;
        default: //Always set a default value on state machine.
            state = SLCAN_STATE_IDLE;
            break;

    }
}