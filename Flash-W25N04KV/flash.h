/*
 * flash.h
 *
 *  Created on: Dec 9, 2024
 *      Author: niyaz
 */

#ifndef FLASH_H_
#define FLASH_H_

#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_uart.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#ifndef FLASH_CONSTANTS_H
#define FLASH_CONSTANTS_H

// Instruction Set Constants
static const uint8_t RESET_DEVICE = 0xFF;
static const uint8_t GET_JEDEC = 0x9F;
static const uint8_t READ_REGISTER = 0x0F;
static const uint8_t WRITE_REGISTER = 0x01;
static const uint8_t READ_PAGE = 0x13;
static const uint8_t READ_BUFFER = 0x03;
static const uint8_t WRITE_ENABLE = 0x06;
static const uint8_t WRITE_BUFFER = 0x84;
static const uint8_t WRITE_BUFFER_WITH_RESET = 0x02;
static const uint8_t WRITE_EXECUTE = 0x10;
static const uint8_t ERASE_BLOCK = 0xD8;

// Addresses of 3 status registers
static const uint8_t REGISTER_ONE = 0xA0;
static const uint8_t REGISTER_TWO = 0xB0;
static const uint8_t REGISTER_THREE = 0xC0;
static const uint8_t *REGISTERS[] = {
    &REGISTER_ONE,
    &REGISTER_TWO,
    &REGISTER_THREE};

// Timeout to use for all communications (in ms)
#define COM_TIMEOUT 100

#endif /* FLASH_CONSTANTS_H */

// Track position of packets on Flash
typedef struct
{
    uint32_t head; // Byte address of buffer head
    uint32_t tail; // Byte address of buffer tail
} circularBuffer;

// Packet of data
typedef struct
{
    uint8_t dummy;   // Dummy byte signifying start of page, non-FF
    uint8_t pl[337]; // Payload of the packet (useful data)
} pkt;

// Page of packets
typedef struct
{
    pkt packetArray[6];  // Array of all 6 packets within the page
    uint8_t padding[20]; // Padding at end of each page
} pageRead;              // Structure of bytes read from an entire page

union pageStructure
{
    pageRead page;                   // Contains structured page data
    uint8_t bytes[sizeof(pageRead)]; // Contains raw page data (for portability)
};

// SPI, UART, and queue handling types, must be defined in main.c
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart3;
extern osMessageQueueId_t uartQueueHandle;

// Register management functions
uint8_t FLASH_ReadRegister(int registerNo);

// Read functions
void FLASH_ReadJEDECID(void);
void FLASH_ReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);
void FLASH_ReadPage(uint32_t pageAddress);

// Write Functions
void FLASH_WriteEnable(void);
void FLASH_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress);
void FLASH_WriteExecute(uint32_t pageAddress);

// Erase Functions
void FLASH_EraseBuffer(void);
void FLASH_EraseBlock(uint16_t blockAddress);
void FLASH_ResetDeviceSoftware(void);
void FLASH_EraseDevice(void);

// Testing Functions
void FLASH_ListenCommands(void);
void FLASH_RunCommand(char *cmdStr);

#endif /* FLASH_H_ */
