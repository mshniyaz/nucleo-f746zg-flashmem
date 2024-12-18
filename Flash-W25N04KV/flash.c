/*
 * flash.c
 *
 *  Created on: Dec 9, 2024
 *      Author: niyaz
 */

#include "flash.h"

//! General Operations

// Prints a string of arbitrary size via UART
void UART_Printf(const char *format, ...)
{
  va_list args;
  int UARTBufLen;
  char *UARTBuf;

  // Start variadic argument processing
  va_start(args, format);
  UARTBufLen = vsnprintf(NULL, 0, format, args) + 1; // Calc required buffer length (+1 for null terminator)

  // Check for error in `vsnprintf`
  if (UARTBufLen <= 0)
  {
    va_end(args);
    return;
  }

  UARTBuf = (char *)malloc(UARTBufLen);
  if (UARTBuf == NULL)
  {
    va_end(args);
    return; // Handle memory allocation failure
  }

  // Reset args and format the string into the allocated buffer (reset va_list first)
  va_end(args);
  va_start(args, format);
  vsnprintf(UARTBuf, UARTBufLen, format, args);
  va_end(args);

  // Transmit the string using UART
  HAL_UART_Transmit(&huart3, (uint8_t *)UARTBuf, UARTBufLen - 1, COM_TIMEOUT);
  free(UARTBuf); // Free allocated memory
}

// Drives Chip Select Low to issue a command
void FLASH_CS_Low(void)
{
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

  // Verify if the pin state is actually low
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) != GPIO_PIN_RESET)
  {
    UART_Printf("Error: Failed to flash chip select low\r\n");
  }
}

// Drives Chip Select High after issuing a command
void FLASH_CS_High(void)
{
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);

  // Verify if the pin state is actually low
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) != GPIO_PIN_SET)
  {
    UART_Printf("Error: Failed to flash chip select high\r\n");
  }
}

// Transmits 1 or more bytes of data from master to slave via SPI
void FLASH_Transmit(uint8_t *data, uint16_t size)
{
  if (HAL_SPI_Transmit(&hspi1, data, size, COM_TIMEOUT) != HAL_OK)
  {
    UART_Printf("SPI transmit failed\r\n");
    FLASH_CS_High();
    return;
  }
}

// Receive output from the slave (flash memory) via SPI
void FLASH_Receive(uint8_t *buf, uint16_t size)
{
  uint8_t response[size];
  if (HAL_SPI_Receive(&hspi1, response, size, COM_TIMEOUT) != HAL_OK)
  {
    UART_Printf("SPI receive failed \r\n");
    FLASH_CS_High();
    return;
  }

  // Copy received data to the output buffer
  memcpy(buf, response, size);
}

// Generates a cycle of 8 dummy clocks by transmitting 0x00
void FLASH_DummyClock(void)
{
  uint8_t dummy_byte = 0x00;
  FLASH_Transmit(&dummy_byte, 1);
}

// Convenience functions for splitting uint32_t (little endian) into its 3 last bits only (from last memory address to first memory address)
void uitn32GetLastThreeBits(uint32_t n, uint8_t *threeLSB)
{
  for (int i = 0; i <= 2; i++)
  {
    threeLSB[2 - i] = *(((uint8_t *)&n) + i); // Note uint32_t is little endian
  }
}

// Convert uint16_t to a big endian byte array
void uint16ToByteArray(uint16_t n, uint8_t *byteArray)
{
  byteArray[0] = (n >> 8) & 0xFF;
  byteArray[1] = n & 0xFF;
}

//! Managing Status Registers

// Reads registers (either 1,2, or 3)
uint8_t FLASH_ReadRegister(int registerNo)
{
  uint8_t registerResponse[1];

  // Carry out the read instruction
  FLASH_CS_Low();
  FLASH_Transmit(&READ_REGISTER, 1);
  if (registerNo >= 1 && registerNo <= 3)
  {
    FLASH_Transmit(REGISTERS[registerNo - 1], 1);
  }
  else
  {
    FLASH_Transmit(REGISTERS[2], 1); // Get status register by deafult
  }
  FLASH_Receive(registerResponse, 1);
  FLASH_CS_High();
  return registerResponse[0];
}

// Read Write Enable Latch (WEL) Bit
bool FLASH_IsWEL(void)
{
  uint8_t statusRegister = FLASH_ReadRegister(3);
  bool isWriteEnabled = (statusRegister & (1 << 1)) >> 1; // WEL is 2nd last bit
  return isWriteEnabled;
}

// Read BUSY Bit
bool FLASH_IsBusy(void)
{
  uint8_t statusRegister = FLASH_ReadRegister(3);
  bool isBusy = statusRegister & 1; // Busy bit is last bit
  return isBusy;
}

// Wait till BUSY bit is cleared to zero
void FLASH_AwaitNotBusy(void)
{
  while (FLASH_IsBusy())
  {
    HAL_Delay(10); // Short delays of 10ms
  }
}

