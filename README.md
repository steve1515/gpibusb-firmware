
# GPIBUSB Adapter Firmware (Version 6.00)

The goal of this project is to create firmware for the [Galvant Industries](https://github.com/Galvant) GPIBUSB adapter containing near full compatibility with the [Prologix GPIB-USB](http://prologix.biz/gpib-usb-controller.html) controller.

## Hardware
GPIBUSB adapter hardware is designed by [Galvant Industries](https://github.com/Galvant).

The associated PCB project can be found at <https://github.com/Galvant/gpibusb-pcb>, and pre-assembled adapters can be purchased at <http://www.galvant.ca/>.

## Firmware
Compiling from source requires the CCS compiler from <https://www.ccsinfo.com/>.\
*Note: A pre-compiled HEX file is included with the source code.*

## Compatiblity
This firmware is compatible with GPIBUSB hardware versions 3 and 4 only.

## Installation
The GPIBUSB comes pre-installed with the *Tiny Bootloader*.\
Firmware HEX files can easily be flashed using the following utilities:\
[Tiny Bootloader Windows](http://www.etc.ugal.ro/cchiculita/software/tinyblddownload.htm)\
[Tiny Bootloader Linux](http://tinybldlin.sourceforge.net/)

*Note: Use a baud rate of 115200 when updating firmware with the Tiny Bootloader.*

## Communication Settings
**Baud Rate:** 460800\
**Data Bits:** 8\
**Stop Bits:** 1\
**Parity:** None\
**Flow Control:** None

## Data Transmission
- Characters received over USB are interpreted only once a termination character (**CR** or **LF**) is received.

- Any **CR**, **LF**, **ESC** or **'+'** characters in the USB data to be sent to the GPIB bus must be escaped by preceding them with the **ESC** character.

- All un-escaped **CR**, **LF**, **ESC** and **'+'** characters received over USB are discarded.

- Any USB input starting with an un-escaped **"++"** character sequence is interpreted as a command and is not sent to the GPIB bus.

- Binary data may be sent or received, but as described above **CR**, **LF**, **ESC**, and **'+'** must be escaped in order to be sent to the GPIB bus.

*Note:*\
**CR** = ASCII 13\
**LF** = ASCII 10\
**ESC** = ASCII 27

## Prologix Command List
The following commands are compatible with the [Prologix Command Set](http://prologix.biz/manuals.html).\
Any differences will be noted below.\
<br/>

**Get/Set GPIB Address**\
This command sets the target device address when in controller mode and sets the device address of the GPIBUSB when in device mode.
```
++addr [<PAD> [<SAD>]]
```
`++addr`: Display current GPIB address.\
`++addr 18`: Set GPIB primary address to 18.\
`++addr 18 98`: Set GPIB primary address to 18 and secondary address to 2.

*Note:*\
Valid primary address range is 1-30.\
Valid secondary address range is 96-126 representing 0-30. *(e.g. 96 = 0, 97=1, 98=2, etc.)*\
The secondary address is not used when the GPIBUSB is in device mode.\
<br/>

**Enable/Disable Read-After-Write**\
This command enables or disables automatically requesting a read to EOI from the target device after any data is sent from USB to the GPIB bus.
```
++auto [0|1]
```
`++auto`: Display current read-after-write setting.\
`++auto 0`: Disable read-after-write.\
`++auto 1`: Enable read-after-write.

*Note:*\
This command only applies when the GPIBUSB is in controller mode.\
<br/>

**Send Selected Device Clear**\
This command sends the Selected Device Clear (SDC) GPIB command to the currently addressed device.
```
++clr
```

*Note:*\
This command only applies when the GPIBUSB is in controller mode.\
<br/>

**Enable/Disable EOI Assertion**\
This command enables or disables asserting the EOI signal with the final byte when sending data from USB to the GPIB bus.
```
++eoi [0|1]
```
`++eoi`: Display current EOI assertion setting.\
`++eoi 0`: Disable EOI assertion.\
`++eoi 1`: Enable EOI assertion.\
<br/>

**Get/Set GPIB Termination Characters**\
This command sets which termination characters are automatically appended to data received over USB before sending them to the GPIB bus.
```
++eos [0|1|2|3]
```
`++eos`: Display current GPIB termination setting.\
`++eos 0`: Append **CR**+**LF** to data.\
`++eos 1`: Append **CR** to data.\
`++eos 2`: Append **LF** to data.\
`++eos 3`: Append nothing to data.\
<br/>

**Enable/Disable EOT Character**\
This command enables or disables appending a user specified character to any USB output whenever the EOI signal is detected when reading data from the GPIB bus.
```
++eot_enable [0|1]
```
`++eot_enable`: Display current EOT enable setting.\
`++eot_enable 0`: Disable appending EOT character.\
`++eot_enable 1`: Enable appending EOT character.\
<br/>

**Get/Set EOT Character**\
This command sets the user specified EOT character.
```
++eot_char [<char>]
```
`++eot_char`: Display current EOT character.\
`++eot_char 10`: Set EOT character to **LF**.

*Note:*\
Valid character range is 0-255.\
<br/>

**Send Interface Clear Signal**\
This command asserts the GPIB IFC signal for 150 &mu;Seconds.
```
++ifc
```

*Note:*\
This command only applies when the GPIBUSB is in controller mode.\
<br/>

**Send Local Lockout**\
This command sends the Local Lockout (LLO) GPIB command to the currently addressed device.
```
++llo
```

*Note:*\
This command only applies when the GPIBUSB is in controller mode.\
<br/>

**Send Go To Local**\
This command sends the Go To Local (GTL) GPIB command to the currently addressed device.
```
++loc
```

*Note:*\
This command only applies when the GPIBUSB is in controller mode.\
<br/>

**Enable/Disable Listen Only Mode**\
This command configures the GPIBUSB to listen to all GPIB bus traffic regardless of the currently set address. In this mode, the GPIBUSB cannot send any data to the GPIB bus.
```
++lon [0|1]
```

*Note:*\
This command only applies when the GPIBUSB is in device mode.\
<br/>

**Get/Set Controller or Device Mode**\
This command configures the GPIBUSB to either act as a GPIB bus controller or a standard addressable device.
```
++mode [0|1]
```
`++mode`: Display current mode setting.\
`++mode 0`: Set GPIBUSB to device mode.\
`++mode 1`: Set GPIBUSB to controller mode.\
<br/>

**Get/Set Read/Write Timeout**\
This command configures the GPIBUSB read and write timeout value in milliseconds.
```
++read_tmo_ms <time>
```
`++rad_tmo_ms`: Display timeout setting.\
`++read_tmo_ms 1000`: Set timeout setting to 1000 milliseconds.

*Note:*\
Valid timeout range is 0-3000.\
<br/>

**Read Data**\
This command reads data from the currently addressed instrument until either a timeout, EOI signal or specified character is detected.
```
++read [eoi|<char>]
```
`++read`: Read until timeout.\
`++read eoi`: Read until EOI or timeout.\
`++read 10`: Read until **LF** is received or timeout.

*Note:*\
Valid character range is 0-255.\
This command only applies when the GPIBUSB is in controller mode.\
<br/>

**Reset GPIBUSB Microprocessor**\
This command performs a power-on reset of the GPIBUSB microcontroller.
```
++rst
```
<br/>

**Enable/Disable Saving to EEPROM**\
This command enables or disables automatically saving settings to EEPROM.
```
++savecfg [0|1]
```
`++savecfg`: Display current save setting.\
`++savecfg 0`: Disable automatic save of settings.\
`++savecfg 1`: Enable automatic save of settings.

*Note:*\
Settings saved are the following: `mode, addr, auto, eoi, eos, eot_enable, eot_char, read_tmo_ms`.\
Executing the `++savecfg 1` command will cause an immediate save of all settings to the EEPROM.\
Frequent writes to can cause the EEPROM to wear out, so this setting is always disabled automatically on power-up.\
<br/>

**Serial Poll**\
This command performs a serial poll of an instrument on the GPIB bus.
```
++spoll [<PAD> [<SAD>]]
```
`++spoll`: Serial poll currently addressed device.\
`++spoll 18`: Serial poll device with primary address 18.\
`++spoll 18 98`: Serial poll device with primary address 18 and secondary address 2.

*Note:*\
Valid primary address range is 1-30.\
Valid secondary address range is 96-126 representing 0-30. *(e.g. 96 = 0, 97=1, 98=2, etc.)*\
This command only applies when the GPIBUSB is in controller mode.\
<br/>

**Get SRQ Signal State**\
This command returns the current state of the GPIB SRQ signal. A 1 is returned if SRQ is asserted, and a 0 is returned if SRQ is not asserted.
```
++srq
```

*Note:*\
This command only applies when the GPIBUSB is in controller mode.\
<br/>

**Get/Set Device Status Byte**\
This command specifies the GPIB device status byte.
```
++status [0-255]
```
`++status`: Display current status byte value.\
`++status 22`: Set status byte to 22 (RQS not set).\
`++status 68`: Set status byte to 68 (RQS is set).

*Note:*\
If the bit 6 (RQS) is set in the status byte, the GPIB SRQ signal will be asserted.\
When a serial poll occurs, the GPIB SRQ signal will be de-asserted and the status byte will be set to zero.\
This command only applies when the GPIBUSB is in device mode.\
<br/>

**Send Group Execute Trigger**\
This command sends the Group Execute Trigger (GET) GPIB command to one or more instruments on the GPIB bus.
```
++trg [[<PAD1> [<SAD1>]] [<PAD2> [<SAD2>]] ... [<PAD15> [<SAD15>]]]
```
`++trg`: Send GET to currently addressed device.\
`++trg 18`: Send GET to device with primary address 18.\
`++trg 18 98`: Send GET to device with primary address 18 and secondary address 2.\
`++trg 18 22`: Send GET to devices with primary address 18 and 22.

*Note:*\
Up to 15 devices may be specified with this command.\
Valid primary address range is 1-30.\
Valid secondary address range is 96-126 representing 0-30. *(e.g. 96 = 0, 97=1, 98=2, etc.)*\
This command only applies when the GPIBUSB is in controller mode.\
<br/>

**Display Version String**\
This command returns the GPIBUSB version string of `GPIB-USB Version 6.00`.
```
++ver
```
<br/>

**Display Help String**\
This command returns the GPIBUSB help string of `Documentation: https://github.com/steve1515/gpibusb-firmware`.
```
++help
```
<br/>

*Note: Due to not having a 2nd controller available to test with, device mode has been implemented but not thoroughly tested.*

## Additional Command List

The following commands are unique to this firmware and are not part of the Prologix command set.\
<br/>

**Enable/Disable Debug Messages**\
This command enables or disables debugging messages.
```
++debug [0|1]
```
`++debug`: Display current debug setting.\
`++debug 0`: Disable debug messages.\
`++debug 1`: Enable debug messages.

*Note:*\
This setting defaults to 0 on power-up.

## License
This code is released under the [AGPLv3 license](LICENSE).

