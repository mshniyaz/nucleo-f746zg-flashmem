/*
 * flash-spi.c
 *
 * Contains code which implements QSPI instructions which
 * transmit or receive data on only the MISO or MOSI lines.
 * IO2 and IO3 remain unused.
 */

#include "W25N04KV.h"

//! General Operations

// Issues a command to the flash via QSPI
int W25N04KV_QSPIInstruct(FlashInstruction *instruction)
{
    QSPI_CommandTypeDef sCommand = {0};

    // Set instruction (opcode)
    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction = instruction->opCode;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // Alternate bytes (not used)
    // Set address mode and size
    if (instruction->addressSize == 0)
    {
        // No address given
        sCommand.AddressMode = QSPI_ADDRESS_NONE;
    }
    else
    {
        // Decide lines to use, 1 by default
        switch (instruction->addressLinesUsed)
        {
        case 2:
            sCommand.AddressMode = QSPI_ADDRESS_2_LINES;
            break;
        case 4:
            sCommand.AddressMode = QSPI_ADDRESS_4_LINES;
            break;
        default:
            sCommand.AddressMode = QSPI_ADDRESS_1_LINE;
            break;
        }
    }
    sCommand.Address = (instruction->addressSize > 0) ? instruction->address : QSPI_ADDRESS_NONE;
    // Convert address size in bytes to bits
    switch (instruction->addressSize)
    {
    case 1:
        sCommand.AddressSize = QSPI_ADDRESS_8_BITS;
        break;
    case 2:
        sCommand.AddressSize = QSPI_ADDRESS_16_BITS;
        break;
    case 3:
        sCommand.AddressSize = QSPI_ADDRESS_24_BITS;
        break;
    case 4:
        sCommand.AddressSize = QSPI_ADDRESS_32_BITS;
        break;
    default:
        sCommand.AddressSize = QSPI_ADDRESS_NONE;
        break;
    }
    // Dummy cycles for operations that require delays at high clock frequencies
    sCommand.DummyCycles = (instruction->dummyClocks != NULL) ? instruction->dummyClocks : 0;
    // Set data mode and size (data mode is 1 line by default unless dataSize is 0)
    if (instruction->dataSize == 0)
    {
        sCommand.DataMode = QSPI_DATA_NONE;
    }
    else
    {
        // Decide lines to use, 1 by default
        switch (instruction->dataLinesUsed)
        {
        case 2:
            sCommand.DataMode = QSPI_DATA_2_LINES;
            break;
        case 4:
            sCommand.DataMode = QSPI_DATA_4_LINES;
            break;
        default:
            sCommand.DataMode = QSPI_DATA_1_LINE;
            break;
        }
    }
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
uint8_t W25N04KV_ReadRegister(int registerNo)
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

    if (W25N04KV_QSPIInstruct(&readRegister) != 0)
    {
        printf("Error: Failed to read register %u\r\n", registerNo);
        return UINT8_MAX;
    }

    return registerResponse;
}

// Disable write protection for all blocks and registers
void W25N04KV_DisableWriteProtect(void)
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

    if (W25N04KV_QSPIInstruct(&disableWriteProtect) != 0)
    {
        printf("Error: Failed to disable write protection\r\n");
    }
}

// Read Write Enable Latch (WEL) Bit
bool W25N04KV_IsWEL(void)
{
    uint8_t statusRegister = W25N04KV_ReadRegister(3);
    return (statusRegister & (1 << 1)) >> 1; // WEL is 2nd last bit
}

// Read BUSY Bit
bool W25N04KV_IsBusy(void)
{
    uint8_t statusRegister = W25N04KV_ReadRegister(3);
    // Handle register read error
    if (statusRegister == UINT8_MAX)
    {
        return true; // Assume busy
    }

    return statusRegister & 1; // Busy bit is last bit
}

// Wait till BUSY bit is cleared to zero
void W25N04KV_AwaitNotBusy(void)
{
    // Repeatedly poll busy bit till success
    while (W25N04KV_IsBusy())
    {
        // TODO: Is constant polling like this ok? Is it blocking?
        // Delay till BUSY bit is 0
        continue;
    }

    return;
}

//! Read Operations

// Read JEDEC ID of flash memory
void W25N04KV_ReadJEDECID(void)
{
    uint8_t jedecResponse[3] = {0}; // Buffer to hold 3 byte ID (0xEFAA23)
    FlashInstruction readJEDEC = {
        .opCode = GET_JEDEC,
        .dummyClocks = 8,
        .dataMode = RECEIVE,
        .dataBuf = jedecResponse,
        .dataSize = 3,
    };

    if (W25N04KV_QSPIInstruct(&readJEDEC) != 0)
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
void W25N04KV_ReadPage(uint32_t pageAddress)
{
    FlashInstruction readPage = {
        .opCode = READ_PAGE,
        .address = pageAddress,
        .addressSize = 3,
    };

    W25N04KV_AwaitNotBusy();
    if (W25N04KV_QSPIInstruct(&readPage) != 0)
    {
        printf("Error: Failed to read page %u\r\n", pageAddress);
    }
}

// Reads data from the flash memory buffer into the provided buffer `readResponse`
void W25N04KV_ReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
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

    W25N04KV_AwaitNotBusy();
    if (W25N04KV_QSPIInstruct(&readBuffer) != 0)
    {
        printf("Error: Failed to read data buffer\r\n");
    }
}

