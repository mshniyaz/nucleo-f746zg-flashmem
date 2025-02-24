#include "flash.h"

//! General Operations

// Issues a command to the flash via QSPI
int FLASH_QSPIInstruction(FlashInstruction *instruction)
{
    QSPI_CommandTypeDef sCommand = {0};

    // Set instruction (opcode)
    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = instruction->opCode;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // Alternate bytes (not used)
    // Set address mode and size (address mode is 1 line by default unless addressSize is 0)
    sCommand.AddressMode = (instruction->addressSize == NULL)     ? QSPI_ADDRESS_NONE
                           : (instruction->addressLinesUsed == 1) ? QSPI_ADDRESS_1_LINE
                           : (instruction->addressLinesUsed == 2) ? QSPI_ADDRESS_2_LINES
                           : (instruction->addressLinesUsed == 4) ? QSPI_ADDRESS_4_LINES
                                                                  : QSPI_ADDRESS_1_LINE;
    sCommand.Address = (instruction->addressSize > 0) ? instruction->address : QSPI_ADDRESS_NONE;
    sCommand.AddressSize = (instruction->addressSize == 1)   ? QSPI_ADDRESS_8_BITS
                           : (instruction->addressSize == 2) ? QSPI_ADDRESS_16_BITS
                           : (instruction->addressSize == 3) ? QSPI_ADDRESS_24_BITS
                           : (instruction->addressSize == 4) ? QSPI_ADDRESS_32_BITS
                                                             : QSPI_ADDRESS_NONE;
    // Dummy cycles for operations that require delays at high clock frequencies
    sCommand.DummyCycles = (instruction->dummyClocks != NULL) ? instruction->dummyClocks : 0;
    // Set data mode and size (data mode is 1 line by default unless dataSize is 0)
    sCommand.DataMode = (instruction->dataSize == NULL)     ? QSPI_DATA_NONE
                        : (instruction->dataLinesUsed == 1) ? QSPI_DATA_1_LINE
                        : (instruction->dataLinesUsed == 2) ? QSPI_DATA_2_LINES
                        : (instruction->dataLinesUsed == 4) ? QSPI_DATA_4_LINES
                                                            : QSPI_DATA_1_LINE;
    sCommand.NbData = instruction->dataSize;

    // Send command
    if (HAL_QSPI_Command(&hqspi, &sCommand, COM_TIMEOUT) != HAL_OK)
    {
        return 1; // Command failed
    }

    // Transmit only if data is provided
    if (instruction->dataBuf != NULL && instruction->dataSize > 0)
    {
        // Handle transmits
        if (instruction->dataMode == TRANSMIT)
        {
            if (HAL_QSPI_Transmit(&hqspi, instruction->dataBuf, COM_TIMEOUT) != HAL_OK)
            {
                return 1; // Transmission failed
            }
        }
        // Handle receives
        else if (instruction->dataMode == RECEIVE)
        {
            if (HAL_QSPI_Receive(&hqspi, instruction->dataBuf, COM_TIMEOUT) != HAL_OK)
            {
                return 1; // Reception failed
            }
        }
    }

    return 0; // Instruction successful
}

//! Managing Status Registers

// Reads registers (either 1,2, or 3)
uint8_t FLASH_ReadRegister(int registerNo)
{
    uint8_t registerResponse;
    FlashInstruction readRegister = {
        .opCode = READ_REGISTER,
        .address = REGISTERS[registerNo - 1],
        .addressSize = 1,
        .dataMode = RECEIVE,
        .dataBuf = &registerResponse,
        .dataSize = 1,
    };

    if (FLASH_QSPIInstruction(&readRegister) != 0)
    {
        printf("Error: Failed to read register %u\r\n", registerNo);
        return;
    }

    return registerResponse;
}

// Disable write protection for all blocks and registers
void FLASH_DisableWriteProtect(void)
{
    uint8_t registerVal = 0x00; // Set all bits of register 1 to 0
    FlashInstruction disableWriteProtect = {
        .opCode = WRITE_REGISTER,
        .address = REGISTER_ONE,
        .addressSize = 1,
        .dataMode = TRANSMIT,
        .dataBuf = &registerVal,
        .dataSize = 1,
    };

    if (FLASH_QSPIInstruction(&disableWriteProtect) != 0)
    {
        printf("Error: Failed to disable write protection\r\n");
    }
}

// Read Write Enable Latch (WEL) Bit
bool FLASH_IsWEL(void)
{
    uint8_t statusRegister = FLASH_ReadRegister(3);
    return (statusRegister & (1 << 1)) >> 1; // WEL is 2nd last bit
}

// Read BUSY Bit
bool FLASH_IsBusy(void)
{
    uint8_t statusRegister = FLASH_ReadRegister(3);
    return statusRegister & 1; // Busy bit is last bit
}

