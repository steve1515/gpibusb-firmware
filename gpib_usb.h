/*****************************************************************************
Firmware for Galvant Industries GPIBUSB Adapter Revision 3 & 4
Copyright (C) 2019  Steve Matos

GPIBUSB adapter hardware designed by Steven Casagrande (scasagrande@galvant.ca)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

This code requires the CCS compiler from <https://www.ccsinfo.com/> to compile.
A pre-compiled hex file is included at
<https://github.com/steve1515/gpibusb-firmware>
*****************************************************************************/


#define DIO1 PIN_B0  // GPIB Data Input/Output Bit 1
#define DIO2 PIN_B1  // GPIB Data Input/Output Bit 2
#define DIO3 PIN_B2  // GPIB Data Input/Output Bit 3
#define DIO4 PIN_B3  // GPIB Data Input/Output Bit 4
#define DIO5 PIN_B4  // GPIB Data Input/Output Bit 5
#define DIO6 PIN_B5  // GPIB Data Input/Output Bit 6
#define DIO7 PIN_B6  // GPIB Data Input/Output Bit 7
#define DIO8 PIN_B7  // GPIB Data Input/Output Bit 8

#define REN PIN_E1   // GPIB Remote Enable
#define EOI PIN_A2   // GPIB End Or Identify
#define DAV PIN_A3   // GPIB Data Valid
#define NRFD PIN_A4  // GPIB Not Ready For Data
#define NDAC PIN_A5  // GPIB Not Data Accepted
#define ATN PIN_A1   // GPIB Attention
#define SRQ PIN_A0   // GPIB Service Request
#define IFC PIN_E0   // GPIB Interface Clear

#define SC PIN_D7  // SN75162B System Control
#define TE PIN_D6  // SN75160B/SN75162B Talk Enable
#define PE PIN_D5  // SN75160B Pullup Enable
#define DC PIN_D4  // SN75162B Direction Control

#define LED_ERROR PIN_C5  // LED Indicator

