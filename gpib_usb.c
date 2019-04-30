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


#include <18F4520.h>
#fuses HS, NOPROTECT, NOLVP, WDT, WDT4096
#use standard_io(all)
#use delay(clock=18432000)
#use rs232(baud=460800,uart1)

#case
#ignore_warnings 204  // Ignore 'Condition always FALSE' warning for debug_printf() and eot_printf()

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "gpib_usb.h"

//#define VERBOSE_DEBUG

#define VERSION_MAJOR   6
#define VERSION_MINOR_A 0
#define VERSION_MINOR_B 0

// The EEPROM version code occupies the first byte in EEPROM. If the code in
// EEPROM differs from the string defined below, then the default values for
// all EEPROM configuration values along with the defined version string will
// be written to EEPROM on startup.
#define EEPROM_VERSION_CODE 0xA1


#define CR  0x0d  // Carriage Return
#define LF  0x0a  // Line Feed
#define ESC 0x1b  // Escape
#define TAB 0x09  // Tab
#define SP  0x20  // Space


// GPIB Command Bytes (See IEEE 488.1 and IEEE 488.2)
#define GPIB_CMD_GTL 0x01  // Go To Local
#define GPIB_CMD_SDC 0x04  // Selected Device Clear
#define GPIB_CMD_PPC 0x05  // PPC Parallel Poll Configure
#define GPIB_CMD_GET 0x08  // Group Execute Trigger
#define GPIB_CMD_TCT 0x09  // TCT Take Control
#define GPIB_CMD_LLO 0x11  // Local Lockout
#define GPIB_CMD_DCL 0x14  // Device Clear
#define GPIB_CMD_PPU 0x15  // PPU Parallel Poll Unconfigure
#define GPIB_CMD_SPE 0x18  // Serial Poll Enable
#define GPIB_CMD_SPD 0x19  // Serial Poll Disable
#define GPIB_CMD_MLA 0x20  // Device Listen Address (MLA)
#define GPIB_CMD_MTA 0x40  // Device Talk Address (MTA)
#define GPIB_CMD_UNL 0x3f  // Unlisten
#define GPIB_CMD_UNT 0x5f  // Untalk
#define GPIB_CMD_PPE 0x60  // PPE Parallel Poll Enable
#define GPIB_CMD_PPD 0x70  // PPD Parallel Poll Disable


#define CONTROLLER_ADDR 0  // Controller GPIB address (always zero)

#define MODE_DEVICE     0
#define MODE_CONTROLLER 1

#define EOS_CR_LF 0
#define EOS_CR    1
#define EOS_LF    2
#define EOS_NONE  3

#define READ_TO_TIMEOUT 0
#define READ_TO_EOI     1
#define READ_TO_CHAR    2


// Note: UART receive ring buffer length of 256 allows for easy rollover of indexes.
//       Do not change buffer length!
#define BUFFER_LEN 256
uint8_t _ringBuffer[BUFFER_LEN];
uint8_t _recvBuffer[BUFFER_LEN];
volatile uint8_t _ringBufferRead = 0;
volatile uint8_t _ringBufferWrite = 0;

bool _debugMode = false;  // True = display user-level debugging messages

uint8_t _gpibMode = MODE_CONTROLLER;

// Address variables represent either the target address in controller mode
// or this device's address in device mode.
// Note: SAD has no effect in device mode.
uint8_t _devicePad = 1;      // Device primary address (PAD)
uint8_t _deviceSad = 0;      // Device secondary address (SAD)
bool _useDeviceSad = false;  // True if device has a secondary address (SAD)

bool _autoRead = true;
bool _useEoi = true;
uint8_t _eosMode = EOS_CR_LF;
bool _eotEnable = true;
char _eotChar = LF;
bool _listenOnlyMode = false;
uint8_t _deviceStatusByte = 0x00;
bool _saveCfgEnable = false;

uint16_t _gpibTimeout = 1000;
uint16_t _mSecTimer = 0;

char _eosBuffer[] = "\r\n";

// Device Mode State Variables
bool _deviceTalk = false;        // True = device addressed as talker
bool _deviceListen = false;      // True = device addressed as listener
bool _deviceSerialPoll = false;  // True = serial poll mode enabled


// Prologix Compatible Command Set
char _cmdAddr[]      = "addr";         // ++addr [<PAD> [<SAD>]]
char _cmdAuto[]      = "auto";         // ++auto [0|1]
char _cmdClr[]       = "clr";          // ++clr
char _cmdEoi[]       = "eoi";          // ++eoi [0|1]
char _cmdEos[]       = "eos";          // ++eos [0|1|2|3]
char _cmdEotEnable[] = "eot_enable";   // ++eot_enable [0|1]
char _cmdEotChar[]   = "eot_char";     // ++eot_char [<char>]
char _cmdIfc[]       = "ifc";          // ++ifc
char _cmdLlo[]       = "llo";          // ++llo
char _cmdLoc[]       = "loc";          // ++loc
char _cmdLon[]       = "lon";          // ++lon [0|1]
char _cmdMode[]      = "mode";         // ++mode [0|1]
char _cmdReadTmoMs[] = "read_tmo_ms";  // ++read_tmo_ms <time>
char _cmdRead[]      = "read";         // ++read [eoi|<char>]
char _cmdRst[]       = "rst";          // ++rst
char _cmdSavecfg[]   = "savecfg";      // ++savecfg [0|1]
char _cmdSpoll[]     = "spoll";        // ++spoll [<PAD> [<SAD>]]
char _cmdSrq[]       = "srq";          // ++srq
char _cmdStatus[]    = "status";       // ++status [0-255]
char _cmdTrg[]       = "trg";          // ++trg [[<PAD1> [<SAD1>]] [<PAD2> [<SAD2>]] ... [<PAD15> [<SAD15>]]]
char _cmdVer[]       = "ver";          // ++ver
char _cmdHelp[]      = "help";         // ++help

// Additional Commands
char _cmdDebug[]     = "debug";        // ++debug [0|1]


#define debug_printf(fmt, ...) do {\
    if (_debugMode)\
    {\
        printf((fmt), ##__VA_ARGS__);\
        if (_eotEnable) printf("%c", _eotChar);\
    }\
} while (0)


#define eot_printf(fmt, ...) do {\
    printf((fmt), ##__VA_ARGS__);\
    if (_eotEnable) printf("%c", _eotChar);\
} while (0)


bool buffer_get(uint8_t *buffer);
char* trim_right(char *str);
char* get_address(char *buffer, uint8_t *pad, uint8_t *sad, uint8_t *validSad);
void handle_command(uint8_t *buffer);
void handle_device_mode();
void handle_listen_only_mode();
#inline void update_eeprom(int8_t address, int8_t value);
void eeprom_read_cfg();
void eeprom_write_cfg();
void gpib_init_pins(uint8_t mode);
#inline void gpib_send_ifc();
bool gpib_read_status_byte(uint8_t *statusByte, uint8_t pad, uint8_t sad, bool useSad);
#inline bool gpib_send_command(uint8_t command);
#inline bool gpib_send_data(uint8_t *buffer, uint8_t length, bool useEoi);
bool gpib_send_setup(uint8_t pad, uint8_t sad, bool useSad);
bool gpib_send(uint8_t *buffer, uint8_t length, bool isCommand, bool useEoi);
bool gpib_receive_setup(uint8_t pad, uint8_t sad, bool useSad);
bool gpib_receive_byte(char *buffer, uint8_t *eoiStatus);
void gpib_receive_data(uint8_t readMode, char readToChar);