// Disable write protection for all blocks and registers
void FLASH_DisableWriteProtect(void)
{
  uint8_t newRegValue = 0x00; // Set all bits to zero

  FLASH_CS_Low();
  FLASH_Transmit(&WRITE_REGISTER, 1);
  FLASH_Transmit(REGISTERS[0], 1);
  FLASH_Transmit(&newRegValue, 1);
  FLASH_CS_High();
}

//! Read Operations

// Read JEDEC ID of flash memory, including manufacturer and product ID
void FLASH_ReadJEDECID(void)
{
  uint8_t jedecResponse[3] = {0}; // Set to 0 by default, will be overridden later

  // Send JEDEC ID opcode
  FLASH_CS_Low();
  FLASH_Transmit(&GET_JEDEC, 1);
  FLASH_DummyClock();

  // Receive 3 bytes (JEDEC ID)
  FLASH_Receive(jedecResponse, 3);
  FLASH_CS_High();

  // Print JEDEC ID to UART
  UART_Printf("\r\n------------------------\r\n");
  UART_Printf("W25N04KV QspiNAND Memory\r\n");
  UART_Printf("JEDEC ID: 0x%02X 0x%02X 0x%02X",
              jedecResponse[0], jedecResponse[1], jedecResponse[2]);
  UART_Printf("\r\n------------------------\r\n");
}

// Transfers data in a page to the flash memory's data buffer
void FLASH_ReadPage(uint32_t pageAddress)
{
  uint8_t truncatedPageAddress[3];
  uitn32GetLastThreeBits(pageAddress, truncatedPageAddress);

  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&READ_PAGE, 1);
  // Shift in 3-byte page address (last 18 bits used, first 12 are block and last 6 are page address)
  FLASH_Transmit(truncatedPageAddress, 3);
  FLASH_CS_High();
}

// Reads data from the flash memory buffer into the provided buffer `readResponse`
void FLASH_ReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
{
  uint8_t columnAddressByteArray[2];
  uint16ToByteArray(columnAddress, columnAddressByteArray);

  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&READ_BUFFER, 1);
  // Shift in 2-byte column address (only last 12 bits used)
  FLASH_Transmit(columnAddressByteArray, 2);
  FLASH_DummyClock();
  FLASH_Receive(readResponse, size);
  FLASH_CS_High();
}

//! Write Operations

// Enable write operations to the flash memory
void FLASH_WriteEnable(void)
{
  FLASH_CS_Low();
  FLASH_Transmit(&WRITE_ENABLE, 1);
  FLASH_CS_High();
}

// Write to the flash memory's data buffer
void FLASH_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress)
{
  uint8_t readResponse[size];
  uint8_t columnAddressByteArray[2];
  uint16ToByteArray(columnAddress, columnAddressByteArray);

  FLASH_WriteEnable();
  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&WRITE_BUFFER, 1);
  FLASH_Transmit(columnAddressByteArray, 2); // Shift in 2-byte column address (only last 12 bits used)
  FLASH_Transmit(data, size);
  FLASH_CS_High();
}

// Write data in buffer to a page with a 3 byte address (Only up to end of page, extra data discarded)
void FLASH_WriteExecute(uint32_t pageAddress)
{
  uint8_t truncatedPageAddress[3];
  uitn32GetLastThreeBits(pageAddress, truncatedPageAddress);

  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&WRITE_EXECUTE, 1);
  // Shift in 3-byte page address (last 18 bits used, first 12 are block and last 6 are page address)
  FLASH_Transmit(truncatedPageAddress, 3);
  FLASH_CS_High();
}

//! Erase Operations

// Erase the entire data buffer
void FLASH_EraseBuffer(void) {
  uint8_t columnAddressByteArray[2] = {0, 0};

  FLASH_WriteEnable();
  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&WRITE_BUFFER_WITH_RESET, 1);
  FLASH_Transmit(columnAddressByteArray, 2); // Shift in 2-byte column address (only last 12 bits used)
  FLASH_CS_High();
}

// Erase the block which the page at the given address is located within
void FLASH_EraseBlock(uint32_t pageAddress)
{
  uint8_t truncatedPageAddress[3];
  uitn32GetLastThreeBits(pageAddress, truncatedPageAddress);

  FLASH_WriteEnable();
  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&ERASE_BLOCK, 1);
  // Shift in 3-byte page address (last 18 bits used, first 12 are block and last 6 are page address)
  FLASH_Transmit(truncatedPageAddress, 3);
  FLASH_CS_High();
}

// Resets device software and disables write protection
void FLASH_ResetDevice(void)
{
  FLASH_AwaitNotBusy();
  FLASH_CS_Low();
  FLASH_Transmit(&RESET_DEVICE, 1);
  FLASH_CS_High();

  FLASH_DisableWriteProtect();
}

// Resets entire memory array of flash to 0xFF, for testing only
void FLASH_EraseDevice(void)
{
  // There are 262144 (2^18 or 0x3FFFF+0x1) pages in eraseable blocks of 64
  for (int i = 0; i < 0x3FFFF; i += 64)
  {
    FLASH_EraseBlock(i);
    HAL_Delay(10); // Maxmimum possible erase time
  }

  // Erase buffer
  FLASH_EraseBuffer();
}