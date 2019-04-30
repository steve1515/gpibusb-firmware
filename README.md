# GPIBUSB Adapter Firmware (Version 6.00)

This repository contains firmware source and hex files for the @Galvant Industries GPIBUSB adapter revision 3 and 4.

The goal of this project is to achieve near full compatibility with the [Prologix GPIB-USB adapter](http://prologix.biz/manuals.html).

The associated PCB project can be found at https://github.com/Galvant/gpibusb-pcb

Pre-assembled boards can be found at http://www.galvant.ca/

The source code requires the CCS compiler from https://www.ccsinfo.com/ to compile, but a pre-compiled hex file is included with the source code.

## Installation
The firmware of the GPIBUSB may be updated with the Tiny Bootloader:
[Tiny Bootloader Windows](http://www.etc.ugal.ro/cchiculita/software/tinyblddownload.htm)
[Tiny Bootloader Linux](http://tinybldlin.sourceforge.net/)

**Note:** Use a baud rate of 115200 when updating with the Tiny Bootloader.

## Communication Settings
**Baud Rate:** 460800
**Data Bits:** 8
**Stop Bits:** 1
**Parity:** None
**Flow Control:** None

## Data Transmission
- Characters received over USB are interpreted only once a termination character is received.

- Valid termination characters are **CR (ASCII 13)** and **LF (ASCII 10)**.

- Any **CR**, **LF**, **ESC** or **'+'** characters in the USB data to be sent to the GPIB bus must be escaped by preceding them with the **ESC (ASCII 27)** character.

- All un-escaped **CR**, **LF**, **ESC** and **'+'** characters received over USB are discarded.

- Any USB input starting with an un-escaped **"++"** character sequence is interpreted as a command and is not sent to the GPIB bus.

- Binary data may be sent or received, but as described above **CR**, **LF**, **ESC**, and **'+'** must be escaped in order to be sent to the GPIB bus.

## Command List
The following commands are compatible with the [Prologix Command Set](http://prologix.biz/manuals.html). Any differences will be noted below.

Command|Description|Prologix Differences
:---|:---|:---
`++addr [<PAD> [<SAD>]]`|Get/Set GPIB Address|
`++auto [0\|1]`|Enable/Disable Read-After-Write|
`++clr`|Send Selected Device Clear (SDC) GPIB Command|
`++eoi [0\|1]`|Enable/Disable EOI Assertion with Last Character|
`++eos [0\|1\|2\|3]`|Get/Set GPIB Termination Characters|
`++eot_enable [0\|1]`|Enable/Disable EOT Character when EOI Detected|
`++eot_char [<char>]`|Get/Set EOT Character|
`++ifc`|Send GPIB IFC Signal|
`++llo`|Send Local Lockout (LLO) GPIB Command|
`++loc`|Send Go To Local (GTL) GPIB Command|
`++lon [0\|1]`|Enable/Disable Listen Only Mode|
`++mode [0\|1]`|Get/Set Controller or Device Mode|
`++read_tmo_ms <time>`|Get/Set Timeout Value (mSec)|
`++read [eoi\|<char>]`|Read Data from Addressed Instrument|
`++rst`|Reset GPIBUSB Microprocessor|
`++savecfg [0\|1]`|Enable/Disable Saving to EEPROM|Default = Disabled on Power Up
`++spoll [<PAD> [<SAD>]]`|Serial Poll Instrument|
`++srq`|Get SRQ Signal State|
`++status [0-255]`|Get/Set Device Status Byte|
`++trg [[<PAD1> [<SAD1>]] [<PAD2> [<SAD2>]] ... [<PAD15> [<SAD15>]]]`|Send Group Execute Trigger (GET) GPIB Command|
`++ver`|Display Version String|Displays `GPIB-USB Version 6.00`
`++help`|Display Help String|Displays `Documentation: https://github.com/steve1515/gpibusb-firmware`

*Note: Due to not having a 2nd controller available to test with, device mode has been implemented but not thoroughly tested.*

The following commands are unique to this firmware and are not part of the Prologix command set.

Command|Description
:---|:---
`++debug [0\|1]`|Enable/Disable Debug Messages (Default = 0)

## License
This code is released under the AGPLv3 license.