void main()
{
#ifdef VERBOSE_DEBUG
    // Get microcontroller restart cause.
    // Note: This must be done before any other registers are modified.
    uint8_t restartCause = restart_cause();
    switch (restartCause)
    {
        case WDT_TIMEOUT:
            eot_printf("Restart Cause: Watchdog Timeout");
            break;
            
        case NORMAL_POWER_UP:
            eot_printf("Restart Cause: Normal Power Up");
            break;
            
        case MCLR_FROM_RUN:
            eot_printf("Restart Cause: Reset Push-button");
            break;
            
        case RESET_INSTRUCTION:
            eot_printf("Restart Cause: Reset Instruction");
            break;
            
        default:
            eot_printf("Restart Cause: Other (%u)", restartCause);
            break;
    }
#endif
    
    // Turn on error LED
    output_high(LED_ERROR);
    
    // Setup watchdog timer
    setup_wdt(WDT_ON);
    
    // Setup timeout timer
    set_rtcc(0);
    setup_timer_2(T2_DIV_BY_16, 144, 2);  // 1 mSec interrupt
    enable_interrupts(GLOBAL);
    disable_interrupts(INT_TIMER2);
    
    // Read EEPROM configuration values
    eeprom_read_cfg();

    // Initialize GPIB bus lines
    gpib_init_pins(_gpibMode);
    if (_gpibMode == MODE_CONTROLLER)
        gpib_send_ifc();
    
    // Delay before enabling RDA interrupt.
    // Note: Delaying the enable of the RDA interrupt solves some issues
    //       on Linux operating systems where the "modemmanager" package
    //       is installed. The "modemmanager" package appears to cause
    //       a ~30 second delay where the serial port is unaccessible.
    
    // Blink LED during delay
    output_low(LED_ERROR);
    restart_wdt(); delay_ms(100);
    output_high(LED_ERROR);
    restart_wdt(); delay_ms(100);
    
    enable_interrupts(INT_RDA);
    restart_wdt();
    output_low(LED_ERROR);
    
    // Main Loop
    for (;;)
    {
        restart_wdt();
        
        // Check for data in UART receive buffer and process as required
        if (buffer_get(_recvBuffer))
        {
            // Check if the received data is a controller command sequence (++ command)
            // Note: First byte in receive buffer is the control
            //       command flag (CCF). If CCF == 1, then data is a command.
            if (_recvBuffer[0])
            {
                handle_command(_recvBuffer);
            }
            else  // Not an internal controller command sequence
            {
                uint8_t dataLen = _recvBuffer[1];
                uint8_t *pBuf = _recvBuffer+2;
                
                if (_gpibMode == MODE_CONTROLLER)
                {
                    bool errorStatus = false;
                    
                    // Address target device and send data
                    errorStatus = errorStatus || gpib_send_setup(_devicePad, _deviceSad, _useDeviceSad);
                    errorStatus = errorStatus || gpib_send_data(pBuf, dataLen, _useEoi);
                    
                    // Automatically read after sending data if auto read mode is enabled
                    if (_autoRead)
                    {
                        errorStatus = errorStatus || gpib_receive_setup(_devicePad, _deviceSad, _useDeviceSad);
                        if (!errorStatus)
                            gpib_receive_data(READ_TO_EOI, NULL);
                    }
                }
                else  // Device mode
                {
                    // Sending data is only allowed when addressed to talk,
                    // serial poll mode disabled, and ATN deasserted.
                    // Reference: IEEE 488.1-1987 - Section 2.5.2 T Function State Diagrams
                    if (_deviceTalk && !_deviceSerialPoll && input(ATN))
                        gpib_send_data(pBuf, dataLen, _useEoi);
                }
            }
        }
        
        // Handle device mode processing
        if (_gpibMode == MODE_DEVICE)
        {
            if (_listenOnlyMode)
                handle_listen_only_mode();
            else    
                handle_device_mode();
        }
    }
}


#int_timer2
void clock_isr()
{
    _mSecTimer++;
}


#int_rda
void RDA_isr()
{
    // This interrupt handler takes incoming UART data and fills a ring buffer
    // further processing in the main loop.
    
    // Ring Buffer Format
    // ==================
    //  - Read index points to buffer index of next read.
    //
    //  - Write index points to buffer index of next free byte.
    //
    //  - Buffer empty is indicated by Read Index == Write Index.
    //
    //  - Buffer size is 256 bytes and index pointers are unsigned 8-bit values,
    //    so wrap arounds are automatically handled by binary 8-bit arithmetic.
    //
    //  - When data is available in the ring buffer, the read index points
    //    to a controller command flag, followed by a data byte length,
    //    followed by data bytes.
    //
    //    Example (where read pointer points to byte 0):
    //    | Byte 0 | Byte 1 | Byte 2 | Byte 3 | ... | Byte N |
    //    |  CCF   |  DLEN  |   D1   |   D2   | ... |   DN   |
    //    where...
    //    CCF = Control Command Flag (1 = Controller Command; 0 = Device Data)
    //    DLEN = Data Length in Bytes
    //    D1..DN = Data of size DLEN bytes
    
    // UART Data Notes
    // ===============
    //  - All un-escaped LF (0x0a), CR (0x0d), ESC (0x1b), and '+' characters
    //    are discarded.
    //
    //  - Any UART input that starts with an un-escaped '++' character sequence
    //    is interpreted as a controller command and not transmitted over GPIB.


    // Do nothing if no data is ready
    if (!kbhit())
        return;
    
    uint8_t startIndex = _ringBufferWrite;
    uint8_t readNum = 0;
    uint8_t byteLen = 0;
    bool escapeNext = false;
    char c;
    char c1 = '\0';
    char c2 = '\0';
    
    for (;;)
    {
        // Get character from UART
        c = getc();
        readNum++;
        
        // Save 1st and 2nd characters received.
        // Note: These characters will be used later to determine if the received
        //       string is a controller command.
        if (readNum == 1)
            c1 = c;
            
        if (readNum == 2)
            c2 = c;
        
        // If the escape flag is not set and an escape character is
        // received, set the escape flag for the next character.
        // Note: Checking that the escape flag is not set,
        //       allows escaping of the escape character.
        if (!escapeNext && c == ESC)
        {
            escapeNext = true;
            continue;
        }
        
        // Discard un-escaped '+' characters
        if (!escapeNext && c == '+')
            continue;
        
        // Exit loop if un-escaped termination character (CR or LF) is received
        if (!escapeNext && (c == CR || c == LF))
            break;
        
        // Before adding the first character to the buffer below,
        // advance the ring buffer write pointer 2 positions.
        // Note: The 1st and 2nd bytes are used for the controller command flag
        //       and data length size respectively.
        if (byteLen == 0)
            _ringBufferWrite += 2;
        
        // Add character to buffer (if escaped or a character other then ESC, '+', CR, LF)
        _ringBuffer[_ringBufferWrite] = c;
        _ringBufferWrite++;
        byteLen++;
        escapeNext = false;
        
        // If data added to the ring buffer has caused the pointers to become
        // equal, then this means the buffer is full and will overflow if more
        // bytes are added. We also don't allow the pointers to be equal unless
        // the buffer is empty, so this data must be discarded and the write
        // pointer must be reset to where it was before data was added to the
        // ring buffer.
        // Simply put, pointers being equal means empty not full,
        // so we must discard the incoming data.
        if (_ringBufferRead == _ringBufferWrite)
        {
            byteLen = 0;
            _ringBufferWrite = startIndex;
            break;
        }        
    }
    
    // Consume any additional bytes (flush receive buffer)
    while (kbhit())
        getc();
    
    // Do nothing if no bytes were added to the buffer
    if (byteLen == 0)
        return;
    
    // Set controller command flag if first two characters received were '++'
    _ringBuffer[startIndex] = (c1 == '+' && c2 == '+') ? 0x01 : 0x00;
    
    // Set data byte length
    _ringBuffer[(uint8_t)(startIndex + 1)] = byteLen;
}


