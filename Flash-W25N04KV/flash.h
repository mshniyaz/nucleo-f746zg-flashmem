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
#include <stdbool.h>

// Instruction set constants
extern const uint8_t GET_JEDEC;
extern const uint8_t READ_STATUS_REGISTER;
extern const uint8_t WRITE_ENABLE;
extern const uint8_t BLOCK_ERASE;
extern const uint8_t BUFFER_WRITE;
extern const uint8_t EXECUTE_WRITE;
extern const uint32_t SPI_TIMEOUT;
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart3;

// Function declarations
void UART_Printf(const char *format, ...);
void FLASH_CS_Low(void);
void FLASH_CS_High(void);
void FLASH_Transmit(uint8_t *opcode, uint16_t size, uint32_t timeout);
void FLASH_Receive(uint8_t *buf, uint16_t size, uint32_t timeout);
void FLASH_ReadJEDECID(void);
void FLASH_ReadPage(uint8_t *pageAddress);
void FLASH_ReadBuffer(uint16_t columnAddress, uint16_t size);
uint8_t FLASH_ReadStatusRegister(int registerNo);
bool FLASH_IsBusy(void);
void FLASH_AwaitNotBusy(void);
bool FLASH_IsWEL(void);
void FLASH_WriteEnable(void);
void FLASH_WriteExecute(uint8_t *pageAddress);
void FLASH_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress);
void FLASH_ResetDevice(void);

#endif /* FLASH_H_ */
