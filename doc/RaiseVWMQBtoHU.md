# Raise VW MQB CANbox Protocol Documentation

**Document Version:** 1.0

## 1. Introduction

This document describes the serial communication protocol commonly used by "Raise" (and similar) CANbus decoders (CANbox) designed for **Volkswagen MQB platform vehicles** (e.g., Golf 7, Passat B8, Tiguan AD, Skoda Octavia III, Seat Leon 5F, Audi A3 8V). It outlines the communication rules between an aftermarket Android head unit (DVD 主机) and the CANbox (协议盒).

This protocol enables the head unit to receive detailed vehicle status information and send basic control commands (primarily via steering wheel button emulation).

**Target Functionality:**

*   Receive basic signals (ACC, Illumination, Reverse, Park Brake).
*   Receive steering wheel control (SWC) signals.
*   Receive door, trunk, bonnet status.
*   Receive climate control (Climatronic) status.
*   Receive parking sensor (OPS/ParkPilot) data.
*   Receive vehicle speed, RPM, various temperatures (coolant, oil, outside), voltage, fuel level.
*   Receive Trip computer data (consumption, range, distance, time, averages).
*   Receive Tyre Pressure Monitoring System (TPMS) data (if equipped and supported).
*   Receive vehicle settings status (e.g., light settings, lock settings).
*   Receive CANbox version information.
*   *Potentially* handle commands *from* the head unit (less common for basic functions).

## 2. Physical Layer

Communication uses a standard **UART** interface.

*   **Baud Rate:** **38400 bps** (Standard for Raise/Hiworld)
*   **Data Bits:** 8
*   **Parity:** None
*   **Stop Bits:** 1
*   **Flow Control:** None
*   **Logic Level:** Typically 3.3V (Verify with specific CANbox hardware).

## 3. Link Layer (Raise Variant)

**3.1 Frame Structure:**

Data is exchanged using frames with the following structure:

`HEADER (1 byte) | DATATYPE (1 byte) | LENGTH (1 byte) | DATA (n bytes) | CHECKSUM (1 byte)`

*   **HEADER:** Fixed value `0x2E`. Indicates the start of a frame.
*   **DATATYPE (`ID`):** 1 byte. Command/Message Identifier, defines the purpose/content of the message.
*   **LENGTH (`LEN`):** 1 byte. Specifies the number of **DATA bytes only** (`n`). Does *not* include Header, DataType, Length, or Checksum bytes.
*   **DATA:** `n` bytes (`Data0` to `Datan`). The actual payload, where `n = LEN`. Structure depends on the `DataType`.
*   **CHECKSUM (`CS`):** 1 byte. Calculated as `(DataType + Length + SUM(DATA bytes)) XOR 0xFF`. Summation is 8-bit arithmetic (modulo 256).

**3.2 Checksum Calculation Example (Raise):**

For a frame sending DataType `0x12` (Doors), Length `0x07` (7 data bytes), Data `00 00 80 00 00 00 00` (FL door open):

1.  Bytes to sum: `DataType (0x12)`, `Length (0x07)`, `Data0 (0x00)`...`Data6 (0x00)`
2.  Sum = `0x12 + 0x07 + 0x00 + 0x00 + 0x80 + 0x00 + 0x00 + 0x00 + 0x00 = 0x99`
3.  Checksum = `Sum XOR 0xFF = 0x99 XOR 0xFF = 0x66`
4.  Full Frame: `2E 12 07 00 00 80 00 00 00 00 66`

**3.3 Hiworld Variant Differences:**

*   **Header:** `0x5A 0xA5` (2 bytes)
*   **Length Byte:** Counts `DataType + Data Bytes`.
*   **Checksum:** `(Length + DataType + SUM(DATA bytes)) - 1` (Modulo 256).

**3.4 Acknowledgement:**

No standard ACK/NACK mechanism is typically used at the link layer for these protocols.

## 4. Application Layer (DataType Definitions)

**Notation:**

*   **Slave:** CAN BUS Decoder (CANbox)
*   **Host:** DVD Navigation Host (Android Head Unit)
*   **Ref:** Common reference name or Java Constant (`RZC_Volkswagen_Golf7_Protocol.java`)

