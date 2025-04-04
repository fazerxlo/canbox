# Alternative Firmware for Chinese CANBUS Adapters — DRIVE2

**Published:** September 29, 2021

**Author:** SmartGauges (<a href="https://www.drive2.ru/users/smartgauges/">SmartGauges Profile</a>)


      
---
**Please note:**

This Markdown document is a translation and summarization of the original Russian HTML content.

The translation and processing, including content enhancement from the comment section, were performed by an AI language model.

**AI Model:** Gemini Pro

**Service Provider:** Google AI

---

This note provides information about a type of device called a CANBUS adapter for Android head units. On [AliExpress](https://shopnow.pub/redirect/cpa/o/scgb141706sg2h7oxo9c1ll67u9jafvt/?erid=2SDnjcixWX&sub1=fromtext), these devices are sold under names like "canbus box", "canbus decoder", and similar variations. In this note, I will refer to such devices as CAN adapters or simply adapters.

The initial part of this note may be of interest to those who are completely unfamiliar with this type of device, while the later part may be of interest to those who know what it is and would like to install it in their car or change its operating logic.

Externally, a CAN adapter looks like a small box about the size of a matchbox with one connector of 16 or 20 pins. Most CAN adapters on AliExpress are sold with connectors for the standard wiring of a specific car model, which requires minimal knowledge of electrical engineering and car wiring diagrams.

![CAN adapters on ali](https://a.d-cd.net/eIfamfTGP9N4s_vwfV6GG6DdZq0-960.jpg)
*CAN adapters on ali*

In short, a CAN adapter is designed to simplify the integration of an Android head unit into a car. It extracts digital signals from the car's MS CAN bus and transmits them to the Android head unit. Without a CAN adapter, you would need to manually search for the necessary analog signals in the standard wiring.

The main functions of a CAN adapter are to generate four output signals:

— **"Ignition" (IGN)** or **"Accessory" (ACC)**, 12 Volts level, to turn on the Android head unit
— **"Illumination" (ILLUM)**, 12 Volts level, to change the backlight in the Android head unit
— **"Parking Brake" (PARK)**, GND level, to inform the Android head unit about the car being in driving/parking state.
— **"Reverse Gear" (REAR)**, 12 Volts level, to switch the Android head unit to the rear-view camera and turn on the power for the rear-view camera.

Additional functions of the CAN adapter include displaying the status of the car's internal units in the Android head unit and controlling some of them via the **RX/TX** (UART) information line:

— Displaying the status of parking sensors
— Displaying the steering wheel position as dynamic guidelines when reversing
— Displaying the status of open doors
— Displaying the status of unfastened passengers and driver
— Setting the time in the car
— Displaying and controlling the climate control mode
— Translating steering wheel button presses to the Android head unit
— Displaying fuel level
— Access to the trip computer
— Controlling the factory amplifier

![CAN adapter connection](https://a.d-cd.net/GwPbCFrrlO-C-J1zdoMvE5t5de8-960.jpg)
*CAN adapter connection*

Now, more details about the output signals generated by the CAN adapter. For any Android head unit, in addition to power, at least one more additional signal is needed - the **"Ignition" (IGN)** signal. The ignition signal (IGN) is necessary for the Android head unit to turn on or wake up from sleep mode when the car is started and turn off or go to sleep when the car is turned off.

Currently, most Android head units probably have a sleep mode, with which the Android head unit is ready to work within a couple of seconds after starting the car. Otherwise, each time the car is started, Android in the head unit will take several minutes to boot. However, the sleep mode of the head unit has one disadvantage - it consumes current of about 10-20mA in sleep mode, and this can be critical in winter or with short trips.

For example, my Android head unit, purchased in 2019, did not have a sleep mode. Several months after purchase, the manufacturer added a sleep mode function in the Android head unit firmware update. Remembering the power consumption of the head unit in sleep mode, I turn off the sleep mode in the head unit for the winter, and turn it back on when warm weather arrives. Turning off/on the sleep mode in my head unit takes a couple of minutes.

The optional **"Illumination" (ILLUM)** signal is used to illuminate the buttons of the Android head unit or to reduce the brightness of the head unit screen at night and increase the screen brightness during the day.

The optional **"Reverse Gear" (REAR)** signal is used to switch the head unit screen to the camera and connect power to the rear-view camera. Otherwise, you will have to run wires from the taillights, which is inconvenient and, secondly, on some cars with pulsed burnt-out bulb control, such power supply for the camera will create interference in the camera image.

The optional **"Parking Brake" (PARK)** signal is used to differentiate some functionality in motion and in parking, for example, to prohibit watching videos while driving.

Some Android head units for specific car models are sold with a CAN adapter and all the necessary wiring and connectors, which is of course very convenient. My Android head unit was sold without a CAN adapter, but this disadvantage was compensated by the cost of the head unit at $50. However, for my Freelander 2 car, I could not find a CAN adapter either a couple of years ago or now. But with the help of a homemade CAN adapter, I was able to display the factory parking sensor readings on the Android head unit, which is described in detail in the note about [connecting factory parking sensors to an Android head unit](https://www.drive2.ru/l/543928403334529733/). Thanks to this note, several people were able to display parking sensors in their Freelander 2 cars, including the restyled Freelander 2.

Actually, this story began after that note, when last year a colleague purchased an Android head unit for his XC90 on AliExpress. The head unit came with a CAN adapter, but several problems were found in its operation:

— Track switching in the player using steering wheel buttons did not work
— Parking sensors were not displayed
— Steering wheel position was not displayed

Opening this CAN adapter and seeing a GD32F103 processor on the board, I rashly told my colleague that all the shortcomings of this adapter could be corrected by writing my own program for it, since documentation is freely available for this processor and it is also a clone of the STM32F103, and therefore all development tools available for STM32 microprocessors can be used for it. But at that time there was no time to deal with it at all.

![CAN adapter OD-VOLVO-OD2 for Volvo XC90](https://a.d-cd.net/WzSR87hXDCSI0B2_SRVVe7AafMk-960.jpg)
*CAN adapter OD-VOLVO-OD2 for Volvo XC90*

Autumn, winter, and then spring passed, and my colleague periodically asked when we would update the firmware in the adapter, but there was still no time to deal with it. But with the onset of summer, due to lunch delivery to work, free time appeared, which was previously spent on lunch trips. Thanks to this, in a few days, using a multimeter, the adapter board was investigated, and as a result, a schematic of the OD-VOLVO-02 adapter was drawn.

![Schematic OD-VOLVO-02](https://a.d-cd.net/zueoV8mIOHkEGjy_DZVL-KCqH2E-960.jpg)
*Schematic OD-VOLVO-02*

In general, there is nothing superfluous in the schematic, the functionality of the schematic coincides with the general description of the CAN adapter, these are 4 output signals (IGN, ILLUM, BRAKE, REAR) and a serial port (RX/TX) for information communication with the Android head unit. The designations and ratings of the elements in the schematic coincide with the inscriptions on the printed circuit board. For microcontroller programming via the SWDIO/SWCLK interface, there is a special internal connector CON2 on the board. To update the firmware, you can use any SWDIO/SWCLK programmer. I reflashed the microcontroller on the board using a Chinese ST-LinkV2 from AliExpress and the openocd program.
The pinout of the signals on the external connector of the OD-VOLVO-02 CAN adapter exactly matched the pinout of other Chinese 16-pin CAN adapters found on the network, so it can be assumed that various Chinese manufacturers of CAN adapters make physically compatible adapters, which was confirmed later.

![16-pin CAN adapter connector](https://a.d-cd.net/SXIyJrnHvUQPbg53518O9PzRyeA-960.jpg)
*16-pin CAN adapter connector*

According to the finished device schematic, writing the firmware was already only a matter of time. The firmware writing process was iterative and alternated with tests on the car during lunch breaks. This summer in the Urals turned out to be warm and it was pleasant to spend some time outside with the car - to switch to a different activity from the main work.
In addition to eliminating the shortcomings in the original firmware, my colleague asked to add a delay in removing power from the camera when switching from R to D and the ability to control the backlight level on the Android head unit. This functionality was implemented and made configurable.
For convenience of firmware and debugging of the CAN adapter in the car, wiring was made to connect the adapter to the OBD2 connector of the car. Through the OBD2 connector, the CAN adapter receives power and access to the MS CAN bus, and the RX/TX line of the adapter is connected to a usb2com adapter. A laptop or Android phone can then be connected to the usb2com adapter to control the operation of the adapter. If it was necessary to check the functionality of the CAN adapter, the adapter was connected to the head unit. If it was necessary to flash, debug, or configure the adapter, the adapter was connected to the OBD2 connector of the car.

![Connecting the adapter to the car](https://a.d-cd.net/b9EMTniq3I-yEOjN340t1j_vatI-960.jpg)
*Connecting the adapter to the car*

For debugging and configuration, a debugging interface was implemented in the CAN adapter via the RX/TX information line. Normally, data about the car's status is transmitted to the Android head unit via the RX/TX line, but by connecting to the RX/TX line with a laptop or Android phone, you can switch the adapter to debugging mode, and then a simple text debugging interface will be available via the RX/TX line. With the debugging interface, you can control the correct functioning of the adapter or make changes to its configuration. Changing the adapter's operating configuration and switching to debugging mode occurs when you press the corresponding keys on the keyboard:

— `OOOOOOOOOOOOO` switching to debugging mode (you need to send at least 10 characters per 1 second)
— `o` switching to normal operation mode
— `b` selecting the emulated protocol from the list Raise VW(PQ), Raise VW(MQB), Oudi BMW(Nbt Evo, HiWorld VW(MQB)
— `c` selecting the car from the list FL2 2007MY, FL2 2013MY, XC90 2007MY
— `I` increasing the backlight level threshold
— `i` decreasing the backlight level threshold
— `D` increasing the camera off delay
— `d` decreasing the camera off delay
— `m` selecting the active CAN message
— `s` saving the configuration

The following screenshot shows the CAN adapter debugging interface

![CAN adapter settings](https://a.d-cd.net/TSjwSLCA8UJDLxKCwj5lJas0WRA-960.jpg)
*CAN adapter settings*

Since the data is constantly changing and to avoid the effect of text running across the screen, a terminal emulator program is needed to work with the debugging interface. For Windows, putty or hyperterminal are suitable.
The **Configuration** section displays the CAN adapter settings:

— **Car** car model
— **Vin** car VIN (displayed only in FL2)
— **CanBox** emulated canbus protocol
— **Illum** backlight level (in percent) in the cabin at which the Illum output is generated
— **Rear Delay** delay (in milliseconds) to turn off the REAR signal

The **State** section displays the current state of the CAN adapter and the car:

— **Acc, Ign, Selector, Wheel** — status of key position, ignition, automatic transmission selector and steering wheel position (not displayed in FL2)
— **R** — REAR output status
— **Illum, ParkLights, NearLights** — backlight level in the cabin and status of parking lights and low beam
— **Can, Msgs, Irqs** — CAN message debugging counters
— **CanX/Y** — current status of CanY message with a specific ID from list Y with all IDs

The adapter extracts data from the MS CAN bus in listening mode, without sending any requests to the car's electronic control units, so the information that it can extract from the CAN bus depends on the car. For example, in Freelander2, information about the steering wheel position and button presses is not transmitted in the MS CAN bus, and in the MS CAN bus of the XC90 car, the VIN code of the car is not transmitted.

When searching for and deciphering CAN data in the car, a technique was used where all CAN messages are recorded in the car while creating a certain pattern in the data, and then in the recorded CAN messages, using a program that displays data in the form of graphs, the necessary pattern is searched for and the data is identified. A pattern in the data implies a change in the state of the car's units over time, for example, the driver's door was opened and closed five times, then the passenger door was opened and closed 4 times, etc. Recording a specific pattern with data into separate files facilitates subsequent analysis and search.
I performed such analysis using the [plot](https://github.com/smartgauges/obd2-bt-stm32/raw/master/qt/plot/plot.exe) program, which I mentioned earlier in the note [on deciphering data in the CAN bus](https://www.drive2.ru/b/522073135831319435). Since the publication of this note, support for log (can-uitils) and trc (canhacker) file formats, as well as group display (all 8 bytes of CAN message) of messages on graphs, has been added to the plot program. With group display, you can quickly analyze data in messages with a specific CAN ID, and then, without group display, you can more accurately try to interpret data from each byte of the CAN message by applying various masks and coefficients.

Below, to understand the complexity of the data search process, a couple of screenshots of the plot application with data from the MS CAN bus of the XC90 car are shown. With patience and imagination, it is easy to find the necessary data in the entire stream of CAN messages.
The first screenshot shows a pattern (gray color) set during recording: 3 short presses of the PREV button, pause, 3 short presses of the PREV button, pause, 3 long presses of the PREV button.

![Pressing the PREV button on the XC90 steering wheel](https://a.d-cd.net/za0ueuiA2eyo4KO39amiR7oVP_o-960.jpg)
*Pressing the PREV button on the XC90 steering wheel*

The second screenshot shows a pattern (green and turquoise colors) set during recording: 3 quick changes in backlight level, pause, 3 quick changes in backlight level, pause, 3 slow changes in backlight level.

![Changing the backlight level in the XC90 cabin](https://a.d-cd.net/3jWb_IfakD5v33oSjrsid_uDI5g-960.jpg)
*Changing the backlight level in the XC90 cabin*

In the process of deciphering CAN data for the XC90, I tried to find ready-made results on deciphering CAN messages in the XC90 on the network and found interesting information in the note [about a homemade CAN adapter for XC90](https://www.drive2.ru/l/523338948592799298/) by user [Olegelm](https://www.drive2.ru/p/u/B0JVAEAAD3U) that in Volvo cars each electronic control unit has a unique identifier and it is different for all cars of the same model. This was intended to prevent modules from one car from being installed in another car, and thereby reduce car and car part theft.
Therefore, it turns out that the information on the deciphered data from the XC90 car of a colleague is most likely not suitable for another XC90 instance. For another XC90 instance, you can only use information on the location of data inside 8 bytes of CAN message, and the identifier itself will need to be selected independently.
Perhaps due to the uniqueness of addresses, the factory firmware of the XC90 CAN adapter had problems, since the Chinese could not make a universal firmware suitable for all XC90s. I could not find information on what algorithm is used to assign identifiers to electronic control units in Volvo cars; it can be assumed that it is based on VIN or date of manufacture. Perhaps there are Volvo specialists who can comment on this.

The firmware emulates 4 different protocols for interacting with the Android head unit:

— Raise VW(PQ)
— Raise VW(MQB)
— Oudi BMW(Nbt Evo
— HiWorld VW(MQB)

When choosing the type of connected adapter in the Android head unit, a list of a dozen different adapter manufacturers is offered, the most common adapters from Raise and HiWorld on AliExpress. Volkswagen and Raise and HiWorld adapters have the greatest functionality for displaying car units in the Android head unit, so these protocols were chosen for emulation.

![Choosing CAN adapter type in Android head unit](https://a.d-cd.net/AdaqBT6Fx-4c4YdkWVPVePiIOTY-960.jpg)
*Choosing CAN adapter type in Android head unit*

But the Chinese are the new Indians, adapter support in their head units is sometimes buggy, and I don't want to be limited to the functionality that is in the head unit, so in my car I am gradually disabling the CAN adapter emulation in the usb2most board and drawing the things I need through my application.

In addition to the above, my colleague drove the car, tested the CAN adapter, and identified comments on its operation. To address these comments, I needed time and the adapter itself, but the colleague also needed the adapter. It turned out that the adapter was needed by both me and him at the same time. There was no point in buying the same second CAN adapter VOLVO-OD-O2, but I had an adapter for the VW brand, which I purchased slightly later than buying my head unit, out of curiosity about what was inside. Inside it turned out to be a processor with the inscription PocketLink. Back in 2019, I could not reflash this adapter, because I did not fully understand what kind of processor it was, and now the inscription on the PocketLink chip will not give information about what is inside this chip.

![CAN adapter VW_NC03 for VW Tiguan](https://a.d-cd.net/lcrmmZukQI4WbjREH1iCUEmSflo-960.jpg)
*CAN adapter VW_NC03 for VW Tiguan*

Apparently, spending a little more time than before, I managed to understand that the processor NUC131 is inside, and [BSP](https://github.com/OpenNuvoton/NUC131BSP) on github is freely available for it. Further, again with the help of a multimeter, the board was analyzed and a schematic of this adapter was drawn in KiCAD. The Chinese saved on silkscreen printing, so the designations in the schematic are arbitrary, but the ratings match. According to the schematic, using examples from the BSP, support for this board was added to the firmware. The firmware now supported two boards, and it was possible to safely exchange adapters with a colleague during the next update of the test firmware.
The schematic of the VW_NC03 adapter is a bit simpler than OD-VOLVO-02, but the main functionality is the same: 4 output signals and a serial port.

![Schematic VW_NC03](https://a.d-cd.net/TOt3xFA084XdfuJ-wxH2xoNnJ3k-960.jpg)
*Schematic VW_NC03*

Later, during experiments, I bricked the board from the VW_NC03 CAN adapter - I could no longer reflash it, so another VW_NC03 adapter was ordered on AliExpress, but inside it was already a VW_NCD01 board dated 2021, while the VW_NC03 board was dated 2017. Analysis with a multimeter showed that this board is almost completely compatible with VW_NC03 in terms of circuitry, but the processor in it is already different. Debugging via openocd showed that the processor inside was clearly Nuvoton, had identifiers similar to NUC131, and, as it turned out, the firmware for the VW_NC03 board is suitable for it, so I did not create a separate schematic or firmware for it. But formally, we can say that the firmware supports 3 different adapters.

![CAN adapter VW_NC03(VW_NCD01) for VW Tiguan](https://a.d-cd.net/XBccAGWv2w7oQSR8jxY74UEPtng-960.jpg)
*CAN adapter VW_NC03(VW_NCD01) for VW Tiguan*

In general, Chinese processors in Chinese adapters turned out to be no worse and no better than the same STM32s, inside they have the same ARM Cortex-M core as in STM32, so I can recommend such adapters for any automotive DIY projects. Self-assembly of such a device on discrete elements or from several boards will not be cheaper, and here is a ready-made device with 4 programmable outputs and in a housing.

For those who want to purchase such adapters on AliExpress, the VW_NC03 adapter can be found by searching for "canbus box volkswagen", and then choosing an adapter with the inscription HW:VW-NC-003. Usually, the seller can purchase either just the adapter or a kit of adapter with wiring, the cost of the adapter starts from $7.
By searching for "volvo xc90 canbus" or "OD VOLVO 02" on AliExpress, you can find a CAN adapter for Volvo also with or without wiring.
And the wiring kit from XC90 is connector-compatible with Freelander2, so Freelander2 owners in the configuration with MOST bus will be able to use such wiring when installing Android head units in their cars. With the connectors from this wiring, you can neatly connect to the standard wiring of Freelander2 "connector to connector" without destroying the standard wiring. The gray connector has AUX and a signal for the FM radio amplifier, which many use instead of the IGN signal. The green connector has an MS CAN bus for connecting a CAN adapter and a signal from the resistive divider of the steering wheel buttons.

![Wiring for CAN adapter xc90](https://a.d-cd.net/kJc14raonl1BArJxxAiHr58hsoA-960.jpg)
*Wiring for CAN adapter xc90*

The source code of the firmware and ready-made files for reflashing adapters are available on github: [github.com/smartgauges/canbox](https://github.com/smartgauges/canbox). To reflash the adapter, you need ST-Link V2 and the openocd program. I used the version from github: [github.com/OpenNuvoton/OpenOCD-Nuvoton](https://github.com/OpenNuvoton/OpenOCD-Nuvoton).
To build the firmware, arm-none-eabi-gcc from the ubuntu distribution was used. The adapter firmware is flashed using the commands `make flash_volvo_od2` and `make flash_vw_nc03` for OD-VOLVO-02 and VW_NC03 adapters, respectively.

If someone reaches the firmware stage, I have prepared a virtual machine with Linux with all the necessary tools for building and flashing. The machine is available at the link: [drive.google.com/file/d/1…kctj8inW/view?usp=sharing](https://drive.google.com/file/d/1XCHW03gKX6WUB6tFS7kKD0kZkctj8inW/view?usp=sharing)

User data in the virtual machine
Login:user
Password:user

Brief command prompt in Linux:
mc - file manager
cd canbox - go to the directory with canbox sources
git pull - get canbox source updates from github
make clean - clear current firmware build
make - build a new firmware build
make flash_vw_nc03 - flash firmware to WV-NC-003 board
The screenshot shows an example of how the flashing process looks

![Adapter reflashing process using virtual machine](https://a.d-cd.net/-GiRY-G76lu8QXJ3LWkatUmnwuc-960.jpg)
*Adapter reflashing process using virtual machine*

As a result of a sufficiently long presentation:

1) My colleague is satisfied with the adapter, but is already asking questions about the engine chip )
2) The firmware supports 2(3) CAN adapters.
3) Power consumption of the OD-VOLVO-02 adapter in sleep mode is 4mA (with factory firmware 20mA)
4) Power consumption of the VW_NC03 adapter in sleep mode is 1mA (with factory firmware 1mA)
5) The firmware supports 2(3) cars (restyled Frelander2 - from the point of view of the CAN bus it is a different car)
6) The firmware supports 4 protocols for communication with the Android head unit

PS
Thanks to [kostya-sha](https://www.drive2.ru/p/u/AAAAAAAwNwk) for the tip on the project with the Android head unit source code, it is much easier to see the implementation of various canbox manufacturers' protocols in it,
for example, a description of the Raise VW(MQB) commands and protocol: [github.com/runmousefly/Wo…n_Golf7_Protocol.java#L23](https://github.com/runmousefly/Work/blob/7d99feea7de35b8fc7a89fdc3ba4b14249422f07/MctCoreServices/src/com/mct/carmodels/RZC_Volkswagen_Golf7_Protocol.java#L23)

PSPS
Thanks to [iied](https://www.drive2.ru/p/u/AAAAAAAvUtE) for the link to a very large list with [Protocol Descriptions for various adapters](https://github.com/cxsichen/helllo-world/tree/master/%E5%8D%8F%E8%AE%AE/%E7%9D%BF%E5%BF%97%E8%AF%9A)

PSPSPS
Interesting links on the topic of CAN for Volvo:
1) Description of the diagnostic software protocol (VIDA) [hackingvolvo.blogspot.com/2012/12/success.html](https://hackingvolvo.blogspot.com/2012/12/success.html)
2) Implementation of a device based on diagnostic requests to blocks: [github.com/vtl/volvo-ddd](https://github.com/vtl/volvo-ddd)
3) Implementation of a device based on listening to data: [github.com/vtl/volvo-kenwood](https://github.com/vtl/volvo-kenwood)

---

**Additions from comments:**

* **Firmware and OpenOCD:** For flashing VW_NC03 adapters, especially newer versions, a **older version of OpenOCD** may be required (e.g., commit `0.10.0-dev-00475-g043c9287`). Newer versions may not recognize controllers or throw errors during erasing. Building OpenOCD from source code ([github.com/OpenNuvoton/OpenOCD-Nuvoton](https://github.com/OpenNuvoton/OpenOCD-Nuvoton)) is recommended, using commands `./bootstrap`, `./configure --disable-werror --disable-shared --enable-ftdi`, `make`, `sudo make install`. In the `canbox` project Makefile, you need to specify the correct path to the installed OpenOCD version.

* **Raise RZ-13 and RZ-63 Adapters:** The firmware is **compatible with Raise RZ-13 and RZ-63 adapters**, which use the NUC131 processor.

* **RX/TX Logic Levels:** The RX/TX logic levels between the CAN adapter and the head unit are **3.3V**. However, on some adapters (RZ-FD-10), it has been observed that the level from CAN Rx to GU Tx is 3.3V, and from CAN Tx to GU Rx - 5V.

* **Sleep Mode Power Consumption:** Alternative firmware reduces power consumption in sleep mode for the OD-VOLVO-02 adapter from 20mA to **4mA**. For VW_NC03, power consumption remains at **1mA**.

* **ST-Link Connection:** ST-Link connection to the VW_NC03 adapter is made through the internal CON2 connector. The pinout location can be determined using the schematic or by ringing out. It is important to correctly connect GND, PWR, SWDIO, SWCLK and RST (optional).

* **Protocols and Customization:** The firmware supports emulation of **Raise VW (PQ, MQB), Oudi BMW (Nbt Evo), HiWorld VW (MQB)** protocols. Users have successfully added support for new cars and features by modifying the source code and reflashing the adapters.

* **CAN bus Volvo XC90:** For Volvo XC90 of different model years (2002-2004, 2005-2006, 2007+), CAN message IDs and even data formats may differ. The firmware requires adaptation for a specific model year. Identifying the model year and the corresponding CAN message IDs is a key step for successful integration.

* **Alternative CAN Adapters:** The possibility of using CAN adapters with **two and three CAN buses** for cars with multiple CAN networks (e.g., Audi) is being considered. An adapter with two CAN buses (based on a photo in the comments) has been noticed.

* **Troubleshooting Firmware Issues:** If OpenOCD throws a `target not halted` error, it is recommended to restart the flashing process, check the ST-Link connection, and use a **3.3V power supply** for the controller during flashing. In some cases, using an **older version of OpenOCD** helped.

* **UART Debugging:** For debugging the CAN adapter's UART interface, a baud rate of **38400** is used. Connecting a terminal (Putty, HyperTerminal) allows you to control the adapter's operation and change its configuration through a text interface. To enter debugging mode, you need to send the character sequence `OOOOOOOOOOOOO` (13 Latin capital 'O's).

* **Steering Wheel Button Emulation:** Android HU owners face the problem of **remapping steering wheel buttons**, especially when using CAN adapters. Standard button remapping may only work with resistive connection (KEY1/KEY2), not via CAN. Solving this problem requires further research and possibly APK file modification of the HU.

* **Virtual Machine:** Link to a virtual machine with a pre-installed environment for building and flashing: [drive.google.com/file/d/1XCHW03gKX6WUB6tFS7kKD0kZkctj8inW/view?usp=sharing](https://drive.google.com/file/d/1XCHW03gKX6WUB6tFS7kKD0kZkctj8inW/view?usp=sharing). Login/password: `user/user`.

**Caution:** Before flashing the CAN adapter, ensure compatibility with your car and head unit. Incorrect firmware can lead to incorrect operation or damage to devices. Use the information from the article and comments at your own risk.

---