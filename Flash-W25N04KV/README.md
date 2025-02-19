# Flash-W25N04KV Library Documentation

The FLASH-W25N04KV library contains functions for operating the flash memory. Detailed documentation can be found in the FLASH-25N04KV folder. Include it in your code with the below:  

```c
#include <flash.h>
```

This library relies on some of its variables being defined externally, within the file it is included in. All of the below must be configured within the .ioc file of the project (locations in .ioc file menu shown as comments).

```c
extern SPI_HandleTypeDef hspi1; // Connectivity > SPI1
extern UART_HandleTypeDef huart3; // Connectivity > USART3

// Middleware > FreeRTOS > Tasks and Queues > Add Queue
extern osMessageQueueId_t uartQueueHandle; // uartQueue
extern osMessageQueueId_t cmdParamQueueHandle; // cmdParamQueue
```

All included functions fall into one of the below types:

1. Register Management
2. Read
3. Write
4. Erase
5. Circular Buffer Management
6. Testing

## Register Management Functions

```c
uint8_t FLASH_ReadRegister(int registerNo)
```

Reads the value of a specified register.  

**Params**:  

- `registerNo`: The register which is read (either 1, 2, or 3).  

**Returns**:  

- The value of the register.

## Read Functions

```c
void FLASH_ReadJEDECID(void)
```

Reads and prints the JEDEC ID of the flash memory device via UART.

```c
void FLASH_ReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
```

Reads data from the flash memory's data buffer, starting at the specified column address and storing the read data.

**Params**:  

- `columnAddress`: The starting column address.  
- `size`: The number of bytes to read.  
- `readResponse`: Pointer to the buffer to store the read data.  

```c
void FLASH_ReadPage(uint32_t pageAddress)
```

Reads an entire page of data from the specified page address into the data buffer.

**Params**:  

- `pageAddress`: The address of the page to read, from 0 to 262143.

## Write Functions

```c
void FLASH_WriteEnable(void)
```

Enables write operations for the flash memory, setting the Write Enable Latch (WEL) bit to 1.

```c
void FLASH_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress)
```

Writes data into the data buffer at the specified column address. Data exceeding the buffer size is discarded.  

**Params**:  

- `data`: Pointer to the buffer containing the data to write.  
- `size`: The number of bytes to write.  
- `columnAddress`: The starting column address.

```c
void FLASH_WriteExecute(uint32_t pageAddress)
```

Commits the data written to the buffer to the specified page address.  

**Params**:  

- `pageAddress`: The address of the page to write to, between 0 and 262143.  // Testing functions

## Erase Functions

```c
void FLASH_EraseBuffer(void)
```

Erases the data buffer, setting all bytes to `0xFF`.  

```c
void FLASH_EraseBlock(uint16_t blockAddress)
```

Erases a specific block in the flash memory.

**Params**:  

- `blockAddress`: The address of the block to erase, between 0 and 4095.

```c
void FLASH_ResetDeviceSoftware(void)
```

Resets the flash memory device using a software reset command. Data stored should remain unaffected.

```c
void FLASH_EraseDevice(void)
```

Performs a full device erase, clearing all data in the main data array. Takes approximately 40 seconds.

## Circular Buffer Functions

```c
void FLASH_FindHeadTail(circularBuffer *buf, uint8_t pageRange[2])
```

Finds the head and tail positions in a circular buffer within the specified page range.  

**Params**:  
- `buf`: Pointer to the circular buffer struct.
- `pageRange`: Array of two `uint8_t` values representing the start and end of the page range to search for head and tail.

## Testing Functions

**Note**: All the below functions must only be called within the `listenCommands` FreeRTOS task.

```c
void FLASH_ListenCommands(void)
```

Listens for and processes incoming commands for testing and debugging. Not meant to be called except within the FreeRTOS "ListenCommands" task.

```c
void FLASH_RunCommand(char *cmdStr)
```

Parses any string passed to it as a command. If the command is valid, it creates a new thread to run the command so the flash can continue accepting commands.  

**Params**:  
- `cmdStr`: Pointer to the command string to be parsed and executed.