bool buffer_get(uint8_t *buffer)
{
    // This function gets an item from the ring buffer and places it into
    // the buffer given.
    // It returns true if successful, false otherwise.
    // Note: It is assumed that the destination buffer given is the same
    //       size as the ring buffer. This prevents the data from filling
    //       the destination buffer completely due to the fact that the
    //       pointers will only allow the ring buffer to be filled to
    //       its size minus one (e.g. pointers can never be equal if data
    //       is in the ring buffer, so there is always at least one free byte).
    //       This ensures that there will always be enough space to add a null
    //       terminator on the end of the received data.


    // Return false if buffer is empty
    if (_ringBufferRead == _ringBufferWrite)
        return false;
    
    // Get byte length of data to copy in input buffer (CCF + DLEN + Data)
    uint8_t byteLen = _ringBuffer[(uint8_t)(_ringBufferRead + 1)] + 2;
    
    // Zero destination buffer
    // Note: This allows string read functions to work since any data
    //       copied into the destination buffer will be null terminated.
    memset(buffer, 0, BUFFER_LEN);
    
    // Check if the data to be read will wrap around the
    // ring buffer, and perform two copies if required.
    if (((uint16_t)_ringBufferRead + byteLen) > BUFFER_LEN)
    {
        uint8_t readLen1 = BUFFER_LEN - _ringBufferRead;
        uint8_t readLen2 = byteLen - readLen1;
        
        memcpy(buffer, &_ringBuffer[_ringBufferRead], readLen1);
        memcpy(&buffer[readLen1], _ringBuffer, readLen2);
    }
    else
    {
        memcpy(buffer, &_ringBuffer[_ringBufferRead], byteLen);
    }

    // Update the read pointer
    _ringBufferRead += byteLen;
    
    return true;
}


char* trim_right(char *str)
{
    // This function trims whitespace from the right side of the given string.
    // Note: The given string is modified in place.
    
    uint8_t len, i;
    
    len = strlen(str);
    if (len < 1)
        return str;
    
    // Trim trailing spaces and tabs
    i = strlen(str) - 1;
    while (i >= 0 && (str[i] == SP || str[i] == TAB))
    {
        str[i] = '\0';
        i--;
    }
    
    return str;
}


char* get_address(char *buffer, uint8_t *pad, uint8_t *sad, uint8_t *validSad)
{
    // This function returns the next PAD and SAD if available from the given string.
    //
    // Parameters:
    //   [in]  buffer:   Buffer containing string to search for PAD and SAD
    //   [out] pad:      Primary address (PAD) of device [Valid Range = 1-30]
    //   [out] sad:      Secondary address (SAD) of device [Valid Range = 0-30]
    //   [out] validSad: 1 = SAD was found
    //
    // Return Value: Pointer to next PAD in buffer, otherwise NULL (See Notes)
    //
    // Notes:
    //   1. The input buffer must be NULL terminated.
    //   2. The input buffer may contain multiple PADs or PAD/SAD combinations.
    //   3. If the input buffer contains multiple PAD/SADs, each value must be
    //      separated by one or more space characters (0x20).
    //   4. If an invalid PAD is encountered, NULL will be returned and
    //      pad will be set to zero.
    //   5. If an invalid SAD is encountered, NULL will be returned and
    //      validSad will be set to 0.
    
    
    // Initialize output values
    *pad = 0;
    *sad = 0;
    *validSad = 0;
    
    char *pBuf = buffer;
    uint8_t value;

    // Consume any leading spaces
    while (*pBuf == SP)
        pBuf++;
        
    // If no following characters are found (end of string), return NULL
    if (pBuf == NULL)
        return NULL;
    
    // Get PAD
    value = atoi(pBuf);
    
    // If PAD is not valid, return NULL
    if (value < 1 || value > 30)
        return NULL;
        
    // Valid PAD found
    *pad = value;
    
    // Search for next space character
    pBuf = strchr(pBuf, SP);
    
    // If no following space character is found, return NULL
    if (pBuf == NULL)
        return NULL;
        
    // Consume any leading spaces
    while (*pBuf == SP)
        pBuf++;
        
    // If no following characters are found (end of string), return NULL
    if (pBuf == NULL)
        return NULL;
        
    // Get next value
    // Note: This could be a PAD or a SAD
    value = atoi(pBuf);
    
    // If value is PAD (1-30), return pointer (no SAD found)
    if (value >=1 && value <= 30)
        return pBuf;
    
    // If value is not a valid SAD, return NULL
    if (value < 96 || value > 126)
        return NULL;
        
    // Valid SAD found
    // Note: User enters 96-126 to indicate SAD of 0-30, so 0x60 is subtracted
    //       before internally storing the value as 0-30.
    *sad = value - 0x60;
    *validSad = 1;
    
    // Search for next space character
    pBuf = strchr(pBuf, SP);
    
    // If no following space character is found, return NULL
    if (pBuf == NULL)
        return NULL;
        
    // Consume any leading spaces
    while (*pBuf == SP)
        pBuf++;
        
    // Return pointer
    // Note: This may be a NULL if end of string was reached.
    return pBuf;
}