| DataType (Hex) | Direction     | Ref / Java Constant              | Description                     | Priority     | Notes                                       |
| :------------- | :------------ | :------------------------------- | :------------------------------ | :----------- | :------------------------------------------ |
| **`0x12`**     | Slave -> Host | `DATA_TYPE_DOOR_INFO`            | Door/Bonnet/Trunk Status        | **Must-Have**  | See payload details.                        |
| **`0x20`**     | Slave -> Host | `DATA_TYPE_STEERING_WHEEL_KEY`   | Steering Wheel Button Press     | **Must-Have**  | See payload details.                        |
| **`0x21`**     | Slave -> Host | `DATA_TYPE_AIR_CONDITION_INFO`   | Climate Control (AC) Status     | **High**       | See payload details.                        |
| **`0x29`**     | Slave -> Host | `DATA_TYPE_STEERING_WHEEL_ANGLE` | Steering Wheel Angle            | Should-Have  | Used for reverse lines, ESP info.         |
| **`0x41`**     | Slave -> Host | `DATA_TYPE_RADAR_INFO`           | Parking Sensor (OPS) Data       | **High**       | See payload details.                        |
| **`0x42`**     | Slave -> Host | `DATA_TYPE_CAR_INFO`             | Basic Vehicle Info (Speed, RPM) | **High**       | See payload details.                        |
| **`0x43`**     | Slave -> Host | `DATA_TYPE_TRIP_COMPUTER_INFO`   | Trip Computer Data (Page 1)     | Should-Have  | Range, Consumption, Distance, Time.       |
| **`0x44`**     | Slave -> Host | `DATA_TYPE_TRIP_COMPUTER_INFO_2` | Trip Computer Data (Page 2)     | Should-Have  | Long Term Averages.                       |
| **`0x45`**     | Slave -> Host | `DATA_TYPE_TEMP_INFO`            | Temperatures (Outside, Oil)     | Should-Have  | Coolant often included elsewhere (`0x42`?). |
| **`0x46`**     | Slave -> Host | `DATA_TYPE_TIRE_PRESSURE_INFO`   | TPMS Data                       | Should-Have  | If vehicle supports & CANbox decodes.   |
| **`0x47`**     | Slave -> Host | `DATA_TYPE_BASIC_INFO`           | Basic Signals (Reverse, P.Brake)| **Must-Have**  | See payload details.                        |
| **`0x48`**     | Slave -> Host | `DATA_TYPE_CAR_SETING_INFO`      | Vehicle Settings Status         | Could-Have   | e.g., Light settings, Lock settings.    |
| **`0x4A`**     | Slave -> Host | `DATA_TYPE_OIL_INFO`             | Oil Level/Pressure Info         | Could-Have   | Specific sensors needed.                    |
| **`0x4B`**     | Slave -> Host | `DATA_TYPE_MAINTENANCE_INFO`     | Service Interval Info           | Could-Have   |                                             |
| **`0x4D`**     | Slave -> Host | `DATA_TYPE_VOLTAGE_INFO`         | Battery Voltage                 | Should-Have  | Often included in `0x42` as well.         |
| **`0x7F`**     | Slave -> Host | `DATA_TYPE_VERSION_INFO`         | CANbox Firmware Version         | Low          | Debug/Info.                               |
| ---            | ---           | ---                              | ---                             | ---          | ---                                         |
| **`0x81`**     | Host -> Slave | `DATA_TYPE_HOST_STATUS_INFO`     | HU Status (Power, Mute etc)     | Optional     | CANbox might use this.                      |
| **`0x82`**     | Host -> Slave | `DATA_TYPE_SET_CAR_INFO`         | Set Vehicle Options/Settings    | Low          | Unlikely used by basic proxy.             |
| **`0x84`**     | Host -> Slave | `DATA_TYPE_TIME_SET`             | Set Vehicle Time                | Low          | Unlikely used by basic proxy.             |

**4.1 Detailed Payload Descriptions (Slave -> Host - Key Types)**

**DataType `0x12`: Door Info (`DATA_TYPE_DOOR_INFO`)**
*   `Length`: `0x07`
*   `Data0-1`: Reserved (`0x00`)
*   `Data2`: Door Status Bits
    *   Bit 7: Front Left (1=Open)
    *   Bit 6: Front Right (1=Open)
    *   Bit 5: Rear Left (1=Open)
    *   Bit 4: Rear Right (1=Open)
    *   Bit 3: Trunk (1=Open)
    *   Bit 2: Bonnet (Hood) (1=Open)
*   `Data3-6`: Reserved (`0x00`)

**DataType `0x20`: Steering Wheel Key (`DATA_TYPE_STEERING_WHEEL_KEY`)**
*   `Length`: `0x02`
*   `Data0`: Key Code (See table below)
*   `Data1`: Key Status (`0x01`=Pressed, `0x00`=Released, `0x02`=Long Press Start, `0x03`=Long Press Repeat?)

    **Common Key Codes (Data0):**
    *   `0x01`: Vol+
    *   `0x02`: Vol-
    *   `0x03`: Next Track / Seek+
    *   `0x04`: Prev Track / Seek-
    *   `0x09`: Phone / OK / Mute (Varies)
    *   `0x0A`: Mode / Source
    *   `0x0C`: Voice Command (MICI)
    *   *(Others exist for specific wheel types)*

