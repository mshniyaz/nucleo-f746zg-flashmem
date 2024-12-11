/*
 * flash.c
 *
 *  Created on: Dec 9, 2024
 *      Author: niyaz
 */

#include "flash.h"

// Instruction Set Constants
const uint8_t RESET_DEVICE = 0xFF;
const uint8_t GET_JEDEC = 0x9F;
const uint8_t READ_STATUS_REGISTER = 0x0F;
const uint8_t READ_PAGE = 0x13;
const uint8_t READ_BUFFER = 0x03;
const uint8_t WRITE_ENABLE = 0x06;
const uint8_t WRITE_BUFFER = 0x84; //! Unused bits in data buffer not erased
const uint8_t WRITE_EXECUTE = 0x10;
const uint8_t ERASE_BLOCK = 0xD8;
// Addresses of 3 status registers
const uint8_t STATUS_REGISTER_ONE = 0xA0;
const uint8_t STATUS_REGISTER_TWO = 0xB0;
const uint8_t STATUS_REGISTER_THREE = 0xC0;
// Timeout to use for all SPI communications (in ms)
const uint32_t SPI_TIMEOUT = 100;

//! General Operations
// TODO: Extend error handling for transmit, receive, and malloc (for UART print)
// TODO: Readd busy checks once testing of read/write is complete

// Prints a string of arbitrary size via UART
void UART_Printf(const char *format, ...)
{
  va_list args;
  int UARTBufLen;
  char *UARTBuf;

  // Start variadic argument processing
  va_start(args, format);
  UARTBufLen = vsnprintf(NULL, 0, format, args) + 1; // Calc required buffer length (+1 for null terminator)
  UARTBuf = (char *)malloc(UARTBufLen);
  if (UARTBuf == NULL)
  {
    va_end(args);
    return;
  }

  // Transmit the string using UART
  vsnprintf(UARTBuf, UARTBufLen, format, args);
  HAL_UART_Transmit(&huart3, (uint8_t *)UARTBuf, UARTBufLen - 1, SPI_TIMEOUT); // -1 to exclude the null terminator
  free(UARTBuf);
  // End variadic argument processing
  va_end(args);
}

// Drives Chip Select Low to issue a command
void FLASH_CS_Low(void)
{
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
}

// Drives Chip Select High after issuing a command
void FLASH_CS_High(void)
{
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
}

// Transmits 1 or more bytes of data from master to slave via SPI
void FLASH_Transmit(uint8_t *data, uint16_t size, uint32_t timeout)
{
  if (HAL_SPI_Transmit(&hspi1, data, size, timeout) != HAL_OK)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
    return;
  }
}

// Generates a cycle of 8 dummy clocks by transmitting 0x00
void FLASH_DummyClock(void)
{
  uint8_t dummy_byte = 0x00;
  FLASH_Transmit(&dummy_byte, 1, SPI_TIMEOUT);
}

// Receive output from the slave (flash memory) via SPI
void FLASH_Receive(uint8_t *buf, uint16_t size, uint32_t timeout)
{
  uint8_t response[size];
  if (HAL_SPI_Receive(&hspi1, response, size, timeout) != HAL_OK)
  {
    FLASH_CS_High();
    return;
  }

  // Copy received data to the output buffer
  memcpy(buf, response, size);
}

//! Read Operations

// Read JEDEC ID of flash memory, including manufacturer and product ID
void FLASH_ReadJEDECID(void)
{
  uint8_t jedecResponse[3] = {0}; // Set to 0 by default, will be overridden later

  // Send JEDEC ID opcode
  FLASH_CS_Low();
  FLASH_Transmit(&GET_JEDEC, 1, SPI_TIMEOUT);
  FLASH_DummyClock();

  // Receive 3 bytes (JEDEC ID)
  FLASH_Receive(jedecResponse, 3, SPI_TIMEOUT);
  FLASH_CS_High();

  // Print JEDEC ID to UART
  UART_Printf("\r\nJEDEC ID: 0x%02X 0x%02X 0x%02X\r\n",
              jedecResponse[0], jedecResponse[1], jedecResponse[2]);
}