void handle_command(uint8_t *buffer)
{
    // This function handles a controller command sequence (++ command).
    //
    // Parameters:
    //   [in] buffer: Byte buffer containing a command sequence
    
    
    // Verify that the CCF flag is set and data length > 0
    if (!buffer[0] || buffer[1] < 1)
        return;
    
    // Get a pointer to the data section of the buffer
    char *pBuf = trim_right(&buffer[2]);
    
#ifdef VERBOSE_DEBUG
    eot_printf("Trimmed Command String: '%s'", pBuf);
#endif
    
    // ++addr [<PAD> [<SAD>]]    
    if (!strncmp(pBuf, _cmdAddr, 4))
    {        
        if (*(pBuf+4) == '\0')  // Query current address
        {
            if (_useDeviceSad)
                eot_printf("%u %u", _devicePad, _deviceSad + 0x60);
            else
                eot_printf("%u", _devicePad);
        }
        else if (*(pBuf+4) == SP)  // Set address
        {
            uint8_t pad, sad, validSad;
            get_address(pBuf+5, &pad, &sad, &validSad);
            
            // If PAD was found valid, update address variables
            if (pad > 0)
            {
                _devicePad = pad;
                _deviceSad = sad;
                _useDeviceSad = validSad;
                
                if (_saveCfgEnable)
                    eeprom_write_cfg();
            }
        }
    }
    
    // ++auto [0|1]
    else if (_gpibMode == MODE_CONTROLLER && !strncmp(pBuf, _cmdAuto, 4))
    {
        if (*(pBuf+4) == '\0')     // Query current auto read mode
        {
            eot_printf("%u", _autoRead);
        }
        else if (*(pBuf+4) == SP)  // Set auto read mode
        {
            _autoRead = atoi(pBuf+5) > 0;
            
            if (_saveCfgEnable)
                eeprom_write_cfg();
        }
    }
    
    // ++clr
    else if (_gpibMode == MODE_CONTROLLER && !strncmp(pBuf, _cmdClr, 3))
    {
        bool errorStatus = false;
        errorStatus = errorStatus || gpib_send_setup(_devicePad, _deviceSad, _useDeviceSad);
        errorStatus = errorStatus || gpib_send_command(GPIB_CMD_SDC);
    }
    
    // ++eoi [0|1]
    else if (!strncmp(pBuf, _cmdEoi, 3))
    {
        if (*(pBuf+3) == '\0')     // Query current EOI mode
        {
            eot_printf("%u", _useEoi);
        }
        else if (*(pBuf+3) == SP)  // Set EOI mode
        {
            _useEoi = atoi(pBuf+4) > 0;
            
            if (_saveCfgEnable)
                eeprom_write_cfg();
        }
    }
    
    // ++eos [0|1|2|3]
    else if (!strncmp(pBuf, _cmdEos, 3))
    {
        if (*(pBuf+3) == '\0')     // Query current EOS mode
        {
            eot_printf("%u", _eosMode);
        }
        else if (*(pBuf+3) == SP)  // Set EOS mode
        {
            uint8_t value = atoi(pBuf+4);
            
            // Only accept valid values
            if (value >= 0 && value <= 3)
            {
                _eosMode = value;
                
                if (_saveCfgEnable)
                    eeprom_write_cfg();
            }
        }
    }
    
    // ++eot_enable [0|1]
    else if (!strncmp(pBuf, _cmdEotEnable, 10))
    {
        if (*(pBuf+10) == '\0')     // Query current EOT mode
        {
            eot_printf("%u", _eotEnable);
        }
        else if (*(pBuf+10) == SP)  // Set EOT mode
        {
            _eotEnable = atoi(pBuf+11) > 0;
            
            if (_saveCfgEnable)
                eeprom_write_cfg();
        }
    }
    
    // ++eot_char [<char>]
    else if (!strncmp(pBuf, _cmdEotChar, 8))
    {
        if (*(pBuf+8) == '\0')     // Query current EOT character
        {
            eot_printf("%u", _eotChar);
        }
        else if (*(pBuf+8) == SP)  // Set EOT character
        {
            _eotChar = atoi(pBuf+9);
            
            if (_saveCfgEnable)
                eeprom_write_cfg();
        }
    }
    
    // ++ifc
    else if (_gpibMode == MODE_CONTROLLER && !strncmp(pBuf, _cmdIfc, 3))
    {
        gpib_send_ifc();
    }
    
    // ++llo
    else if (_gpibMode == MODE_CONTROLLER && !strncmp(pBuf, _cmdLlo, 3))
    {
        bool errorStatus = false;
        errorStatus = errorStatus || gpib_send_setup(_devicePad, _deviceSad, _useDeviceSad);
        errorStatus = errorStatus || gpib_send_command(GPIB_CMD_LLO);
    }
    
    // ++loc
    else if (_gpibMode == MODE_CONTROLLER && !strncmp(pBuf, _cmdLoc, 3))
    {
        bool errorStatus = false;
        errorStatus = errorStatus || gpib_send_setup(_devicePad, _deviceSad, _useDeviceSad);
        errorStatus = errorStatus || gpib_send_command(GPIB_CMD_GTL);
    }
    
    // ++lon [0|1]
    else if (_gpibMode == MODE_DEVICE && !strncmp(pBuf, _cmdLon, 3))
    {
        if (*(pBuf+3) == '\0')     // Query current listen only mode
            eot_printf("%u", _listenOnlyMode);
        else if (*(pBuf+3) == SP)  // Set listen only mode
            _listenOnlyMode = atoi(pBuf+4) > 0;
    }
    
    // ++mode [0|1]
    else if (!strncmp(pBuf, _cmdMode, 4))
    {
        if (*(pBuf+4) == '\0')     // Query current mode
        {
            eot_printf("%u", _gpibMode);
        }
        else if (*(pBuf+4) == SP)  // Set mode
        {
            uint8_t value = atoi(pBuf+5);
            
            // Set new mode only if mode is changed and in valid range
            if (_gpibMode != value && value >= 0 && value <= 1)
            {
                _gpibMode = value;
                gpib_init_pins(_gpibMode);
                _listenOnlyMode = false;
                _deviceTalk = false;
                _deviceListen = false;
                _deviceSerialPoll = false;
                _deviceStatusByte = 0x00;
                
                if (_gpibMode == MODE_CONTROLLER)
                    gpib_send_ifc();
                    
                if (_saveCfgEnable)
                    eeprom_write_cfg();
            }
        }
    }
    
    // Note: The processing of '++read_tmo_ms' must come before '++read' or
    //       else it will never get processed.
    
    // ++read_tmo_ms <time>
    else if (!strncmp(pBuf, _cmdReadTmoMs, 11))
    {
        if (*(pBuf+11) == '\0')     // Query current timeout
        {
            eot_printf("%lu", _gpibTimeout);
        }
        else if (*(pBuf+11) == SP)  // Set timeout
        {
            uint32_t value = atoi32(pBuf+12);
            
            // Only accept valid values
            if (value >= 0 && value <= 3000)
            {
                _gpibTimeout = (uint16_t)value;
                
                if (_saveCfgEnable)
                    eeprom_write_cfg();
            }
        }
    }
    
    // ++read [eoi|<char>]
    else if (_gpibMode == MODE_CONTROLLER && !strncmp(pBuf, _cmdRead, 4))
    {
        if (*(pBuf+4) == '\0')                                            // Read until timeout
        {
            if (!gpib_receive_setup(_devicePad, _deviceSad, _useDeviceSad))
                gpib_receive_data(READ_TO_TIMEOUT, NULL);
        }
        else if (*(pBuf+4) == SP
            && *(pBuf+5) == 'e' && *(pBuf+6) == 'o' && *(pBuf+7) == 'i')  // Read until EOI (or timeout)
        {
            if (!gpib_receive_setup(_devicePad, _deviceSad, _useDeviceSad))
                gpib_receive_data(READ_TO_EOI, NULL);
        }
        else if (*(pBuf+4) == SP)                                         // Read until character (or timeout)
        {
            char c = atoi(pBuf+5);
        
            if (!gpib_receive_setup(_devicePad, _deviceSad, _useDeviceSad))
                gpib_receive_data(READ_TO_CHAR, c);
        }
    }
    
    // ++rst
    else if (!strncmp(pBuf, _cmdRst, 3))
    {
        delay_ms(1);
        reset_cpu();
    }
    
    // ++savecfg [0|1]
    else if (!strncmp(pBuf, _cmdSavecfg, 7))
    {
        if (*(pBuf+7) == '\0')     // Query current save configuration mode
        {
            eot_printf("%u", _saveCfgEnable);
        }
        else if (*(pBuf+7) == SP)  // Set save configuration mode
        {
            _saveCfgEnable = atoi(pBuf+8) > 0;
            
            // Save immediately when "++savecfg 1" is received
            if (_saveCfgEnable)
                eeprom_write_cfg();
        }
    }
    
    // ++spoll [<PAD> [<SAD>]]
    else if (_gpibMode == MODE_CONTROLLER && !strncmp(pBuf, _cmdSpoll, 5))
    {
        if (*(pBuf+5) == '\0')  // Serial poll currently addressed device
        {
            uint8_t statusByte = 0x00;
            if (!gpib_read_status_byte(&statusByte, _devicePad, _deviceSad, _useDeviceSad))
                putc(statusByte);
        }
        else if (*(pBuf+5) == SP)  // Serial poll specified device address
        {
            uint8_t pad, sad, validSad;
            uint8_t statusByte = 0x00;
            
            get_address(pBuf+6, &pad, &sad, &validSad);
            
            if (pad > 0 && !gpib_read_status_byte(&statusByte, pad, sad, validSad))
                putc(statusByte);
        }
    }
    
    // ++srq
    else if (_gpibMode == MODE_CONTROLLER && !strncmp(pBuf, _cmdSrq, 3))
    {
        eot_printf("%u", !input(SRQ));
    }
    
    // ++status [0-255]
    else if (_gpibMode == MODE_DEVICE && !strncmp(pBuf, _cmdStatus, 6))
    {  
        if (*(pBuf+6) == '\0')     // Query current status byte
        {
            eot_printf("%u", _deviceStatusByte);
        }
        else if (*(pBuf+6) == SP)  // Set status byte
        {
            _deviceStatusByte = atoi(pBuf+7);
            
            // When RQS (bit 6) is set, assert SRQ
            if (_deviceStatusByte & 0x40)
                output_low(SRQ);
            else
                output_high(SRQ);
        }
    }
    
    // ++trg [[<PAD1> [<SAD1>]] [<PAD2> [<SAD2>]] ... [<PAD15> [<SAD15>]]]
    else if (_gpibMode == MODE_CONTROLLER && !strncmp(pBuf, _cmdTrg, 3))
    {
        if (*(pBuf+3) == '\0')  // Send GPIB GET to currently addressed device
        {
            bool errorStatus = false;
            errorStatus = errorStatus || gpib_send_setup(_devicePad, _deviceSad, _useDeviceSad);
            errorStatus = errorStatus || gpib_send_command(GPIB_CMD_GET);
        }
        else if (*(pBuf+3) == SP)  // Send GPIB GET to specified device addresses
        {
            uint8_t pad, sad, validSad;
            bool errorStatus = false;
            pBuf = pBuf+4;
        
            // Send to a maximum of 15 device addresses
            for (uint8_t i = 0; i < 15; i++)
            {
                restart_wdt();
                
                pBuf = get_address(pBuf, &pad, &sad, &validSad);
                
                // Exit loop if invalid PAD was found
                if (pad < 1)
                    break;
                    
                errorStatus = false;
                errorStatus = errorStatus || gpib_send_setup(pad, sad, validSad);
                errorStatus = errorStatus || gpib_send_command(GPIB_CMD_GET);    
                
                // Exit loop if no more addresses were given
                if (pBuf == NULL)
                    break;
            }
        }
    }
    
    // ++ver
    else if (!strncmp(pBuf, _cmdVer, 3))
    {
        eot_printf("GPIB-USB Version %u.%u%u",
            VERSION_MAJOR, VERSION_MINOR_A, VERSION_MINOR_B);
    }
    
    // ++help
    else if (!strncmp(pBuf, _cmdHelp, 4))
    {
        eot_printf("Documentation: https://github.com/steve1515/gpibusb-firmware");
    }
    
    // ++debug [0|1]
    else if (!strncmp(pBuf, _cmdDebug, 5))
    {
        if (*(pBuf+5) == '\0')     // Query current debug mode
            eot_printf("%u", _debugMode);
        else if (*(pBuf+5) == SP)  // Set debug mode
            _debugMode = atoi(pBuf+6) > 0;
    }
    
    // ++<unkonwn>
    else
    {
        debug_printf("Unrecognized command.");
    }
}


