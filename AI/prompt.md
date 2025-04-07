Hello! I'd like to continue working on the `canbox-renode` firmware project.

**Project Goal:** To create a functional open-source firmware for STM32F1-based CAN boxes (like the Volvo OD2 adapter) acting as a proxy between a Peugeot 407 (2008MY, PF2 Platform, CAN Comfort @ 125kbps) and a generic Android Head Unit (QF001/ROCO K706, UIS7862a/s).

**Target Head Unit Protocol:** The *ultimate target* protocol for the Android Head Unit is **Raise VW MQB**. The target serial baud rate for this protocol is **38400** bps. However, we will first finalize and test the RZC PSA protocol.

**Current Status (as of last session):**

*   We have implemented CAN handlers in `cars/peugeot_407.c` for the following verified CAN IDs: `0x036`, `0x0F6`, `0x131`, `0x21F`, `0x161`, `0x0E1`, `0x168`, `0x28C`, `0x2A1`, `0x261`.
*   We have implemented parts of the **RZC PSA** serial protocol (`protocol/raiserzc.c`) to send basic vehicle status (doors, basic lights, temp, voltage, some trip data) at 19200 baud.
*   We identified discrepancies in the RZC PSA implementation regarding Temperature mapping (`0x36`), frame Length bytes (`0x02`, `0x29`, `0x32`), and Trip Computer scaling (`0x33-0x35`).
*   We identified the need to verify the CAN source for Park Brake state and potentially TPMS status for the Peugeot 407.
*   The development environment uses Renode for simulation/debugging, PlatformIO/Make for building, and `tio`/`cansend`/`candump`/`logger.py` for interaction/logging.
*   We have a workbench setup with the physical CAN box, factory radio+CDC, target Android head unit, and CAN interface.

**Attached:**

*   A ZIP file containing the current state of the `canbox-renode` project repository.
*   Condensed CAN/Serial logs (`can_serial_log_condensed.txt`, `can_serial_log_20250405_190502_condensed.csv`) from previous testing sessions.

**Development Plan:**

Our focus moving forward involves several stages:

1.  **Finalize RZC PSA Implementation:** Complete the implementation of the **RZC PSA** protocol (`protocol/raiserzc.c`) based on our recent log analysis and documentation updates. This includes:
    *   Correcting Temperature mapping (`0x36`: `Data0 = Temp_C + 68`).
    *   Using correct `LEN` bytes for *sending* messages (`0x02`:4, `0x29`:3, `0x32`:8, `0x33`:12, `0x34`/`0x35`:7, `0x36`:2, `0x38`:7).
    *   Confirming/using the correct Header byte (`FD` or `DF`).
    *   Implementing verified Trip Computer scaling (Cons\*10, Speed km/h, Dist km).
2.  **Workbench Verification (RZC PSA):** Thoroughly test the RZC PSA serial output (`FD`/`DF` header, 19200 baud) from the `canbox-renode` firmware on the workbench, comparing it against expected values based on CAN logs and testing interaction with the target Android Head Unit (if it supports RZC).
3.  **Implement Basic CDC Emulation (TBD):** Add functionality to emulate essential CD Changer CAN messages (e.g., `0x162`, `0x1E2`) and handle factory radio commands (`0x131`) to allow audio integration via the factory radio's CDC input. Define serial communication method for Android HU -> CANbox media status updates.
4.  **Workbench Verification (CDC Emu):** Test the CDC emulation by connecting the CANbox to the factory radio (RTx) on the bench and observing interaction/audio playback.
5.  **Robot Tests (PSA Decoding):** Develop Robot Framework tests to verify Peugeot 407 CAN message decoding (`cars/peugeot_407.c`) by checking `carstate` values via Renode monitor/debug.
6.  **Implement Raise VW MQB Mapping:** Create/modify `protocol/raisemqb.c` to translate the data from the Peugeot 407 (`carstate` struct) into the **Raise VW MQB** serial protocol format (`2E` header, 38400 baud). Document the specific PSA->MQB mappings, including handling for unavailable data (like per-wheel TPMS).
7.  **Robot Tests (Raise VW MQB Output):** Create Robot Framework tests that simulate Peugeot 407 CAN messages and verify the corresponding **Raise VW MQB** serial output from the CANbox is correctly formatted and scaled.
8.  **Workbench Verification (Raise VW MQB):** Test the Raise VW MQB serial output on the workbench with the target Android Head Unit, confirming data display and SWC functionality.

**Task for This Session:**

Let's focus on **Step 1: Finalize RZC PSA Implementation**. Please help me review and modify the code in `protocol/raiserzc.c` and potentially `utils.c` (for `snd_raise_rcz_msg`) to:
*   Implement the corrected temperature mapping for DataType `0x36`.
*   Ensure the `LEN` byte is calculated and sent correctly for Data Types `0x02`, `0x29`, `0x32`, `0x33`, `0x34`, `0x35`, `0x36`, `0x38`.
*   Use the correct Header byte (`FD` unless testing proves `DF` is required by the HU).
*   Implement the verified scaling for Trip Computer data (`0x33`, `0x34`, `0x35`).
*   Review the `raise_rcz_cmd_process` function for handling incoming commands (mostly ignoring payloads as per our strategy).

Please analyze the attached project state and assist with these code modifications.
--- END OF FILE ---