// Wait till BUSY bit is cleared to zero
void FLASH_AwaitNotBusy(void)
{
    if (FLASH_IsBusy())
    {
        osDelay(1); // TODO: Check if there is any better delay
        FLASH_AwaitNotBusy();
    }
}

//! Read Operations

// Read JEDEC ID of flash memory
void FLASH_ReadJEDECID(void)
{
    uint8_t jedecResponse[3] = {0}; // Buffer to hold 3 byte ID (0xEFAA23)
    FlashInstruction readJEDEC = {
        .opCode = GET_JEDEC,
        .address = NULL,
        .dummyClocks = 8,
        .dataMode = RECEIVE,
        .dataBuf = jedecResponse,
        .dataSize = 3,
    };

    if (FLASH_QSPIInstruction(&readJEDEC) != 0)
    {
        printf("Error: Failed to send JEDEC ID command\r\n");
        return;
    }

    // Print JEDEC ID to UART
    printf("\r\n------------------------\r\n");
    printf("W25N04KV QspiNAND Memory\r\n");
    printf("JEDEC ID: 0x%02X 0x%02X 0x%02X", jedecResponse[0], jedecResponse[1], jedecResponse[2]);
    printf("\r\n------------------------\r\n");
}

// Transfers data in a page to the flash memory's data buffer
void FLASH_ReadPage(uint32_t pageAddress)
{
    FlashInstruction readPage = {
        .opCode = READ_PAGE,
        .address = pageAddress,
        .addressSize = 3,
    };

    FLASH_AwaitNotBusy();
    if (FLASH_QSPIInstruction(&readPage) != 0)
    {
        printf("Error: Failed to read page %u\r\n", pageAddress);
    }
    osDelay(1); // TODO: Should be 25 microseconds
}

// Reads data from the flash memory buffer into the provided buffer `readResponse`
void FLASH_ReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
{
    FlashInstruction readBuffer = {
        .opCode = READ_BUFFER,
        .address = columnAddress,
        .addressSize = 2,
        .dummyClocks = 8,
        .dataMode = RECEIVE,
        .dataBuf = readResponse,
        .dataSize = size,
    };

    if (FLASH_QSPIInstruction(&readBuffer) != 0)
    {
        printf("Error: Failed to read data buffer\r\n");
    }
}

// TODO: Figure out what this command actually does
// Read buffer for higher clock rates
void FLASH_FastReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
{
    FlashInstruction fastReadBuffer = {
        .opCode = FAST_READ_BUFFER,
        .address = columnAddress,
        .addressSize = 2,
        .dummyClocks = 8,
        .dataMode = RECEIVE,
        .dataBuf = readResponse,
        .dataSize = size,
    };

    if (FLASH_QSPIInstruction(&fastReadBuffer) != 0)
    {
        printf("Error: Failed to read data buffer\r\n");
    }
}

// Read buffer on 2 lines
void FLASH_FastDualReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
{
    FlashInstruction fastDualReadBuffer = {
        .opCode = FAST_DUAL_READ_BUFFER,
        .address = columnAddress,
        .addressSize = 2,
        .dummyClocks = 8,
        .dataMode = RECEIVE,
        .dataBuf = readResponse,
        .dataSize = size,
        .dataLinesUsed = 2,
    };

    if (FLASH_QSPIInstruction(&fastDualReadBuffer) != 0)
    {
        printf("Error: Failed to read data buffer on 2 lines\r\n");
    }
}

// Read buffer on 4 lines
void FLASH_FastQuadReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
{
    FlashInstruction fastQuadReadBuffer = {
        .opCode = FAST_QUAD_READ_BUFFER,
        .address = columnAddress,
        .addressSize = 2,
        .dummyClocks = 8,
        .dataMode = RECEIVE,
        .dataBuf = readResponse,
        .dataSize = size,
        .dataLinesUsed = 4,
    };

    if (FLASH_QSPIInstruction(&fastQuadReadBuffer) != 0)
    {
        printf("Error: Failed to read data buffer on 4 lines\r\n");
    }
}

//! Write Operations

// Enable write operations to the flash memory
void FLASH_WriteEnable(void)
{
    FlashInstruction writeEnable = {.opCode = WRITE_ENABLE};

    if (FLASH_QSPIInstruction(&writeEnable) != 0)
    {
        printf("Error: Failed to enable writes\r\n");
    }
}

// Disable write operations to the flash memory
void FLASH_WriteDisable(void)
{
    FlashInstruction writeDisable = {.opCode = WRITE_DISABLE};

    if (FLASH_QSPIInstruction(&writeDisable) != 0)
    {
        printf("Error: Failed to disable writes\r\n");
    }
}