void handle_device_mode()
{
    // This function handles setting device mode states and associated
    // command handling.
    //
    // References:
    //   IEEE 488.1-1987 - 2.5 Talker (T) Interface Function (Includes Serial Poll Capabilities)
    //   IEEE 488.1-1987 - 2.6 Listener (L) Interface Function
    //   IEEE 488.1-1987 - 2.7 Service Request (SR) Interface Function
    //   IEEE 488.1-1987 - 2.8 Remote Local (RL) Interface Function
    //   IEEE 488.1-1987 - 2.10 Device Clear (DC) Interface Function
    //   IEEE 488.1-1987 - 2.11 Device Trigger (DT) Interface Function
    
    
    // Reset device state if IFC is asserted
    if (!input(IFC))
    {
        _deviceTalk = false;
        _deviceListen = false;
        _deviceSerialPoll = false;
        _deviceStatusByte = 0x00;
        return;
    }
    
    // If ATN is asserted we must wait for a command from the controller
    if (!input(ATN))
    {
        // Set GPIB lines for receiving
        output_float(DIO1);
        output_float(DIO2);
        output_float(DIO3);
        output_float(DIO4);
        output_float(DIO5);
        output_float(DIO6);
        output_float(DIO7);
        output_float(DIO8);
        
        output_float(DAV);
        output_float(EOI);
        output_low(TE);
        output_low(NDAC);
        output_high(NRFD);  // Indicate ready for data
    
        // Do nothing if ATN is asserted, but DAV is deasserted (waiting for command)
        if (input(DAV))
            return;
            
        // Read command byte (Do nothing if read fails)
        uint8_t cmdByte = 0x00;
        uint8_t eoiStatus;
        if (gpib_receive_byte(&cmdByte, &eoiStatus))
            return;
            
        // GTL - Go To Local    
        if (cmdByte == GPIB_CMD_GTL && _deviceListen)
        {
            eot_printf("GPIB_CMD_GTL");
        }
        
        // SDC - Selected Device Clear
        else if (cmdByte == GPIB_CMD_SDC && _deviceListen)
        {
            eot_printf("GPIB_CMD_SDC");
            _deviceTalk = false;
            _deviceListen = false;
            _deviceSerialPoll = false;
            _deviceStatusByte = 0x00;
        }
        
        // GET - Group Execute Trigger
        else if (cmdByte == GPIB_CMD_GET && _deviceListen)
        {
            eot_printf("GPIB_CMD_GET");
        }
        
        // LLO - Local Lockout
        else if (cmdByte == GPIB_CMD_LLO && _deviceListen)
        {
            eot_printf("GPIB_CMD_LLO");
        }
        
        // DCL - Device Clear
        else if (cmdByte == GPIB_CMD_DCL)
        {
            eot_printf("GPIB_CMD_DCL");
            _deviceTalk = false;
            _deviceListen = false;
            _deviceSerialPoll = false;
            _deviceStatusByte = 0x00;
        }
        
        // SPE - Serial Poll Enable
        else if (cmdByte == GPIB_CMD_SPE)
        {
            _deviceSerialPoll = true;
        }
        
        // SPD - Serial Poll Disable
        else if (cmdByte == GPIB_CMD_SPD)
        {
            _deviceSerialPoll = false;
        }
        
        // MLA - Device Listen Address
        else if ((cmdByte & 0xe0) == GPIB_CMD_MLA)
        {
            // Listen and Untalk if this device was addressed
            if ((cmdByte & 0x1f) == _devicePad)
            {
                _deviceTalk = false;
                _deviceListen = true;
            }
        }
        
        // MTA - Device Talk Address
        else if ((cmdByte & 0xe0) == GPIB_CMD_MTA)
        {
            // Talk and Unlisten if this device was addressed
            if ((cmdByte & 0x1f) == _devicePad)
            {
                _deviceTalk = true;
                _deviceListen = false;
            }
            
            // Untalk if another device was addressed
            else
            {
                _deviceTalk = false;
            }
        }
        
        // UNL - Unlisten
        else if (cmdByte == GPIB_CMD_UNL)
        {
            _deviceListen = false;
        }
        
        // UNT - Untalk
        else if (cmdByte == GPIB_CMD_UNT)
        {
            _deviceTalk = false;
        }
    }
    
    // If ATN is deasserted, we can resume normal operation
    else
    {
        // Set GPIB lines for sending if addressed to talk
        if (_deviceTalk)
        {
            output_float(NDAC);
            output_float(NRFD);
            output_high(TE);
            output_high(DAV);
            output_high(EOI);
        }
        
        // Set GPIB lines for receiving if addressed to listen
        if (_deviceListen)
        {
            output_float(DIO1);
            output_float(DIO2);
            output_float(DIO3);
            output_float(DIO4);
            output_float(DIO5);
            output_float(DIO6);
            output_float(DIO7);
            output_float(DIO8);
            
            output_float(DAV);
            output_float(EOI);
            output_low(TE);
            output_low(NDAC);
            output_high(NRFD);  // Indicate ready for data
        }
        
        // Send status byte if addressed to talk and serial poll mode enabled
        if (_deviceTalk && _deviceSerialPoll)
        {
            // Send status byte
            gpib_send_data(&_deviceStatusByte, 1, false);
            
            // Zero status byte and deassert SRQ
            _deviceStatusByte = 0x00;
            output_high(SRQ);
            
            // Disable serial poll mode so we only send at most
            // one byte per serial poll enable command received.
            _deviceSerialPoll = false;
        }
        
        // Read data if addressed to listen and data is available (DAV asserted)
        if (_deviceListen && !input(DAV))
            gpib_receive_data(READ_TO_EOI, NULL);
    }
}