**DataType `0x21`: Air Condition Info (`DATA_TYPE_AIR_CONDITION_INFO`)**
*   `Length`: `0x05`
*   `Data0`: Status Bits 1
    *   Bit 7: Power/Full OFF (1=Full System OFF?)
    *   Bit 6: AC Compressor (1=ON)
    *   Bit 5: Recirculation (1=ON)
    *   Bit 4: Max Recirc? (AQS?)
    *   Bit 3: Auto Mode (1=ON)
    *   Bit 2: Dual Mode (1=ON)
    *   Bit 1: AC MAX (1=ON)
    *   Bit 0: Rear Defrost (1=ON)
*   `Data1`: Status Bits 2 & Fan Speed
    *   Bit 7: Airflow Windshield
    *   Bit 6: Airflow Face/Mid
    *   Bit 5: Airflow Floor
    *   Bits 3-0: Fan Speed (0=Off, 1-7=Speed)
*   `Data2`: Left Temperature Setting (See Temp Table)
*   `Data3`: Right Temperature Setting (See Temp Table)
*   `Data4`: Seat Heating/Ventilation (Bits vary by car model)
    *   Bits 7-6: Unknown/Reserved
    *   Bits 5-4: Left Seat Heat Level (0-3)
    *   Bits 3-2: Unknown/Reserved
    *   Bits 1-0: Right Seat Heat Level (0-3)

    **Temperature Table (Data2, Data3):** (Common VW Mapping)
    *   `0x00`: OFF
    *   `0x01`: LO
    *   `0x02` - `0x1F`: Approx 16.0°C to 29.5°C in 0.5° steps. `Value = 15.5 + (Raw * 0.5)`
    *   `0xFE`: HI
    *   `0xFF`: Invalid/Sync

**DataType `0x29`: Steering Wheel Angle (`DATA_TYPE_STEERING_WHEEL_ANGLE`)**
*   `Length`: `0x02`
*   `Data0`: Angle Low Byte
*   `Data1`: Angle High Byte
    *   Value is Signed 16-bit integer (`(Data1 << 8) | Data0`).
    *   Scaling: Degrees * 10 (e.g., 900 = 90.0 degrees). Left is Negative, Right is Positive. Range approx -7800 to +7800.

**DataType `0x41`: Radar Info (`DATA_TYPE_RADAR_INFO`)**
*   `Length`: `0x0C` (12 bytes)
*   `Data0`: Rear Left Outer Sensor Distance (0-255, `0xFF`=Inactive/Max)
*   `Data1`: Rear Left Inner Sensor Distance
*   `Data2`: Rear Right Inner Sensor Distance
*   `Data3`: Rear Right Outer Sensor Distance
*   `Data4`: Front Right Outer Sensor Distance
*   `Data5`: Front Right Inner Sensor Distance
*   `Data6`: Front Left Inner Sensor Distance
*   `Data7`: Front Left Outer Sensor Distance
*   `Data8-10`: Reserved/Unknown (`0x00`)
*   `Data11`: System Status (`0x00`=Off, `0x10`=Rear Active, `0x20`=Front Active, `0x30`=Both Active)
    *   **Note:** Distance unit/scaling needs CAN verification (often cm, but raw value might need scaling/offset).

**DataType `0x42`: Basic Vehicle Info (`DATA_TYPE_CAR_INFO`)**
*   `Length`: `0x0C` (12 bytes)
*   `Data0-1`: Vehicle Speed (km/h * 100, Big Endian)
*   `Data2-3`: Engine RPM (RPM, Big Endian)
*   `Data4`: Outside Temperature (`Value = (Raw * 0.5) - 40` °C)
*   `Data5`: Coolant Temperature (`Value = Raw - 40` °C)
*   `Data6`: Fuel Level (%)
*   `Data7-11`: Reserved/Unknown (`0x00`)

**DataType `0x43`: Trip Computer Page 1 (`DATA_TYPE_TRIP_COMPUTER_INFO`)**
*   `Length`: `0x0B` (11 bytes)
*   `Data0`: Page Identifier (`0x01` for Page 1)
*   `Data1-2`: Range (km, Big Endian)
*   `Data3-4`: Current Fuel Consumption (L/100km or other unit * 10? Big Endian - **Verify Scale/Unit**)
*   `Data5-6`: Average Fuel Consumption (L/100km or other unit * 10? Big Endian - **Verify Scale/Unit**)
*   `Data7-8`: Average Speed (km/h, Big Endian)
*   `Data9-10`: Driving Time (Minutes, Big Endian)

