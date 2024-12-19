/*
 * flash.h
 *
 *  Created on: Dec 9, 2024
 *      Author: niyaz
 */

#ifndef FLASH_H_
#define FLASH_H_

#include "stm32f7xx_hal.h"
#include <stdio.h>
#include <stdarg.h>
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

// Maximum buffer length for inputs
static const uint8_t MAX_INPUT_LEN = 64; // Must match cmd buffer size

// Timeout to use for all communications (in ms)
static const uint32_t COM_TIMEOUT = 100;
#endif /* FLASH_CONSTANTS_H */

// SPI and UART handling types, must be defined in main.c
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart3;

// General functions
void UART_ListenInput(char *resultBuffer, int *resultLen);

// Status Register management functions
uint8_t FLASH_ReadRegister(int registerNo);
bool FLASH_IsBusy(void);
bool FLASH_IsWEL(void);

// Read functions
void FLASH_ReadJEDECID(void);
void FLASH_ReadPage(uint32_t pageAddress);
void FLASH_ReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse);

// Write Functions
void FLASH_WriteEnable(void);
void FLASH_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress);
void FLASH_WriteExecute(uint32_t pageAddress);

// Erase Functions
void FLASH_EraseBuffer(void);
void FLASH_EraseBlock(uint32_t pageAddress);
void FLASH_ResetDeviceSoftware(void);
void FLASH_EraseDevice(void);

// Testing Functions
void FLASH_ListenCommands(void);

#endif /* FLASH_H_ */