void handle_listen_only_mode()
{
    // This function handles listen only mode where all traffic on the GPIB
    // bus is read regardless of the currently specified address.
    // Note: No data can be sent in listen only mode.
    
    
    _deviceTalk = false;
    _deviceListen = false;
    _deviceSerialPoll = false;
    
    // Set GPIB lines for receiving
    output_float(DIO1);
    output_float(DIO2);
    output_float(DIO3);
    output_float(DIO4);
    output_float(DIO5);
    output_float(DIO6);
    output_float(DIO7);
    output_float(DIO8);
    
    output_float(DAV);
    output_float(EOI);
    output_low(TE);
    output_low(NDAC);
    output_high(NRFD);  // Indicate ready for data

    
    // If ATN is asserted we must wait for a command from the controller
    if (!input(ATN))
    {
        // Do nothing if ATN is asserted, but DAV is deasserted (waiting for command)
        if (input(DAV))
            return;
            
        // Read command byte (Do nothing if read fails)
        uint8_t cmdByte = 0x00;
        uint8_t eoiStatus;
        if (gpib_receive_byte(&cmdByte, &eoiStatus))
            return;
            
        switch (cmdByte)
        {
            case GPIB_CMD_GTL:
                eot_printf("GPIB_CMD_GTL (0x%x)", cmdByte);
                break;
                
            case GPIB_CMD_SDC:
                eot_printf("GPIB_CMD_SDC (0x%x)", cmdByte);
                break;
                
            case GPIB_CMD_GET:
                eot_printf("GPIB_CMD_GET (0x%x)", cmdByte);
                break;
                
            case GPIB_CMD_LLO:
                eot_printf("GPIB_CMD_LLO (0x%x)", cmdByte);
                break;
                
            case GPIB_CMD_DCL:
                eot_printf("GPIB_CMD_DCL (0x%x)", cmdByte);
                break;
                
            case GPIB_CMD_SPE:
                eot_printf("GPIB_CMD_SPE (0x%x)", cmdByte);
                break;
                
            case GPIB_CMD_SPD:
                eot_printf("GPIB_CMD_SPD (0x%x)", cmdByte);
                break;
                
            case GPIB_CMD_UNL:
                eot_printf("GPIB_CMD_UNL (0x%x)", cmdByte);
                break;
                
            case GPIB_CMD_UNT:
                eot_printf("GPIB_CMD_UNT (0x%x)", cmdByte);
                break;
                
            default:
                eot_printf("GPIB_COMMAND (0x%x)", cmdByte);
                break;
        }
    }
    
    // If ATN is deasserted, we can resume normal operation
    else
    {
        // Read data if data is available (DAV asserted)
        // Note: In listen only mode, all data is read regardless of currently
        //       addressed listeners.
        if (!input(DAV))
            gpib_receive_data(READ_TO_EOI, NULL);
    }
}


#inline
void update_eeprom(int8_t address, int8_t value)
{
    // This function performs an EEPROM update where the byte is read first and
    // writing is skipped if the current value is the same as the new value.
    // This should prolong the EEPROM life by preventing excessive writes.
    
    // Only write to EEPROM if the value will change
    if (read_eeprom(address) != value)
    {
        write_eeprom(address, value);
        
#ifdef VERBOSE_DEBUG
        eot_printf("EEPROM Write: Address = 0x%x, Value = %u (0x%x)", address, value, value);
#endif
    }
}


void eeprom_read_cfg()
{
    // This function reads all configuration values from EEPROM
    
    
    // Only read EEPROM configuration values if version code is valid
    if (read_eeprom(0x00) != EEPROM_VERSION_CODE)
    {
        eeprom_write_cfg();
        return;
    }
    
#ifdef VERBOSE_DEBUG
    eot_printf("Reading EEPROM...");
#endif

    _gpibMode =     read_eeprom(0x01);
    _devicePad =    read_eeprom(0x02);
    _deviceSad =    read_eeprom(0x03);
    _useDeviceSad = read_eeprom(0x04);
    _autoRead =     read_eeprom(0x05);
    _useEoi =       read_eeprom(0x06);
    _eosMode =      read_eeprom(0x07);
    _eotEnable =    read_eeprom(0x08);
    _eotChar =      read_eeprom(0x09);
    _gpibTimeout =  make16(read_eeprom(0x0b), read_eeprom(0x0a));
}


void eeprom_write_cfg()
{
    // This function writes all configuration values to EEPROM
    
    
#ifdef VERBOSE_DEBUG
    eot_printf("Writing EEPROM...");
#endif

    update_eeprom(0x00, EEPROM_VERSION_CODE);
    update_eeprom(0x01, _gpibMode);
    update_eeprom(0x02, _devicePad);
    update_eeprom(0x03, _deviceSad);
    update_eeprom(0x04, _useDeviceSad);
    update_eeprom(0x05, _autoRead);
    update_eeprom(0x06, _useEoi);
    update_eeprom(0x07, _eosMode);
    update_eeprom(0x08, _eotEnable);
    update_eeprom(0x09, _eotChar);
    update_eeprom(0x0a, make8(_gpibTimeout, 0));
    update_eeprom(0x0b, make8(_gpibTimeout, 1));
}


void gpib_init_pins(uint8_t mode)
{
    // This function initializes the microcontroller pins for the given mode.
    //
    // Parameters:
    //   [in] mode: GPIB mode (e.g. Controller or Device)


    if (mode == MODE_CONTROLLER)
    {
        output_low(TE);   // Disable talking on data and handshake lines
        output_high(PE);  // Enable pullups on data lines (GPIB bus side)
        
        output_high(SC);  // Enable transmit on REN and IFC
        output_low(DC);   // Enable transmit on ATN and SRQ
        
        // Set all microcontroller data pins to inputs with pullups enabled
        output_float(DIO1);
        output_float(DIO2);
        output_float(DIO3);
        output_float(DIO4);
        output_float(DIO5);
        output_float(DIO6);
        output_float(DIO7);
        output_float(DIO8);
        
        output_high(ATN);   // Deassert the ATN
        output_float(SRQ);  // Set SRQ microcontroller pin to input with pullup enabled
        
        output_low(REN);    // Assert REN
        output_high(IFC);   // Deassert IFC
        
        output_high(EOI);   // Deassert EOI
        
        output_float(DAV);  // Set DAV microcontroller pin to input with pullup enabled
        output_low(NDAC);   // Assert NDAC
        output_low(NRFD);   // Assert NRFC
    }
    else  // Device mode
    {
        output_low(TE);   // Disable talking on data and handshake lines
        output_high(PE);  // Enable pullups on data lines (GPIB bus side)
        
        output_low(SC);   // Enable receive on REN and IFC
        output_high(DC);  // Enable receive on ATN and SRQ
        
        // Set all microcontroller data pins to inputs with pullups enabled
        output_float(DIO1);
        output_float(DIO2);
        output_float(DIO3);
        output_float(DIO4);
        output_float(DIO5);
        output_float(DIO6);
        output_float(DIO7);
        output_float(DIO8);
        
        output_float(ATN);  // Set ATN microcontroller pin to input with pullup enabled
        output_high(SRQ);   // Deassert SRQ
        
        output_float(REN);  // Set REN microcontroller pin to input with pullup enabled
        output_float(IFC);  // Set IFC microcontroller pin to input with pullup enabled
        
        output_float(EOI);  // Set EOI microcontroller pin to input with pullup enabled
        
        output_float(DAV);  // Set DAV microcontroller pin to input with pullup enabled
        output_low(NDAC);   // Assert NDAC
        output_low(NRFD);   // Assert NRFC
    }
}