**DataType `0x44`: Trip Computer Page 2 (`DATA_TYPE_TRIP_COMPUTER_INFO_2`)**
*   `Length`: `0x0B` (11 bytes)
*   `Data0`: Page Identifier (`0x02` for Page 2)
*   `Data1-2`: Distance Traveled (km, Big Endian)
*   `Data3-4`: Average Fuel Consumption (Long Term) (L/100km or other unit * 10? BE - **Verify**)
*   `Data5-6`: Average Speed (Long Term) (km/h, BE)
*   `Data7-8`: Driving Time (Long Term) (Minutes, BE)
*   `Data9-10`: Reserved/Unknown (`0x00`)

**DataType `0x45`: Temperature Info (`DATA_TYPE_TEMP_INFO`)**
*   `Length`: `0x03`
*   `Data0`: Outside Temperature (`Value = (Raw * 0.5) - 40` °C) - Often redundant with `0x42`.
*   `Data1`: Engine Oil Temperature (`Value = Raw - 40` °C, `0xFF`=Invalid)
*   `Data2`: Reserved (`0x00`)

**DataType `0x46`: TPMS Info (`DATA_TYPE_TIRE_PRESSURE_INFO`)**
*   `Length`: `0x08` (Typically)
*   `Data0`: Front Left Pressure
*   `Data1`: Front Right Pressure
*   `Data2`: Rear Left Pressure
*   `Data3`: Rear Right Pressure
*   `Data4`: Front Left Temperature
*   `Data5`: Front Right Temperature
*   `Data6`: Rear Left Temperature
*   `Data7`: Rear Right Temperature
    *   **Note:** Units (kPa, Bar, PSI) and scaling/offset for pressure and temperature vary significantly. Requires CAN verification for the specific TPMS module. Temp often `Raw - 40` or `Raw - 50` °C. Pressure often needs scaling factor and conversion.

**DataType `0x47`: Basic Info (`DATA_TYPE_BASIC_INFO`)**
*   `Length`: `0x02`
*   `Data0`: Status Bits
    *   Bit 3: Seat Belt Warning (1=Unfastened)
    *   Bit 2: Parking Brake (1=Engaged)
    *   Bit 1: Reverse Gear (1=Engaged)
    *   Bit 0: ACC Power (1=ON)
*   `Data1`: Light Status Bits
    *   Bit 1: Headlights (Low Beam) (1=ON)
    *   Bit 0: Parking Lights (Sidelights) (1=ON)

### 5. Implementation Notes for `canbox-renode` (Mapping PSA to MQB)

*   **Protocol Selection:** Use `e_cb_raise_vw_mqb` or `e_cb_hiworld_vw_mqb`. Set UART baud to **38400**.
*   **Mapping Challenge:** The core task is mapping data read from PSA CAN IDs (using `peugeot_407.c` handlers and stored in `carstate`) into the MQB protocol payload structures defined above.
*   **Data Availability:** Not all data expected by the MQB protocol might be available on the PSA PF2 CAN bus (e.g., detailed TPMS per wheel, specific seat heating levels, oil level, certain vehicle settings). Send default/invalid values (`0xFF`, `0x00`) for unavailable data.
*   **Scaling & Units:** Carefully verify and implement the correct scaling and offsets when converting PSA CAN values (e.g., temperature, speed, RPM, consumption) to the format expected by the MQB protocol payload bytes.
*   **Temperature:** Use `car_get_temp()` for Outside Temp (map to `0x42`/`0x45`). Use `carstate.engine_temp` for Coolant Temp (map to `0x42`). Use `carstate.oil_temp` for Oil Temp (map to `0x45`).
*   **Fuel:** Map `carstate.fuel_lvl` (%) to `0x42` Data6.
*   **Trip Data:** Map PSA trip data (after verifying scaling) to `0x43`/`0x44` payloads. Range goes in `0x43`.
*   **TPMS:** If the Peugeot 407 *does* have TPMS data on CAN, identify the ID and format, then map it to the `0x46` payload structure. If not available, send default/invalid values in `0x46`.
*   **AC:** Map data from `car_air_state` (read by `peugeot_407_ms_1D0_handler`) into the `0x21` payload structure, noting potential differences in temp scales or feature availability.

