/*
 * flash.h
 *
 *  Created on: Dec 9, 2024
 *      Author: niyaz
 */

#ifndef FLASH_H_
#define FLASH_H_

#include "cmsis_os.h"
#include "stm32f7xx_hal.h"
#include "testing/cli.h"

// From C's standard lib
#ifndef FLASH_STDLIB_
#define FLASH_STDLIB_
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#endif

// Constants
#define COM_TIMEOUT 100   /* Timeout to use for all communications (in ms) */
#define MAX_CMD_LENGTH 64 /* Maximum command length for CLI, arbitrarily chosen */

// Instruction Set
typedef enum
{
    GET_JEDEC = 0x9F,               /* Fetches 3 byte JEDEC ID of flash */
    READ_REGISTER = 0x0F,           /* Reads one of the 3 registers */
    WRITE_REGISTER = 0x01,          /* Writes to one of the 3 registers */
    READ_PAGE = 0x13,               /* Reads a page into the data buffer */
    READ_BUFFER = 0x03,             /* Reads from the data buffer */
    FAST_READ_BUFFER = 0x0B,        /* Reads from the data buffer */
    FAST_DUAL_READ_BUFFER = 0x3B,   /* Reads from the data buffer on 2 lines */
    FAST_DUAL_READ_IO = 0xBB,       /* Transmits address and reads from data buffer on 2 lines */
    FAST_QUAD_READ_BUFFER = 0x6B,   /* Reads from the data buffer on 4 lines */
    FAST_QUAD_READ_IO = 0xEB,       /* Transmits address and reads from the data buffer on 4 lines */
    WRITE_ENABLE = 0x06,            /* Sets the write enable latch to 1 */
    WRITE_DISABLE = 0x04,           /* Sets the write enable latch to 0 */
    WRITE_BUFFER = 0x84,            /* Writes into the data buffer */
    QUAD_WRITE_BUFFER = 0x34,       /* Writes into the data buffer on 4 lines */
    WRITE_BUFFER_WITH_RESET = 0x02, /* Writes into the data buffer, clearing all bits not written to */
    WRITE_EXECUTE = 0x10,           /* Executes write of data buffer to a page */
    ERASE_BLOCK = 0xD8,             /* Erases a block */
    RESET_DEVICE = 0xFF             /* Resets device software */
} FlashOpCode;

// Register Addresses
typedef enum
{
    REGISTER_ONE = 0xA0,  /* Address of register 1 (protection) */
    REGISTER_TWO = 0xB0,  /* Address of register 2 (configuration) */
    REGISTER_THREE = 0xC0 /* Address of register 3 (status) */
} FlashRegisterAddress;

// List of all 3 register addresses
static const FlashRegisterAddress REGISTERS[] = {REGISTER_ONE, REGISTER_TWO, REGISTER_THREE};

// Data mode (Rx or Tx) for instructions
typedef enum
{
    TRANSMIT = 1,
    RECEIVE = 2
} FlashDataMode;

// Struct containing data of a flash instruction
typedef struct
{
    FlashOpCode opCode;        // Single byte operation code
    uint32_t address;          // Address to be sent
    uint16_t addressSize;      // Size of address in bytes (2-3)
    uint32_t addressLinesUsed; // Lines to use to transmit address
    uint8_t dummyClocks;       // Number of dummy clock cycles to send
    FlashDataMode dataMode;    // Mode to determine whether to trasmit or receive data
    uint8_t *dataBuf;          // Buffer to store data to trasmit/hold received data
    uint16_t dataSize;         // Size of data in bytes
    uint8_t dataLinesUsed;     // Number of QSPI lines used for transmit/receive of data
} FlashInstruction;

//! Structs to parse pages
// Track position of packets on Flash
typedef struct
{
    uint32_t head; // Byte address of buffer head
    uint32_t tail; // Byte address of buffer tail
} CircularBuffer;

// Packet of data
typedef struct
{
    uint8_t dummy;   // Dummy byte signifying start of page, non-FF
    uint8_t pl[337]; // Payload of the packet (useful data)
} Packet;

// Page of packets
typedef struct
{
    Packet packetArray[6]; // Array of all 6 packets within the page
    uint8_t padding[20];   // Padding at end of each page
} PageRead;                // Structure of bytes read from an entire page

union PageStructure {
    PageRead page;                   // Contains structured page data
    uint8_t bytes[sizeof(PageRead)]; // Contains raw page data (for portability)
};

//! QSPI, UART, and queue handling types, must be defined in main.c
extern QSPI_HandleTypeDef hqspi;
extern UART_HandleTypeDef huart3;
extern osMessageQueueId_t uartQueueHandle; // Size of 64 bytes, where each item is 64 bytes
extern osMessageQueueId_t cmdParamQueueHandle; // Size of 8 bytes, where each item is 4 bytes (uint32_t), 2 params max