#inline
void gpib_send_ifc()
{
    // This function sends the IFC control sequence.
    // Note: This function can only be executed by the system controller.
    // The effect of the control sequence is to remove all talkers and
    // listeners, serial poll disable all devices, and return control to the
    // system controller (controller becomes controller-in-charge).
    //
    // References:
    //   IEEE 488.2-1992 - 16.2.8 SEND IFC
    
    
    // Do nothing if not in controller mode
    if (_gpibMode != MODE_CONTROLLER)
    {
        debug_printf("Error: Cannot send IFC sequence while not in controller mode.");
        return;
    }
    
    // Assert IFC line for 150 uSec   
    output_low(IFC);
    delay_us(150);
    output_high(IFC);
}


bool gpib_read_status_byte(uint8_t *statusByte, uint8_t pad, uint8_t sad, bool useSad)
{
    // This function provides a means of reading the status byte from a
    // specific device.
    //
    // Parameters:
    //   [out] statusByte: Status byte from device (if read successful)
    //   [in]  pad:        Primary address (PAD) of device [Valid Range = 1-30]
    //   [in]  sad:        Secondary address (SAD) of device [Valid Range = 0-30]
    //   [in]  useSad:     Use secondary address
    //
    // Return Value: False = success; True = error
    //
    // References:
    //   IEEE 488.2-1992 - 16.2.18 READ STATUS BYTE
    
    
    // Initialize status byte to zero in case of error
    *statusByte = 0x00;
    
    bool errorStatus = false;
    
    // Send unlisten message (UNL)
    errorStatus = errorStatus || gpib_send_command(GPIB_CMD_UNL);
    
    // Send controller's listen address
    errorStatus = errorStatus || gpib_send_command(CONTROLLER_ADDR + 0x20);
    
    // Send serial poll enable message (SPE)
    errorStatus = errorStatus || gpib_send_command(GPIB_CMD_SPE);
    
    // Send device talk address
    errorStatus = errorStatus || gpib_send_command(pad + 0x40);
    
    if (useSad)
        errorStatus = errorStatus || gpib_send_command(sad + 0x60);
    
    uint8_t eoiStatus;
    errorStatus = errorStatus || gpib_receive_byte(statusByte, &eoiStatus);
    
    // Send serial poll disable message (SPD)
    errorStatus = errorStatus || gpib_send_command(GPIB_CMD_SPD);
    
    // Send untalk message (UNT)
    errorStatus = errorStatus || gpib_send_command(GPIB_CMD_UNT);
    
    return errorStatus;
}


bool gpib_send_setup(uint8_t pad, uint8_t sad, bool useSad)
{
    // This function configures the GPIB bus so that data can be transferred
    // from the controller to a device.
    //
    // Parameters:
    //   [in] pad:    Primary address (PAD) of device [Valid Range = 1-30]
    //   [in] sad:    Secondary address (SAD) of device [Valid Range = 0-30]
    //   [in] useSad: Use secondary address
    //
    // Return Value: False = success; True = error
    //
    // References:
    //   IEEE 488.2-1992 - 16.2.2 SEND SETUP
    
    
    // Verify PAD is in range of 1-30
    if (pad < 1 || pad > 30)
    {
        debug_printf("Error: Device address out of range (PAD = %u).", pad);
        return true;
    }
        
    // Verify SAD is in range of 0-30 if used
    if (useSad && sad > 30)
    {
        debug_printf("Error: Device address out of range (SAD = %u).", sad);
        return true;
    }
    
#ifdef VERBOSE_DEBUG
    if (useSad)
        eot_printf("GPIB Setup Send: PAD = %u, SAD = %u", pad, sad + 0x60);
    else    
        eot_printf("GPIB Setup Send: PAD = %u", pad);
#endif    
    
    bool errorStatus = false;
    
    // Send controller's talk address
    errorStatus = errorStatus || gpib_send_command(CONTROLLER_ADDR + 0x40);
    
    // Send unlisten message (UNL)
    errorStatus = errorStatus || gpib_send_command(GPIB_CMD_UNL);
    
    // Send device listen address
    errorStatus = errorStatus || gpib_send_command(pad + 0x20);
    
    if (useSad)
        errorStatus = errorStatus || gpib_send_command(sad + 0x60);
    
    return errorStatus;
}


#inline
bool gpib_send_command(uint8_t command)
{
    // This function is a wrapper for the gpib_send() function used to
    // send GPIB commands.
    //
    // Parameters:
    //   [in] command: GPIB command byte to send
    //
    // Return Value: False = success; True = error


    uint8_t buffer[1];
    buffer[0] = command;
    return gpib_send(buffer, 1, true, false);
}


#inline
bool gpib_send_data(uint8_t *buffer, uint8_t length, bool useEoi)
{
    // This function is a wrapper for the gpib_send() function used to
    // send device data. The selected string endings are automatically
    // sent with the data.
    //
    // Parameters:
    //   [in] buffer: Pointer to data to be sent
    //   [in] length: Number of bytes contained in buffer
    //   [in] useEoi: True = Assert EOI with last data byte
    //
    // Return Value: False = success; True = error


    bool errorStatus = false;
    
    // Send data with selected string ending appended
    switch (_eosMode)
    {
        case EOS_CR_LF:
            errorStatus = errorStatus || gpib_send(buffer, length, false, false);
            errorStatus = errorStatus || gpib_send(_eosBuffer, 2, false, useEoi);
            break;
            
        case EOS_CR:
            errorStatus = errorStatus || gpib_send(buffer, length, false, false);
            errorStatus = errorStatus || gpib_send(_eosBuffer, 1, false, useEoi);
            break;
            
        case EOS_LF:
            errorStatus = errorStatus || gpib_send(buffer, length, false, false);
            errorStatus = errorStatus || gpib_send(_eosBuffer+1, 1, false, useEoi);
            break;
            
        case EOS_NONE:
        default:
            errorStatus = errorStatus || gpib_send(buffer, length, false, useEoi);
            break;
    }
    
    return errorStatus;
}


bool gpib_send(uint8_t *buffer, uint8_t length, bool isCommand, bool useEoi)
{
    // This function sends a GPIB command or string of bytes to a device
    // on the GPIB bus.
    //
    // Parameters:
    //   [in] buffer:    Pointer to data to be sent
    //   [in] length:    Number of bytes to send
    //   [in] isCommand: True = GPIB command; False = device data
    //   [in] useEoi:    True = Assert EOI with last data byte (Note: Must be False for GPIB commands.)
    //
    // Return Value: False = success; True = error
    //
    // References:
    //   IEEE 488.1-1987 - Annex B Handshake Process Timing Sequence
    //   IEEE 488.2-1992 - 16.2.1 SEND COMMAND
    //   IEEE 488.2-1992 - 16.2.3 SEND DATA BYTES
    
    
    // Do nothing if there are no bytes to send
    if (length < 1)
        return false;
        
    // Do not allow commands unless in controller mode
    if (isCommand && _gpibMode != MODE_CONTROLLER)
    {
        debug_printf("Error: Trying to send GPIB command while not in controller mode.");
        return true;
    }
    
    // EOI must remain deasserted (useEoi = false) for GPIB commands
    if (isCommand)
        useEoi = false;
    
    // Set NDAC and NRFD lines to inputs with pullups enabled
    output_float(NDAC);
    output_float(NRFD);
    
    // Only control ATN when in controller mode
    if (_gpibMode == MODE_CONTROLLER)
    {
        // Assert ATN if sending a command, otherwise deassert ATN line
        if (isCommand)
            output_low(ATN);
        else
            output_high(ATN);
    }
    
    // Enable talking on GPIB bus
    output_high(TE);
    
    // Set handshake lines to begin data transfer process
    output_high(DAV);
    output_high(EOI);

    // Loop through each byte in the buffer
    for (uint8_t i = 0; i < length; i++)
    {
        restart_wdt();
        
#ifdef VERBOSE_DEBUG
        eot_printf("GPIB Send Byte: '%c' (0x%x)", buffer[i], buffer[i]);
#endif

        // Check for error condition where NRFD and NDAC are both high
        if (input(NRFD) && input(NDAC))
        {
            debug_printf("Error: NRFD and NDAC lines both high.");
            return true;
        }
        
        // Put byte on data lines
        // Note: Data lines are active low.
        output_b(buffer[i] ^ 0xff);
        
        // Wait for listeners to be ready for data (NRFD high)
        _mSecTimer = 0;
        enable_interrupts(INT_TIMER2);
        while (!input(NRFD))
        {
            restart_wdt();
            
            if(_mSecTimer >= _gpibTimeout)
            {
                disable_interrupts(INT_TIMER2);
                debug_printf("Timeout: Waiting for NRFD to go high during send.");
                return true;
            }
        }
        disable_interrupts(INT_TIMER2);
        
        // Assert EOI if required and this is the last byte in the buffer
        if (useEoi && (i == (length - 1)))
            output_low(EOI);
        
        // Inform listeners that the data is ready to be read
        output_low(DAV);
        
        // Wait for listeners to indicate they have read the data (NDAC high)
        _mSecTimer = 0;
        enable_interrupts(INT_TIMER2);
        while (!input(NDAC))
        {
            restart_wdt();
            
            if(_mSecTimer >= _gpibTimeout)
            {
                disable_interrupts(INT_TIMER2);
                output_high(DAV);
                debug_printf("Timeout: Waiting for NDAC to go high during send.");
                return true;
            }
        }
        disable_interrupts(INT_TIMER2);

        // Indicate data is no longer valid
        output_high(DAV);
    }
    
    return false;
}


