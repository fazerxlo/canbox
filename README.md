# CANBUS Box Firmware

Firmware for alternative CANBUS adapters commonly found on AliExpress.

A CANBUS box is a device that facilitates the integration of an Android head unit with a vehicle's CAN bus system. It extracts digital signals from the car's CAN bus and transmits them to the Android unit, eliminating the need to manually tap into the vehicle's wiring for certain functions.

## General Features

The CANBUS adapter provides several key functions for Android head units:

- **Ignition Signal (IGN/ACC)**: Sends a 12V signal to power on the Android head unit when the ignition is turned on.
- **Backlight Signal (ILLUM)**: Controls screen and button illumination based on ambient lighting conditions.
- **Parking Brake Signal (PARK)**: Notifies the head unit when the vehicle is stationary, enabling/disabling certain features such as video playback.
- **Reverse Gear Signal (REAR)**: Activates the rearview camera and provides power to it when reverse gear is engaged.

### Additional Features

Some CANBUS adapters also support advanced features via the RX/TX (UART) communication line, including:

- Displaying **parking sensor** information on the Android head unit.
- Displaying **steering wheel position** dynamically on the screen.
- Indicating **open doors** and **seatbelt status**.
- Syncing **vehicle time** with the Android unit.
- Displaying and controlling **climate control settings**.
- Relaying **steering wheel button presses** to the Android system.
- Displaying **fuel level** and **trip computer data**.
- Controlling the **OEM amplifier**.
- **Adaptive cruise control integration**
- **Transmission mode detection**
- **Customizable logging for vehicle diagnostics**

## Supported Car Models and Protocols

The firmware supports multiple car models and CANBUS protocols:

### Supported Car Models:
- Land Rover Freelander 2 (2007MY, 2013MY)
- Volvo XC90 (2007MY)
- Volkswagen Tiguan
- Other vehicles using similar CANBUS adapters

### Supported CANBUS Protocols:
- **Raise VW (PQ)**
- **Raise VW (MQB)**
- **Audi BMW (NBT Evo)**
- **HiWorld VW (MQB)**
- **Mercedes-Benz UDS-based CANBUS**

## Configuration

Since the adapter supports multiple car models and protocols, it must be configured accordingly. To configure, connect the adapter to the **RX and TX lines** at a **38400 baud rate** using any terminal program (e.g., Putty, HyperTerminal, Minicom).

### Debug Mode

Switching to debug mode allows modification of adapter settings using specific keyboard commands:

- `OOOOOOOOOOOO` (at least 10 times within 1 second) → **Switch to debug mode**
- `o` → **Switch to normal mode**
- `b` → **Select CANBUS protocol**
- `c` → **Select vehicle model**
- `I` → **Increase backlight threshold**
- `i` → **Decrease backlight threshold**
- `D` → **Increase camera turn-off delay**
- `d` → **Decrease camera turn-off delay**
- `m` → **Select active CAN message**
- `s` → **Save configuration**
- `t` → **Enable transmission mode monitoring**
- `v` → **Enable vehicle diagnostics logging**

## Using QEMU for Development and Testing

QEMU can be used to develop, debug, and test firmware for STM32 microcontrollers before deploying to physical hardware. This provides a cost-effective and efficient method to validate functionality without requiring a physical microcontroller.

### Setting Up QEMU for STM32 Development

1. **Install QEMU**:
   ```sh
   sudo apt update
   sudo apt install qemu-system-arm
   ```

2. **Compile the Firmware for QEMU**:
   Ensure the firmware is compiled for an ARM Cortex-M architecture using `arm-none-eabi-gcc`:
   ```sh
   make qemu
   ```

3. **Run the Firmware in QEMU**:
   ```sh
   qemu-system-arm -M stm32-p103 -kernel firmware.elf -nographic -S -s
   ```
   The `-S -s` options allow debugging via GDB.

### Debugging with GDB

1. **Start GDB Session**:
   ```sh
   arm-none-eabi-gdb firmware.elf
   ```

2. **Connect to QEMU**:
   ```sh
   target remote :1234
   ```

3. **Set Breakpoints and Debug**:
   ```sh
   break main
   continue
   ```

4. **Inspect Variables and Memory**:
   ```sh
   info registers
   x/10x 0x20000000  # Inspect memory
   ```

### Communicating with QEMU STM32 UART

QEMU allows UART communication with the STM32 firmware via a virtual serial port.

1. **Run QEMU with UART Output**:
   ```sh
   qemu-system-arm -M stm32-p103 -kernel firmware.elf -serial mon:stdio -nographic
   ```
   This redirects UART output to the terminal.

2. **Send Data to UART**:
   Open another terminal and use `socat` or `minicom` to communicate:
   ```sh
   socat - UNIX-CONNECT:/dev/pts/1
   ```
   Replace `/dev/pts/1` with the correct virtual serial port.

3. **Read UART Output in Real-Time**:
   ```sh
   cat /dev/pts/1
   ```

4. **Send Commands to the Firmware**:
   ```sh
   echo "AT+INFO" > /dev/pts/1
   ```
   This will send `AT+INFO` to the STM32 firmware running in QEMU.

Using this method, developers can interact with STM32 firmware over a virtual UART interface, simulating serial communication without requiring hardware.

### Using QEMU with Linux CAN Interface

QEMU can be used to simulate a CAN network for testing CANBUS messages without physical hardware.

1. **Create a Virtual CAN Interface**:
   ```sh
   sudo modprobe vcan
   sudo ip link add dev vcan0 type vcan
   sudo ip link set up vcan0
   ```

2. **Run QEMU with a CAN Device**:
   ```sh
   qemu-system-arm -M stm32-p103 -kernel firmware.elf -nographic -S -s -device can-host,canbus=canbus0 -netdev vcan,id=canbus0
   ```

3. **Send and Monitor CAN Messages**:
   ```sh
   cansend vcan0 123#DEADBEEF
   candump vcan0
   ```

This setup enables developers to send and receive CAN messages in a simulated environment before deploying to physical CANBUS hardware.

## Firmware Update & Debugging

The firmware can be tested using an emulator (`qemu`) before flashing it to a real device.

To flash a firmware update, use an **ST-Link V2** programmer and **OpenOCD**:

```sh
make flash_volvo_od2  # Flash firmware for Volvo OD2 adapter
make flash_vw_nc03    # Flash firmware for VW NC03 adapter
```

### Debugging with OBD2 Connector

For convenient debugging and firmware updates, the CANBUS adapter can be connected through the OBD2 port, providing both power and access to the MS-CAN bus. Additionally, the RX/TX lines can be connected to a **USB-to-serial adapter**, allowing monitoring and debugging from a PC or Android device.


## References

For more details, visit:
- [GitHub Repository](https://github.com/smartgauges/canbox)
- [OBD2 Plotting Tool](https://github.com/smartgauges/obd2-bt-stm32/raw/master/qt/plot/plot.exe)

## Images

### Debugging Output in QEMU
![debug info](qemu.png)

---

This updated `README.md` provides a more comprehensive guide to the firmware, CANBUS adapters, their hardware and software, and debugging techniques, including QEMU for STM32 development, testing, Linux CAN interface integration, and UART communication.