/// @brief Issues an instruction via the QSPI peropheral
/// @param instruction A struct containing the data of the instruction to be sent
/// @return An error code, 0 if successful and 1 if failed
int W25N04KV_QSPIInstruct(FlashInstruction *instruction);

/// @brief Reads the value of a specified register.
/// @param registerNo The register which is read (either 1, 2, or 3).
/// @return The value of the register. If the register fails to be read, UINT8_MAX is returned.
uint8_t W25N04KV_ReadRegister(int registerNo);

/// @brief Resets the flash memory device using a software reset command. Data stored should remain unaffected.
void W25N04KV_ResetDeviceSoftware(void);

/// @brief Fetches the value of the write enable latch (WEL) of the flash.
/// @return The value of the WEL bit, true if set and false if not.
bool W25N04KV_IsWEL(void);

/// @brief Fetches the value of the BUSY bit of the flash.
/// @return The value of the BUSY bit, true if set and false if not.
bool W25N04KV_IsBusy(void);

/// @brief Reads and prints the JEDEC ID of the flash via UART
void W25N04KV_ReadJEDECID(void);

/// @brief Reads data from the flash memory's data buffer, starting at the specified column address and storing the read
/// data.
/// @param columnAddress The starting column address.
/// @param size The number of bytes to read.
/// @param readResponse Pointer to the buffer to store the read data.
void W25N04KV_ReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);

/// @brief Reads data from the flash memory's data buffer. Functionally the same as W25N04KV_ReadBuffer for this flash,
/// but may give access to higher clock rates in other WinBond flash devices.
/// @param columnAddress The starting column address.
/// @param size The number of bytes to read.
/// @param readResponse Pointer to the buffer to store the read data.
void W25N04KV_FastReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);

/// @brief Reads data from the flash memory's data buffer using 2 lines (IO0 and IO1), but sends address on IO0 only.
/// @param columnAddress The starting column address.
/// @param size The number of bytes to read.
/// @param readResponse Pointer to the buffer to store the read data.
void W25N04KV_FastDualReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);

/// @brief Reads data from the flash memory's data buffer using 2 lines (IO0 and IO1), also sending the address on 2
/// lines.
/// @param columnAddress The starting column address.
/// @param size The number of bytes to read.
/// @param readResponse Pointer to the buffer to store the read data.
void W25N04KV_FastDualReadIO(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);

/// @brief Reads data from the flash memory's data buffer using 4 lines (IO0-IO3), but sends address on IO0 only.
/// @param columnAddress The starting column address.
/// @param size The number of bytes to read.
/// @param readResponse Pointer to the buffer to store the read data.
void W25N04KV_FastQuadReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);

/// @brief Reads data from the flash memory's data buffer using 4 lines (IO0-IO3), also sending the address on 4 lines.
/// @param columnAddress The starting column address.
/// @param size The number of bytes to read.
/// @param readResponse Pointer to the buffer to store the read data.
void W25N04KV_FastQuadReadIO(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);

/// @brief Reads an entire page of data from the specified page address into the data buffer.
/// @param pageAddress The address of the page to read, from 0 to 262143.
void W25N04KV_ReadPage(uint32_t pageAddress);

/// @brief Enables write operations for the flash memory, setting the Write Enable Latch (WEL) bit to 1.
void W25N04KV_WriteEnable(void);

/// @brief Disables write operations for the flash memory, setting the Write Enable Latch (WEL) bit to 0.
void W25N04KV_WriteDisable(void);

/// @brief Writes data into the data buffer at the specified column address. Data exceeding the buffer size is
/// discarded.
/// @param data Pointer to the buffer containing the data to write.
/// @param size The number of bytes to write.
/// @param columnAddress The starting column address.
void W25N04KV_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress);

/// @brief Writes data into the data buffer at the specified column address using 4 lines. Data exceeding the buffer
/// size is discarded.
/// @param data Pointer to the buffer containing the data to write.
/// @param size The number of bytes to write.
/// @param columnAddress The starting column address.
void W25N04KV_QuadWriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress);

/// @brief Commits the data written to the buffer to the specified page address.
/// @param pageAddress The address of the page to write to, between 0 and 262143.
void W25N04KV_WriteExecute(uint32_t pageAddress);

/// @brief Erases the data buffer, setting all bytes to 0xFF.
void W25N04KV_EraseBuffer(void);

/// @brief Erases a specific block in the flash memory.
/// @param blockAddress The address of the block to erase, between 0 and 4095.
void W25N04KV_EraseBlock(uint16_t blockAddress);

/// @brief Performs a full device erase, clearing all data in the main data array.
void W25N04KV_EraseDevice(void);

/// @brief Finds the head and tail positions in a circular buffer within the specified page range.
/// @param buf Pointer to the circular buffer struct.
/// @param pageRange Array of two uint8_t values representing the start and end of the page range to search for head and
/// tail.
void W25N04KV_FindHeadTail(CircularBuffer *buf, uint8_t pageRange[2]);

#endif /* FLASH_H_ */
