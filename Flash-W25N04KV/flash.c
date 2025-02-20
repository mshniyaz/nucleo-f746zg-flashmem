#include "flash.h"

//! General Operations

// Issues a command to the flash via QSPI
int FLASH_QSPIInstruction(FlashInstruction *instruction)
{
    QSPI_CommandTypeDef sCommand = {0};

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE; // Set instruction (opcode)
    sCommand.Instruction = instruction->opCode;
    sCommand.AddressMode = (instruction->addressSize > 0 && instruction->address != NULL)
                               ? QSPI_ADDRESS_1_LINE
                               : QSPI_ADDRESS_NONE; // Set address mode and size
    sCommand.AddressSize = (instruction->addressSize == 1)   ? QSPI_ADDRESS_8_BITS
                           : (instruction->addressSize == 2) ? QSPI_ADDRESS_16_BITS
                           : (instruction->addressSize == 3) ? QSPI_ADDRESS_24_BITS
                                                             : QSPI_ADDRESS_NONE;
    sCommand.Address =
        (instruction->address != NULL) ? (uint32_t)instruction->address : QSPI_ADDRESS_NONE; // Add address field
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // Alternate bytes (not used)
    sCommand.DummyCycles = instruction->dummyClocks;        // Dummy cycles
    sCommand.DataMode = (instruction->linesUsed == 1)   ? QSPI_DATA_1_LINE
                        : (instruction->linesUsed == 2) ? QSPI_DATA_2_LINES
                        : (instruction->linesUsed == 4) ? QSPI_DATA_4_LINES
                                                        : QSPI_DATA_NONE; // Set data mode (1, 2, or 4 lines)
    sCommand.NbData = instruction->dataSize;

    // Send command
    if (HAL_QSPI_Command(&hqspi, &sCommand, COM_TIMEOUT) != HAL_OK)
    {
        return 1; // Command failed
    }

    // Transmit only if data is provided
    if (instruction->dataBuf != NULL && instruction->dataSize > 0)
    {
        if (instruction->dataMode == TRANSMIT && HAL_QSPI_Transmit(&hqspi, instruction->dataBuf, COM_TIMEOUT) != HAL_OK)
        {
            return 1; // Transmission failed
        }
        else if (instruction->dataMode == RECEIVE &&
                 HAL_QSPI_Receive(&hqspi, instruction->dataBuf, COM_TIMEOUT) != HAL_OK)
        {
            return 1; // Reception failed
        }
    }

    return 0; // Instruction successful
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
    if (FLASH_IsBusy())
    {
        osDelay(1); // TODO: Check if there is any better delay
        FLASH_AwaitNotBusy();
    }

    return;
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

// Read JEDEC ID of flash memory
void FLASH_ReadJEDECID(void)
{
    uint8_t jedecResponse[3] = {0}; // Buffer to hold 3 byte ID (0xEFAA23)
    FlashInstruction jedecInstruction = {.opCode = GET_JEDEC,
                                         .address = NULL,
                                         .dummyClocks = 8,
                                         .dataMode = RECEIVE,
                                         .dataBuf = jedecResponse,
                                         .dataSize = 3,
                                         .linesUsed = 1};

    if (FLASH_QSPIInstruction(&jedecInstruction) != 0)
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
    FLASH_AwaitNotBusy();
    FLASH_CS_Low();
    FLASH_Transmit(&READ_PAGE, 1);
    // Shift in 3-byte page address (last 18 bits used, others dummy)
    uint8_t pageAddressBytes[3] = {(pageAddress >> 16) & 0xFF, (pageAddress >> 8) & 0xFF,
                                   pageAddress & 0xFF}; // Unpack into 3 bytes, ignore endianness
    FLASH_Transmit(pageAddressBytes, 3);
    FLASH_CS_High();
    osDelay(1); // TODO: Should be 25 microseconds
}

// Reads data from the flash memory buffer into the provided buffer `readResponse`
void FLASH_ReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
{
    uint8_t columnAddressBytes[2] = {(columnAddress >> 8) & 0xFF,
                                     columnAddress & 0xFF}; // Unpack into 2 bytes, ignore endianness

    FLASH_AwaitNotBusy();
    FLASH_CS_Low();
    FLASH_Transmit(&READ_BUFFER, 1);
    // Shift in 2-byte column address (only last 12 bits used)
    FLASH_Transmit(columnAddressBytes, 2);
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
    uint8_t columnAddressBytes[2] = {(columnAddress >> 8) & 0xFF,
                                     (columnAddress & 0xFF)}; // Unpack into 2 bytes, ignore endianness

    FLASH_WriteEnable();
    FLASH_AwaitNotBusy();
    FLASH_CS_Low();
    FLASH_Transmit(&WRITE_BUFFER, 1);
    FLASH_Transmit(columnAddressBytes, 2); // Shift in 2-byte column address (only last 12 bits used)
    FLASH_Transmit(data, size);
    FLASH_CS_High();
}

// Write data in buffer to a page with a 3 byte address (Only up to end of page, extra data discarded)
void FLASH_WriteExecute(uint32_t pageAddress)
{
    FLASH_AwaitNotBusy();
    FLASH_CS_Low();
    FLASH_Transmit(&WRITE_EXECUTE, 1);
    // Shift in 3-byte page address (last 18 bits used, others dummy)
    uint8_t pageAddressBytes[3] = {(pageAddress >> 16) & 0xFF, (pageAddress >> 8) & 0xFF,
                                   pageAddress & 0xFF}; // Unpack into 3 bytes, ignore endianness
    FLASH_Transmit(pageAddressBytes, 3);
    FLASH_CS_High();
    osDelay(1); // TODO: Should be 700 microseconds
}

//! Erase Operations

// Erase the entire data buffer
void FLASH_EraseBuffer(void)
{
    uint16_t columnAddress = 0;

    FLASH_WriteEnable();
    FLASH_AwaitNotBusy();
    FLASH_CS_Low();
    FLASH_Transmit(&WRITE_BUFFER_WITH_RESET, 1);
    FLASH_Transmit(&columnAddress, 2); // Shift in 2-byte column address (only last 12 bits used)
    FLASH_CS_High();
}

// Erase the given block address without any delay, returning status register after
int FLASH_EraseWithoutDelay(uint16_t blockAddress)
{
    FLASH_WriteEnable();
    FLASH_CS_Low();
    FLASH_Transmit(&ERASE_BLOCK, 1);
    uint32_t pageAddress = blockAddress * 64;
    uint8_t pageAddressBytes[3] = {(pageAddress >> 16) & 0xFF, (pageAddress >> 8) & 0xFF,
                                   pageAddress & 0xFF}; // Unpack into 3 bytes, ignore endianness
    FLASH_Transmit(pageAddressBytes, 3);
    FLASH_CS_High();
    return FLASH_ReadRegister(3);
}

// Erase the block at the given block address (between 0 and 4095)
void FLASH_EraseBlock(uint16_t blockAddress)
{
    FLASH_EraseWithoutDelay(blockAddress);
    osDelay(10); // TODO: Is this delay really needed since we check BUSY bit?
}

// Resets device software and disables write protection
void FLASH_ResetDeviceSoftware(void)
{
    FLASH_AwaitNotBusy();
    FLASH_CS_Low();
    FLASH_Transmit(&RESET_DEVICE, 1);
    FLASH_CS_High();
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