// Read buffer for higher clock rates, functionally same as normal read for this flash
void W25N04KV_FastReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
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

    W25N04KV_AwaitNotBusy();
    if (W25N04KV_QSPIInstruct(&fastReadBuffer) != 0)
    {
        printf("Error: Failed to read data buffer\r\n");
    }
}

// Read buffer on 2 lines
void W25N04KV_FastDualReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
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

    W25N04KV_AwaitNotBusy();
    if (W25N04KV_QSPIInstruct(&fastDualReadBuffer) != 0)
    {
        printf("Error: Failed to read data buffer on 2 lines\r\n");
    }
}

// Read buffer on 2 lines, also send address on 2 lines
void W25N04KV_FastDualReadIO(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
{
    FlashInstruction fastDualReadIO = {
        .opCode = FAST_DUAL_READ_IO,
        .address = columnAddress,
        .addressSize = 2,
        .addressLinesUsed = 2,
        .dummyClocks = 4,
        .dataMode = RECEIVE,
        .dataBuf = readResponse,
        .dataSize = size,
        .dataLinesUsed = 2,
    };

    W25N04KV_AwaitNotBusy();
    if (W25N04KV_QSPIInstruct(&fastDualReadIO) != 0)
    {
        printf("Error: Failed to send address and read data buffer on 2 lines\r\n");
    }
}

//! Write Operations

// Enable write operations to the flash memory
void W25N04KV_WriteEnable(void)
{
    FlashInstruction writeEnable = {.opCode = WRITE_ENABLE};

    if (W25N04KV_QSPIInstruct(&writeEnable) != 0)
    {
        printf("Error: Failed to enable writes\r\n");
    }
}

// Disable write operations to the flash memory
void W25N04KV_WriteDisable(void)
{
    FlashInstruction writeDisable = {.opCode = WRITE_DISABLE};

    if (W25N04KV_QSPIInstruct(&writeDisable) != 0)
    {
        printf("Error: Failed to disable writes\r\n");
    }
}

// Write to the flash memory's data buffer
void W25N04KV_WriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress)
{
    FlashInstruction writeBuffer = {
        .opCode = WRITE_BUFFER,
        .address = columnAddress,
        .addressSize = 2,
        .dataMode = TRANSMIT,
        .dataBuf = data,
        .dataSize = size,
    };

    W25N04KV_AwaitNotBusy();
    W25N04KV_WriteEnable();
    if (W25N04KV_QSPIInstruct(&writeBuffer) != 0)
    {
        printf("Error: Failed to write to data buffer\r\n");
    }
}

// Write data in buffer to a page with a 3 byte address
void W25N04KV_WriteExecute(uint32_t pageAddress)
{
    FlashInstruction writeExecute = {
        .opCode = WRITE_EXECUTE,
        .address = pageAddress,
        .addressSize = 3,
    };

    W25N04KV_AwaitNotBusy();
    if (W25N04KV_QSPIInstruct(&writeExecute) != 0)
    {
        printf("Error: Failed to write buffer into flash\r\n");
    }
}

//! Erase Operations

// Erase the entire data buffer
void W25N04KV_EraseBuffer(void)
{
    FlashInstruction eraseBuffer = {
        .opCode = WRITE_BUFFER_WITH_RESET,
        .address = 0,
        .addressSize = 2,
    };

    W25N04KV_WriteEnable();
    W25N04KV_AwaitNotBusy();
    if (W25N04KV_QSPIInstruct(&eraseBuffer) != 0)
    {
        printf("Error: Failed to erase buffer\r\n");
    }
    W25N04KV_WriteDisable();
}

// Erase the block at the given block address (between 0 and 4095)
void W25N04KV_EraseBlock(uint16_t blockAddress)
{
    FlashInstruction eraseBlock = {
        .opCode = ERASE_BLOCK,
        .address = blockAddress * 64,
        .addressSize = 3,
    };

    W25N04KV_AwaitNotBusy();
    W25N04KV_WriteEnable();
    if (W25N04KV_QSPIInstruct(&eraseBlock) != 0)
    {
        printf("Error: Failed to erase block\r\n");
    }
}

// Resets device software and disables write protection
void W25N04KV_ResetDeviceSoftware(void)
{
    FlashInstruction resetSoftware = {.opCode = RESET_DEVICE};

    W25N04KV_AwaitNotBusy();
    if (W25N04KV_QSPIInstruct(&resetSoftware) != 0)
    {
        printf("Failed to reset software\r\n");
    }
    W25N04KV_DisableWriteProtect();
}

// Resets entire memory array of flash to 0xFF, and also reset software
void W25N04KV_EraseDevice(void)
{
    // There are 40 blocks
    for (int i = 0; i < 4096; i++)
    {
        W25N04KV_EraseBlock(i);
    }

    // Erase buffer and reset software
    W25N04KV_EraseBuffer();
    W25N04KV_ResetDeviceSoftware();
}

//! Circular Buffer Operations

// Finds the head and tail of the flash and stores it into a circular buffer
void W25N04KV_FindHeadTail(CircularBuffer *buf, uint8_t pageRange[2])
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
        W25N04KV_ReadPage(p);
        W25N04KV_ReadBuffer(0, 2048, pageBuf.bytes);

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
