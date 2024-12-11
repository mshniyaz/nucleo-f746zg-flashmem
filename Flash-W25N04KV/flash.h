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

// SPI and UART handling types, must be defined in main.c
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart3;

// General functions
void UART_Printf(const char *format, ...);

// Status Register management functions
uint8_t FLASH_ReadRegister(int registerNo);
bool FLASH_IsBusy(void);
bool FLASH_IsWEL(void);
void FLASH_DisableWriteProtect(void);

// Read functions
void FLASH_ReadJEDECID(void);
void FLASH_ReadPage(uint8_t *pageAddress);
void FLASH_ReadBuffer(uint16_t columnAddress, uint16_t size);

// Write Functions
void FLASH_WriteEnable(void);
void FLASH_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress);
void FLASH_WriteExecute(uint8_t *pageAddress);

// Erase Functions
void FLASH_ResetDevice(void);

#endif /* FLASH_H_ */