bool gpib_receive_setup(uint8_t pad, uint8_t sad, bool useSad)
{
    // This function configures the GPIB bus so that data can be transferred
    // from a device to the controller.
    //
    // Parameters:
    //   [in] pad:    Primary address (PAD) of device [Valid Range = 1-30]
    //   [in] sad:    Secondary address (SAD) of device [Valid Range = 0-30]
    //   [in] useSad: Use secondary address
    //
    // Return Value: False = success; True = error
    //
    // References:
    //   IEEE 488.2-1992 - 16.2.5 RECEIVE SETUP
    
    
    // Verify PAD is in range of 1-30
    if (pad < 1 || pad > 30)
    {
        debug_printf("Error: Device address out of range (PAD = %u).", pad);
        return true;
    }
        
    // Verify SAD is in range of 0-30 if used
    if (useSad && sad > 30)
    {
        debug_printf("Error: Device address out of range (SAD = %u).", sad);
        return true;
    }
    
#ifdef VERBOSE_DEBUG
    if (useSad)
        eot_printf("GPIB Setup Receive: PAD = %u, SAD = %u", pad, sad + 0x60);
    else    
        eot_printf("GPIB Setup Receive: PAD = %u", pad);
#endif       
    
    bool errorStatus = 0;
    
    // Send unlisten message (UNL)
    errorStatus = errorStatus || gpib_send_command(GPIB_CMD_UNL);
    
    // Send controller's listen address
    errorStatus = errorStatus || gpib_send_command(CONTROLLER_ADDR + 0x20);
    
    // Send device talk address
    errorStatus = errorStatus || gpib_send_command(pad + 0x40);
    
    if (useSad)
        errorStatus = errorStatus || gpib_send_command(sad + 0x60);
    
    return errorStatus;
}


bool gpib_receive_byte(char *buffer, uint8_t *eoiStatus)
{
    // This function receives a single byte from a device on the GPIB bus.
    //
    // Parameters:
    //   [out] buffer:    Pointer to buffer where byte will be returned
    //   [out] eoiStatus: 1 = EOI was asserted with byte; 0 = EOI was deasserted with byte
    //
    // Return Value: False = success; True = timeout on receive
    //
    // References:
    //   IEEE 488.1-1987 - Annex B Handshake Process Timing Sequence
    //   IEEE 488.2-1992 - 16.2.6 RECEIVE RESPONSE MESSAGE
    
    
    // Initialize EOI status in case of error
    *eoiStatus = 0;
    
    // Set all data lines to inputs with pullups enabled
    output_float(DIO1);
    output_float(DIO2);
    output_float(DIO3);
    output_float(DIO4);
    output_float(DIO5);
    output_float(DIO6);
    output_float(DIO7);
    output_float(DIO8);
    
    // Set DAV and EOI lines to inputs with pullups enabled
    output_float(DAV);
    output_float(EOI);

    // Deassert ATN line (Only control ATN when in controller mode)
    if (_gpibMode == MODE_CONTROLLER)
        output_high(ATN);
    
    // Disable talking on the GPIB bus (enable talking)
    output_low(TE);
    
    // Indicate that we are ready to accept data
    output_low(NDAC);
    output_high(NRFD);
    
    // Wait for data to become valid (DAV low)
    _mSecTimer = 0;
    enable_interrupts(INT_TIMER2);
    while (input(DAV))
    {
        restart_wdt();
        
        if(_mSecTimer >= _gpibTimeout)
        {
            disable_interrupts(INT_TIMER2);
            output_low(NRFD);
            debug_printf("Timeout: Waiting for DAV to go low during receive.");
            return true;
        }
    }
    disable_interrupts(INT_TIMER2);

    // Assert NRFD to indicate data is being read
    output_low(NRFD);
    
    // Read data lines and EOI
    // Note: Data lines and EOI are active low.
    *buffer = input_b() ^ 0xff;
    *eoiStatus = !input(EOI);

#ifdef VERBOSE_DEBUG
    eot_printf("GPIB Receive Byte: %c (0x%x) [EOI = %u]", *buffer, *buffer, *eoiStatus);
#endif

    // Deassert NDAC to indicate data has been accepted
    output_high(NDAC);
    
    // Wait for DAV to go high
    _mSecTimer = 0;
    enable_interrupts(INT_TIMER2);
    while (!input(DAV))
    {
        restart_wdt();
        
        if(_mSecTimer >= _gpibTimeout)
        {
            disable_interrupts(INT_TIMER2);
            output_low(NDAC);
            debug_printf("Timeout: Waiting for DAV to go high during receive.");
            return true;
        }
    }
    disable_interrupts(INT_TIMER2);

    // Assert NDAC
    output_low(NDAC);
    
    return false;
}


void gpib_receive_data(uint8_t readMode, char readToChar)
{
    // This function receives a response message from a device on the GPIB bus.
    //
    // Parameters:
    //   [in] readMode:   Read mode to use (e.g. To Timeout, To EOI, To Character)
    //   [in] readToChar: Character to read to when in read-to-character mode
    //
    // References:
    //   IEEE 488.2-1992 - 16.2.6 RECEIVE RESPONSE MESSAGE


#ifdef VERBOSE_DEBUG
    eot_printf("GPIB Read Start...");
#endif

    char c;
    uint8_t eoiStatus;
    bool recvTimeout;
    
    // Loop while reading data
    for (;;)
    {
        restart_wdt();
        
        // Read byte from GPIB device
        recvTimeout = gpib_receive_byte(&c, &eoiStatus);
        
        // Stop reading on timeout
        if (recvTimeout)
            break;
            
        // Output character that was read    
        putc(c);
        
        // Output end-of-transmission (EOT) character if enabled and EOI detected
        if (_eotEnable && eoiStatus == 1)
            putc(_eotChar);
            
        // Stop reading at EOI in read to EOI mode
        if (readMode == READ_TO_EOI && eoiStatus == 1)
            break;
        
        // Stop reading at specified character in read to character mode
        if (readMode == READ_TO_CHAR && c == readToChar)
            break;
    }

#ifdef VERBOSE_DEBUG
    eot_printf("GPIB Read End...");
#endif
}

