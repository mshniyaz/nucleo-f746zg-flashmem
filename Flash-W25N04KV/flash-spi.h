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
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Instruction Set Constants
#define GET_JEDEC 0x9F
#define READ_REGISTER 0x0F
#define WRITE_REGISTER 0x01
#define READ_PAGE 0x13
#define READ_BUFFER 0x03
#define FAST_READ_BUFFER 0x0B
#define FAST_DUAL_READ_BUFFER 0x3B
#define FAST_QUAD_READ_BUFFER 0x6B
#define WRITE_ENABLE 0x06
#define WRITE_DISABLE 0x04
#define WRITE_BUFFER 0x84
#define QUAD_WRITE_BUFFER 0x34
#define WRITE_BUFFER_WITH_RESET 0x02
#define WRITE_EXECUTE 0x10
#define ERASE_BLOCK 0xD8
#define RESET_DEVICE 0xFF

// TODO: Impelement below instructions
#define FAST_DUAL_READ_IO 0xBB
#define FAST_QUAD_READ_IO 0xEB

// Addresses of 3 status registers
#define REGISTER_ONE 0xA0
#define REGISTER_TWO 0xB0
#define REGISTER_THREE 0xC0
static const uint8_t REGISTERS[] = {REGISTER_ONE, REGISTER_TWO, REGISTER_THREE};

// Timeout to use for all communications (in ms)
#define COM_TIMEOUT 100
// Maximum command length for CLI
#define MAX_CMD_LENGTH 64
// Macros to use when selecting transmit or receive in instruction
#define TRANSMIT 1
#define RECEIVE 2

// Struct to issue instructions
typedef struct
{
    uint8_t opCode;            // Single byte operation code
    uint32_t address;          // Address to be sent
    uint16_t addressSize;      // Size of address in bytes (2-3)
    uint32_t addressLinesUsed; // Lines to use to transmit address
    uint8_t dummyClocks;       // Number of dummy clock cycles to send
    uint8_t dataMode;          // TRANSMIT or RECEIVE macros to determine whether to trasmit or receive data
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
extern osMessageQueueId_t uartQueueHandle;
extern osMessageQueueId_t cmdParamQueueHandle;

//! Memory management functions
// General QSPI Instructions
int FLASH_QSPIInstruct(FlashInstruction *instruction);

// Register management functions
uint8_t FLASH_ReadRegister(int registerNo);
void FLASH_ResetDeviceSoftware(void);
bool FLASH_IsWEL(void);
bool FLASH_IsBusy(void);

// Read functions
void FLASH_ReadJEDECID(void);
void FLASH_ReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);
void FLASH_FastReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);
void FLASH_FastDualReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);
void FLASH_FastDualReadIO(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);
void FLASH_FastQuadReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);
void FLASH_FastQuadReadIO(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);
void FLASH_ReadPage(uint32_t pageAddress);

// Write Functions
void FLASH_WriteEnable(void);
void FLASH_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress);
void FLASH_QuadWriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress);
void FLASH_WriteExecute(uint32_t pageAddress);

// Erase Functions
void FLASH_EraseBuffer(void);
void FLASH_EraseBlock(uint16_t blockAddress);
void FLASH_EraseDevice(void);

// Circular Buffer Functions
void FLASH_FindHeadTail(CircularBuffer *buf, uint8_t pageRange[2]);

// CLI Listening Functions
void FLASH_ListenCommands(void);
void FLASH_RunCommand(char *cmdStr);

// Testing functions (Not for external use)
void FLASH_ResetDeviceCmd(void);
void FLASH_TestRegistersCmd(void);
void FLASH_TestDataCmd(void);
void FLASH_TestHeadTailCmd(void);

#endif /* FLASH_H_ */
