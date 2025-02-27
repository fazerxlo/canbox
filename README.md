# CANBUS Adapter Firmware for Android Head Unit Integration (Peugeot 407 Focus)

[![Generic badge](https://img.shields.io/badge/Status-Work%20in%20Progress-yellow.svg)](https://shields.io/)
[![Generic badge](https://img.shields.io/badge/License-GPLv2-blue.svg)](https://shields.io/)

This project provides open-source firmware for generic CANBUS adapters commonly found on AliExpress, enabling enhanced integration between a vehicle's CAN bus and an Android head unit. While initially focused on the Peugeot 407, the modular design allows for adaptation to other car models. This project significantly expands upon the original [smartgauges/canbox](https://github.com/smartgauges/canbox) project, providing more detailed documentation, improved code structure, and support for additional debugging and emulation techniques. The majority of the code presented in this project is generated on top of a fork of [smartgauges/canbox](https://github.com/smartgauges/canbox).

**Project Goals:**

*   Provide a customizable and extensible firmware alternative to proprietary CANBUS adapters.
*   Enable advanced features and data display on Android head units beyond basic functionality.
*   Offer a well-documented and easy-to-understand codebase for hobbyists and developers.
*   Support development and debugging through emulation (QEMU) and direct hardware interaction.
*   Facilitate community contributions for expanding vehicle and protocol support.

**Supported Vehicles (Currently):**

*   Peugeot 407 (Primary Focus) - Requires significant CAN bus data verification.
*   Land Rover Freelander 2 (2007MY, 2013MY) - Inherited from the original project.
*   Volvo XC90 (2007MY) - Inherited from the original project.
*   Volkswagen Tiguan - (Support based on hardware compatibility, CAN data needs verification).
*   Skoda Fabia - (Mentioned in original code, requires verification).
*   Audi Q3 2015 - (Mentioned in original code, requires verification).
*   Toyota Premio 26x -(Mentioned in original code, requires verification).
*   Other vehicles using compatible CANBUS adapters and protocols are *potentially* supported, but require custom configuration and testing.

**Supported CANBUS Protocols (Emulated):**

The firmware *emulates* the following protocols, commonly used by Android head units to communicate with CAN adapters:

*   Raise VW (PQ)
*   Raise VW (MQB)
*   Audi BMW (NBT Evo) - *Note: Likely a typo; should be "Audi, BMW (NBT Evo)"*
*   HiWorld VW (MQB)
* Mercedes-Benz UDS-based CANBUS

**Hardware Compatibility:**

The firmware is designed for CANBUS adapters based on:

*   **STM32F103x8 microcontrollers:** (Common in HiWorld adapters).
*   **Nuvoton NUC131 microcontrollers:** (Common in Raise adapters).

The project includes schematics and PCB layouts (where available) for common adapter types.  You should visually inspect your CAN box and compare it to the provided schematics to ensure compatibility. *Crucially*, identify the microcontroller used in your adapter.

## Features

The firmware provides the following core features:

*   **Extraction of CAN Bus Data:**  Reads and decodes messages from the vehicle's CAN bus.  The specific messages and data formats are defined in vehicle-specific configuration files.
*   **Generation of Standard Signals:**  Creates the standard analog signals required by most Android head units:
    *   **IGN/ACC (Ignition/Accessory):** 12V when the ignition is on.
    *   **ILLUM (Illumination):**  12V for backlight control (often tied to headlights or ambient light).
    *   **PARK (Parking Brake):**  Ground signal when the parking brake is engaged.
    *   **REAR (Reverse Gear):** 12V when reverse gear is selected, with configurable delays.
*   **Emulation of CANBUS Protocols:**  Communicates with the Android head unit using a selected protocol (e.g., Raise VW (PQ), HiWorld VW (MQB)). This allows the head unit to display vehicle information and receive button presses.
*   **Configuration:**  A serial-based command interface (accessible via RX/TX lines) allows you to:
    *   Switch between normal operation and debug mode.
    *   Select the emulated CANBUS protocol.
    *   Select the vehicle model.
    *   Adjust parameters (backlight threshold, camera delay, etc.).
    *   Save the configuration to non-volatile memory (where supported).
    *   Monitor CAN bus traffic (sniffer mode).
*   **Extensibility:** The modular design makes it relatively easy to add support for new vehicles and CANBUS protocols.

## Extensive Debugging and Development Documentation

This project provides extensive documentation and tools for debugging and development:

### CAN Communication Debugging (Car & CAN Box)

This section describes how to debug the communication between the *vehicle's* CAN bus and the CAN box itself. This is crucial for verifying that the firmware is correctly receiving and decoding CAN messages.

1.  **Hardware:**
    *   **USB-to-Serial Adapter:**  A 3.3V-compatible USB-to-serial adapter (FTDI, CP210x, or CH340/CH341 based).  *Do not use a 5V adapter!*
    *   **Wiring:** Connect the adapter's TX to the CAN box's RX, RX to the CAN box's TX, and GND to the CAN box's GND.  *Do not* connect the adapter's VCC to the CAN box. Power the CAN box via its normal vehicle connection (or OBD2 connector).
    *   **OBD2 Connector (Recommended):**  Use the provided OBD2 connector setup for easy connection and disconnection during testing.

2.  **Software (Linux):**
    *   **Terminal Program:** Install a terminal program like `minicom`, `screen`, `picocom`, or `tio`.  `picocom` or `tio` are recommended for their simplicity.
        ```bash
        sudo apt update
        sudo apt install picocom  # Or tio, screen, minicom, etc.
        ```
    *   **Identify Serial Port:**  Find the device file for your USB-to-serial adapter (e.g., `/dev/ttyUSB0`, `/dev/ttyACM0`). Use `dmesg | grep tty` or `ls /dev/tty*` (before and after plugging in the adapter) to identify it.
    *   **Connect:**
        ```bash
        picocom -b 38400 /dev/ttyUSB0  # Replace /dev/ttyUSB0 if necessary
        ```
    *   **Enable Debug Mode:**  Type `OOOOOOOOOOOOO` (at least 10 'O' characters) quickly (within 1 second) after connecting.  You should see debug output.
    *   **Log to File:**
        ```bash
        picocom -b 38400 --logfile can_log.txt /dev/ttyUSB0
        ```
        This will log all communication to `can_log.txt`.

3.  **Interpreting the Output:**
    *   The debug output will show raw CAN messages received, decoded vehicle data (speed, RPM, etc.), and messages sent to the head unit.
    *   Use this information to verify that the CAN IDs and data formats in your `cars/peugeot_407.c` file are correct.
    *   Use the 'm' command in debug mode to cycle through the received CAN messages.
    *   Use the 'S' command to enter sniffer mode, which provides a more detailed view of CAN bus activity.

### Emulating CAN Communication (Linux to CAN Box)

This section describes how to *simulate* CAN messages being sent from the vehicle to the CAN box. This is useful for testing the firmware without needing to connect to a real vehicle.  You can use the same serial connection as above.  *No additional hardware is required.*

1.  **Enter Debug Mode:**  Connect to the CAN box using your terminal program and enter debug mode (`OOOOOOOOOOOOO`).

2.  **Use a Terminal Program (or Script):**  You can manually type hex data into the terminal, or use a script to send pre-defined messages. The firmware *receives* CAN messages, but it *transmits* data to the headunit via UART.

3.  **Simulate CAN Data:** The `car_process()` function in `car.c` uses the `hw_can_get_msg()` function to retrieve CAN messages. You will have to manually enter hex data into the terminal.

### UART Communication Debugging (CAN Box & Head Unit)

This section describes how to debug the communication between the CAN box and the Android head unit.  This uses the same serial connection as the CAN bus debugging, but you'll be focusing on the data sent *to* the head unit (using the emulated "Raise VW" or other protocol).

1.  **Hardware:** Same as for CAN bus debugging.
2.  **Software:** Use your chosen terminal program (`minicom`, `screen`, `picocom`).
3.  **Observe Output:**  The debug output will show the formatted messages being sent to the head unit.  Compare these messages to the expected format for the emulated protocol (Raise VW, etc.).
4.  **Test with Head Unit:** Connect the CAN box to your Android head unit and observe its behavior.  Does it display the correct information? Do the steering wheel controls work?

### Using QEMU for Development and Testing

QEMU allows you to emulate the STM32F1 microcontroller, making it possible to develop and test the firmware without needing the physical hardware. This is particularly useful for initial development and debugging.

1.  **Install QEMU:**

    ```bash
    sudo apt update
    sudo apt install qemu-system-arm
    ```

2.  **Compile for QEMU:**

    ```bash
    make qemu
    ```

3.  **Run in QEMU (Basic):**

    ```bash
    qemu-system-arm -M stm32-p103 -kernel qemu.bin -serial stdio -display none
    ```
     This will run the `qemu.bin` firmware in QEMU. The `-serial stdio` option redirects the serial output (USART) to your terminal.  The `-nographic` option disables the graphical display (since we're only interested in the serial output).

4.  **Run in QEMU (with GDB):**

    ```bash
    qemu-system-arm -M stm32-p103 -kernel qemu.bin -serial stdio -display none -S -s
    ```

    *   `-S`:  Halts the CPU at startup.
    *   `-s`:  Opens a GDB server on port 1234.

    In a *separate* terminal, start GDB:

    ```bash
    arm-none-eabi-gdb qemu.elf
    (gdb) target remote :1234
    (gdb) break main
    (gdb) continue
    ```

    Now you can use standard GDB commands to debug the firmware (set breakpoints, step through code, inspect variables, etc.).

5. **Makefile Targets**:
   The `Makefile_qemu` provides convenient targets:
   * `make qemu.bin`: Builds the firmware for QEMU.
   * `make run_qemu`: Runs the firmware in QEMU with stdio for serial output, and enables GDB support.

### Updating the Firmware (from Linux)

1.  **Hardware:** You'll need an ST-Link V2 programmer (or a compatible clone).
2.  **Wiring:** Connect the ST-Link to the SWD pins on the CAN box (SWDIO, SWCLK, GND, and 3.3V).  *Do not* connect the ST-Link's 5V pin.
3.  **Install OpenOCD:**

    ```bash
    sudo apt install openocd
    ```

4.  **Use the Makefile:**  The provided Makefiles (`Makefile_vw_nc03` and `Makefile_volvo_od2`) include targets for flashing:

    ```bash
    make flash_vw_nc03   # For the VW NC03 adapter
    make flash_volvo_od2  # For the Volvo OD2 adapter
    ```

    These targets use `openocd` with configuration files (`nuc131.cfg` or `stm32f103x8.cfg`) to erase the chip and program the new firmware.

### Debugging the Firmware (from Linux)

1.  **Serial Debugging:** As described above, use a terminal program to connect to the CAN box's UART and enter debug mode.  This allows you to see debug output and change settings.

2.  **GDB (with QEMU):**  See the "Using QEMU" section for instructions on using GDB with QEMU.

3.  **GDB (with Hardware):** You can use `openocd` and `arm-none-eabi-gdb` to debug the firmware running on the *physical* CAN box.

    *   **Start OpenOCD:**  In one terminal, run:

        ```bash
        # For STM32F1 (Volvo OD2):
        openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg

        # For Nuvoton NUC131 (VW NC03):
        openocd -f vw_nc03/fw/nuc131.cfg
        ```
        Or from the project root folder, if you have built the firmware for a particular target:
        ```
        make flash_vw_nc03
        ```
        ```
        make flash_volvo_od2
        ```
        These commands will compile, flash and run the openocd command to connect to the device.

    *   **Start GDB:** In another terminal:

        ```bash
        arm-none-eabi-gdb <firmware_file.elf>  # e.g., vw_nc03.elf
        (gdb) target remote :3333
        (gdb) monitor reset halt
        (gdb) load
        (gdb) continue
        ```

        Replace `<firmware_file.elf>` with the correct `.elf` file (e.g., `vw_nc03.elf` or `volvo_od2.elf`).

    Now you can use GDB to set breakpoints, step through the code, inspect variables, etc., while the firmware is running on the actual hardware.

### Peugeot 407 Custom Integration

1.  **CAN Bus Analysis:**  Use a CAN bus analyzer and the `candump` utility (or the provided `plot.exe` tool on a Windows machine, as described in the original `README.md`) to capture CAN bus data from your Peugeot 407.  Identify the CAN IDs and data formats for the signals you want to use (ignition, speed, doors, lights, etc.).  The `PSA CAN.html` file is a good *starting point*, but you *must* verify the information for your specific vehicle.

2.  **Implement Handlers:**  In `cars/peugeot_407.c`, implement the `peugeot_407_xxx_handler` functions to decode the CAN data and update the `carstate` structure.

3.  **Create `canbox_raise_vw_peugeot_*.c` Functions:**  Create functions in `canbox_raise_vw_peugeot.c` to format the Peugeot data into messages that mimic the "Raise VW" protocol.  This is the most challenging part, as you're essentially translating between two different protocols.

4.  **Test and Iterate:**  Use the debug mode and serial output to test your implementation, making adjustments as needed.

### VS Code Configuration

To use VS Code for development and debugging, you'll need to configure it for C/C++ development, cross-compilation, and debugging with OpenOCD.

1.  **Install Extensions:**
    *   **C/C++ (ms-vscode.cpptools):** Provides C/C++ language support (IntelliSense, code completion, etc.).
    *   **Cortex-Debug (marus25.cortex-debug):**  Provides debugging support for ARM Cortex-M microcontrollers using OpenOCD, GDB, and other debuggers.

2.  **Create a `c_cpp_properties.json` file:**  This file tells VS Code about your project's include paths and compiler settings.  Create a directory called `.vscode` in the root of your project, and inside it, create a file named `c_cpp_properties.json`.  Here's an example, which you'll need to adapt:

    ```json
    {
        "configurations": [
            {
                "name": "STM32F1",
                "includePath": [
                    "${workspaceFolder}/**",
                    "${workspaceFolder}/libopencm3/include",
                    "${workspaceFolder}/libopencm3/lib/stm32/f1"
                ],
                "defines": [
                    "STM32F1",
                    "USE_STDPERIPH_DRIVER", // You might not need this
                    "USE_PEUGEOT_407"      // Add this for Peugeot 407 support
                ],
                "compilerPath": "/usr/bin/arm-none-eabi-gcc", // Adjust to your compiler path
                "cStandard": "c11",
                "cppStandard": "c++17",
                "intelliSenseMode": "gcc-arm"
            },
            {
                "name": "NUC131",
                "includePath": [
                    "${workspaceFolder}/**",
                    "${workspaceFolder}/vw_nc03/fw/nuc131bsp/Library/CMSIS/Include",
                    "${workspaceFolder}/vw_nc03/fw/nuc131bsp/Library/Device/Nuvoton/NUC131/Include",
                    "${workspaceFolder}/vw_nc03/fw/nuc131bsp/Library/StdDriver/inc"
                ],
                "defines": [
                    "NUC131",
                    "USE_STDPERIPH_DRIVER", // You might not need this
                    "USE_PEUGEOT_407"      // Add this for Peugeot 407 support
                ],
                "compilerPath": "/usr/bin/arm-none-eabi-gcc", // Adjust to your compiler path
                "cStandard": "c11",
                "cppStandard": "c++17",
                "intelliSenseMode": "gcc-arm"
            }
        ],
        "version": 4
    }
    ```

    *   **`includePath`:**  This tells VS Code where to find header files.  You'll need to adjust these paths to match your project structure.  The example above includes the necessary paths for both the STM32F1 and NUC131 targets.
    *   **`defines`:**  These are preprocessor definitions that are used during compilation.  `STM32F1` or `NUC131` should be defined based on your target. `USE_PEUGEOT_407` should also be defined.
    *   **`compilerPath`:**  This should point to your ARM cross-compiler (e.g., `arm-none-eabi-gcc`).  Make sure this path is correct.
    * **`cStandard`**, **`cppStandard`** and **`intelliSenseMode`**: Set to appropriate values

3.  **Create a `launch.json` file:** This file configures VS Code's debugger.  Create a `launch.json` file in the `.vscode` directory.  Here are examples for both the STM32F1 (using QEMU and ST-Link) and the NUC131:

    **STM32F1 (QEMU):**

    ```json
    {
        "version": "0.2.0",
        "configurations": [
            {
                "name": "Debug (QEMU)",
                "type": "cortex-debug",
                "request": "launch",
                "servertype": "openocd",
                "executable": "${workspaceFolder}/qemu.elf", // Path to your compiled ELF file
                "configFiles": [
                    "interface/stlink-v2.cfg",
                    "target/stm32f1x.cfg" // Or the appropriate STM32 config file
                ],
                "svdFile": "${workspaceFolder}/libopencm3/lib/stm32/f1/NUC131.svd",
                "runToEntryPoint": "main",
                "preLaunchTask": "build_qemu", // Add this line, assuming you have a build task
                "showDevDebugOutput": "both",
                "gdbPath": "arm-none-eabi-gdb", // Path to your GDB executable
                "serverArgs": [
                   "-c",
                   "gdb_port 1234"
                ],
                "searchDir": [ "${workspaceFolder}/qemu/fw/" ],
                "svdFile": "NUC131.svd"
            }
        ]
    }
    ```

    **STM32F1 (ST-Link):**

    ```json
    {
        "version": "0.2.0",
        "configurations": [
            {
                "name": "Debug (ST-Link)",
                "type": "cortex-debug",
                "request": "launch",
                "servertype": "openocd",
                "executable": "${workspaceFolder}/volvo_od2.elf", // Path to your compiled ELF file
                "configFiles": [
                    "interface/stlink-v2.cfg",
                    "target/stm32f1x.cfg" // Or the appropriate STM32 config file
                ],
                "svdFile": "${workspaceFolder}/libopencm3/lib/stm32/f1/NUC131.svd",
                "runToEntryPoint": "main",
                "preLaunchTask": "build_stm32f1", // Add this line, assuming you have a build task
                "showDevDebugOutput": "both",
                "gdbPath": "arm-none-eabi-gdb", // Path to your GDB executable
                "svdFile": "NUC131.svd"
            }
        ]
    }
    ```

    **NUC131:**

    ```json
    {
        "version": "0.2.0",
        "configurations": [
            {
                "name": "Debug (NUC131)",
                "type": "cortex-debug",
                "request": "launch",
                "servertype": "openocd",
                "executable": "${workspaceFolder}/vw_nc03.elf", // Path to your compiled ELF file
                "configFiles": [
                    "interface/stlink-v2.cfg",
                    "vw_nc03/fw/nuc131.cfg" // Path to your NUC131 config file
                ],
                "svdFile": "${workspaceFolder}/vw_nc03/fw/nuc131bsp/Library/CMSIS/Include/NUC131.svd",
                "runToEntryPoint": "main",
                 "preLaunchTask": "build_vw_nc03", // Add this line, assuming you have a build task
                "showDevDebugOutput": "both",
                "gdbPath": "arm-none-eabi-gdb" // Path to your GDB executable
            }
        ]
    }
    ```

    *   **`type`:**  `cortex-debug` (from the Cortex-Debug extension).
    *   **`request`:** `launch` (starts a new debug session).
    *   **`servertype`:** `openocd` (we're using OpenOCD as the debugger interface).
    *   **`executable`:**  The path to your compiled `.elf` file.  This is what GDB will load.
    *   **`configFiles`:**  The OpenOCD configuration files.  You'll likely need to adjust these paths.  The `stlink-v2.cfg` is for the ST-Link programmer. The `stm32f1x.cfg` or `nuc131.cfg` is for the target microcontroller.
    *   **`svdFile`:** Optional but highly recommended.  Provides register definitions for debugging.  You'll need to find the correct SVD file for your microcontroller. The example uses the one distributed with `libopencm3` and `nuc131bsp`.
    *   **`runToEntryPoint`:**  Sets a breakpoint at the `main` function.
    *   **`gdbPath`:**  The path to your ARM GDB executable (e.g., `arm-none-eabi-gdb`).
    *   **`preLaunchTask`:** This is important. It runs a build task *before* launching the debugger.  You'll need to create a corresponding task in `tasks.json`.

4.  **Create a `tasks.json` file (Optional, but Recommended):** This file defines build tasks that VS Code can run.  Create a `tasks.json` file in the `.vscode` directory:

    ```json
    {
        "version": "2.0.0",
        "tasks": [
            {
                "label": "build_stm32f1",
                "type": "shell",
                "command": "make",
                "args": [
                    "volvo_od2.bin"
                ],
                "group": {
                    "kind": "build",
                    "isDefault": true
                },
                "problemMatcher": [
                    "$gcc"
                ]
            },
            {
                "label": "build_vw_nc03",
                "type": "shell",
                "command": "make",
                "args": [
                    "vw_nc03.bin"
                ],
                "group": {
                    "kind": "build",
                    "isDefault": true
                },
                "problemMatcher": [
                    "$gcc"
                ]
            },
    	{
    		"label": "build_qemu",
    		"type": "shell",
    		"command": "make",
    		"args": [
    			"qemu.bin"
    		],
    		"group": {
    			"kind": "build",
    			"isDefault": true
    		},
    		"problemMatcher": [
    			"$gcc"
    		]
    	}
        ]
    }
    ```

    *   **`label`:**  A name for the task (e.g., "build_stm32f1").
    *   **`type`:** `shell` (we're running a shell command).
    *   **`command`:** `make` (we're using Make to build).
    *   **`args`:** The arguments to pass to `make` (the target, e.g., `volvo_od2.bin`).
    *   **`group`:**  Sets this as a build task and makes it the default build task.
    *   **`problemMatcher`:**  Tells VS Code how to parse compiler errors and warnings.  `$gcc` is a built-in matcher for GCC.

5. **Modify Makefiles:**
Make sure that your Makefiles have a target to produce an `.elf` file. The provided Makefiles do this, but double-check. The `.elf` file contains debugging information that GDB needs.

**Debugging in VS Code:**

1.  **Open Folder:** Open the root of your project folder in VS Code.
2.  **Set Breakpoints:** Open your source files (e.g., `main.c`, `car.c`, `cars/peugeot_407.c`) and click in the left margin to set breakpoints.
3.  **Start Debugging:** Go to the "Run and Debug" view (click the bug icon in the left sidebar). Select the appropriate debug configuration ("Debug (ST-Link)" or "Debug (QEMU)") from the dropdown at the top.  Click the green "Start Debugging" arrow.
4.  **Debug Controls:** Use the debugging controls in VS Code to step through your code, inspect variables, view registers, etc.

**Summary of Changes and Files:**

*   **`cars/peugeot_407.c` (NEW):**  Contains the Peugeot-specific CAN message handlers and data structures.
*   **`cars/peugeot_407.h` (NEW):** Header file for `peugeot_407.c`.
*   **`canbox_raise_vw_peugeot.c` (NEW):**  Formats Peugeot data into messages that emulate the Raise VW protocol.
*   **`canbox_raise_vw_peugeot.h` (NEW):**  Header file for `canbox_raise_vw_peugeot.c`.
*   **`car.c` (MODIFIED):** Includes `peugeot_407.c` and calls `peugeot_407_init()` and `peugeot_407_process()` when the Peugeot car is selected. Includes getter functions for the new data.
*   **`car.h` (MODIFIED):**  Adds `e_car_peugeot_407` to the `e_car_t` enum and getter prototypes.
*   **`Makefile_vw_nc03` (MODIFIED):**  Adds `cars/peugeot_407.o` to the object file list and includes the cars directory to include paths.
*  **`Makefile_volvo_od2` (MODIFIED):**  Adds `cars/peugeot_407.o` to the object file list and includes the cars directory to include paths.
*   **`canbox.c` (MODIFIED):** Includes `canbox_raise_vw_peugeot.h` and calls the functions within it when the Peugeot car is selected.
* **`main.c` (MODIFIED):** Add `canbox_raise_vw_peugeot.h`
*   **`.vscode/c_cpp_properties.json` (NEW):**  VS Code C/C++ configuration.
*   **`.vscode/launch.json` (NEW):** VS Code debugger configuration.
*   **`.vscode/tasks.json` (NEW):** VS Code build tasks.
*   **`README.md` (MODIFIED):**  This file!

This detailed guide should get you started with integrating your Peugeot 407 with the CAN box firmware and setting up a robust development and debugging environment. Remember to adapt the code to your *exact* vehicle and head unit.
content_copy
download
Use code with caution.