// Write to the flash memory's data buffer
void FLASH_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress)
{
    FlashInstruction writeBuffer = {
        .opCode = WRITE_BUFFER,
        .address = columnAddress,
        .addressSize = 2,
        .dataMode = TRANSMIT,
        .dataBuf = data,
        .dataSize = size,
    };

    FLASH_AwaitNotBusy();
    FLASH_WriteEnable();
    if (FLASH_QSPIInstruction(&writeBuffer) != 0)
    {
        printf("Error: Failed to write to data buffer\r\n");
    }
}

// Write to the flash memory's data buffer on 4 lines
void FLASH_QuadWriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress)
{
    FlashInstruction quadWriteBuffer = {
        .opCode = QUAD_WRITE_BUFFER,
        .address = columnAddress,
        .addressSize = 2,
        .dataMode = TRANSMIT,
        .dataBuf = data,
        .dataSize = size,
        .dataLinesUsed = 4,
    };

    FLASH_AwaitNotBusy();
    FLASH_WriteEnable();
    if (FLASH_QSPIInstruction(&quadWriteBuffer) != 0)
    {
        printf("Error: Failed to write to data buffer on 4 lines\r\n");
    }
}

// Write data in buffer to a page with a 3 byte address
void FLASH_WriteExecute(uint32_t pageAddress)
{
    FlashInstruction writeExecute = {
        .opCode = WRITE_EXECUTE,
        .address = pageAddress,
        .addressSize = 3,
    };

    FLASH_AwaitNotBusy();
    if (FLASH_QSPIInstruction(&writeExecute) != 0)
    {
        printf("Error: Failed to write buffer into flash\r\n");
    }
    osDelay(1); // TODO: Should be 700 microseconds
}

//! Erase Operations

// Erase the entire data buffer
void FLASH_EraseBuffer(void)
{
    FlashInstruction eraseBuffer = {
        .opCode = WRITE_BUFFER_WITH_RESET,
        .address = 0,
        .addressSize = 2,
    };

    FLASH_WriteEnable();
    FLASH_AwaitNotBusy();
    if (FLASH_QSPIInstruction(&eraseBuffer) != 0)
    {
        printf("Error: Failed to erase buffer\r\n");
    }
    FLASH_WriteDisable();
}

// Erase the block at the given block address (between 0 and 4095)
void FLASH_EraseBlock(uint16_t blockAddress)
{
    FlashInstruction eraseBlock = {
        .opCode = ERASE_BLOCK,
        .address = blockAddress * 64,
        .addressSize = 3,
    };

    FLASH_AwaitNotBusy();
    FLASH_WriteEnable();
    if (FLASH_QSPIInstruction(&eraseBlock) != 0)
    {
        printf("Error: Failed to erase block\r\n");
    }
    osDelay(10); // TODO: Is this delay really needed since we check BUSY bit?
}

// Resets device software and disables write protection
void FLASH_ResetDeviceSoftware(void)
{
    FlashInstruction resetSoftware = {.opCode = RESET_DEVICE};

    FLASH_AwaitNotBusy();
    if (FLASH_QSPIInstruction(&resetSoftware) != 0)
    {
        printf("Failed to reset software\r\n");
    }
    FLASH_DisableWriteProtect();
}

// Resets entire memory array of flash to 0xFF, and also reset software
void FLASH_EraseDevice(void)
{
    // There are 40 blocks
    for (int i = 0; i < 4096; i++)
    {
        FLASH_EraseBlock(i);
    }

    // Erase buffer and reset software
    FLASH_EraseBuffer();
    FLASH_ResetDeviceSoftware();
}

//! Circular Buffer Operations

// Finds the head and tail of the flash and stores it into a circular buffer
void FLASH_FindHeadTail(CircularBuffer *buf, uint8_t pageRange[2])
{
    // Use entire range if not specified
    if (pageRange == NULL)
    {
        pageRange[0] = 0;
        pageRange[1] = 262144;
    }

    bool headFound = false;      // Tracks whether head has been found
    union PageStructure pageBuf; // Buffer to store page data

    for (int p = pageRange[0]; p < pageRange[1]; p++)
    {
        FLASH_ReadPage(p);
        FLASH_ReadBuffer(0, 2048, pageBuf.bytes);

        // Check dummy byte of every packet
        for (int i = 0; i < 6; i++)
        {
            Packet packet = pageBuf.page.packetArray[i];
            if (packet.dummy != 0xFF)
            {
                if (!headFound)
                {
                    buf->head = p * 2048 + i * sizeof(Packet);
                    headFound = true;
                }
                buf->tail = p * 2048 + (i + 1) * sizeof(Packet);
            }
        }
    }
}