// Transfers data in a page to the flash memory's data buffer
void FLASH_ReadPage(uint8_t pageAddress[3])
{
  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&READ_PAGE, 1, SPI_TIMEOUT);
  // Shift in 3-byte page address (last 18 bits used, first 12 are block and last 6 are page address)
  FLASH_Transmit(pageAddress, 3, SPI_TIMEOUT);
  FLASH_CS_High();
}

// Reads data from the flash memory buffer into the provided buffer `readResponse`
void FLASH_ReadBuffer(uint16_t columnAddress, uint16_t size)
{
  uint8_t readResponse[size];

  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&READ_BUFFER, 1, SPI_TIMEOUT);
  // Shift in 2-byte column address (only last 12 bits used)
  FLASH_Transmit(&columnAddress, 2, SPI_TIMEOUT);
  FLASH_DummyClock();
  FLASH_Receive(readResponse, size, SPI_TIMEOUT);
  FLASH_CS_High();

  UART_Printf("\r\n");
  for (uint16_t i = 0; i < size; i++)
  {
    if (i == 0)
    {
      UART_Printf("Buffer Memory: %02x ", readResponse[i]);
    }
    else
    {
      UART_Printf("%02x ", readResponse[i]);
    }
  }
}

// Reads status registers (either 1,2, or 3)
uint8_t FLASH_ReadStatusRegister(int registerNo)
{
  uint8_t registerResponse[1];

  // Carry out the read instruction
  FLASH_CS_Low();
  FLASH_Transmit(&READ_STATUS_REGISTER, 1, SPI_TIMEOUT);
  if (registerNo == 1)
  {
    FLASH_Transmit(&STATUS_REGISTER_ONE, 1, SPI_TIMEOUT);
  }
  else if (registerNo == 2)
  {
    FLASH_Transmit(&STATUS_REGISTER_TWO, 1, SPI_TIMEOUT);
  }
  else
  {
    FLASH_Transmit(&STATUS_REGISTER_THREE, 1, SPI_TIMEOUT);
  }
  FLASH_Receive(registerResponse, 1, SPI_TIMEOUT);
  FLASH_CS_High();
  return registerResponse[0];
}

// Read Write Enable Latch (WEL) Bit
bool FLASH_IsWEL(void)
{
  uint8_t statusRegister = FLASH_ReadStatusRegister(3);
  bool isWriteEnabled = (statusRegister & (1 << 1)) >> 1; // WEL is 2nd last bit
  return isWriteEnabled;
}

// Read BUSY Bit
bool FLASH_IsBusy(void)
{
  uint8_t statusRegister = FLASH_ReadStatusRegister(3);
  bool isBusy = statusRegister & 1; // Busy bit is last bit
  return isBusy;
}

// Wait till BUSY bit is cleared to zero
void FLASH_AwaitNotBusy(void)
{
  while (FLASH_IsBusy())
  {
    HAL_Delay(1); // Short delays of 1ms
  }
}

//! Write Operations

// Enable write operations to the flash memory
void FLASH_WriteEnable(void)
{
  FLASH_CS_Low();
  FLASH_Transmit(&WRITE_ENABLE, 1, SPI_TIMEOUT);
  FLASH_CS_High();
}

// Write to the flash memory's data buffer
void FLASH_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress)
{
  FLASH_WriteEnable();
  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&WRITE_BUFFER, 1, SPI_TIMEOUT);
  FLASH_Transmit(&columnAddress, 2, SPI_TIMEOUT); // Shift in 2-byte column address (only last 12 bits used)
  FLASH_Transmit(data, size, SPI_TIMEOUT);
  FLASH_CS_High();
}

// Write data in buffer to a page with a 3 byte address (Only up to end of page, extra data discarded)
void FLASH_WriteExecute(uint8_t pageAddress[3])
{
  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&WRITE_EXECUTE, 1, SPI_TIMEOUT);
  // Shift in 3-byte page address (last 18 bits used, first 12 are block and last 6 are page address)
  FLASH_Transmit(pageAddress, 3, SPI_TIMEOUT);
  FLASH_CS_High();
}

//! Erase Operations

// Reset all pages in the flash memory to 0xFF
void FLASH_ResetDevice(void)
{
  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&RESET_DEVICE, 1, SPI_TIMEOUT);
  FLASH_CS_High();
}