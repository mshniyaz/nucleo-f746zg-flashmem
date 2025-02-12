/*
 * test.c
 *
 *  Created on: Dec 16, 2024
 *      Author: niyaz
 */

#include "flash.h"

//! Macros for CRC and compile-time constants

#define MAX_CMD_LENGTH 64
#define HELP_CMD 0x8875cac
#define RESET_DEVICE_CMD 0xa730c915
#define REGISTER_TEST_CMD 0x8f0add03
#define READ_WRITE_TEST_CMD 0x415ef1b4
#define ERASE_TEST_CMD 0x5383f2ce
#define CYCLE_TEST_CMD 0xf7bf70e
#define HEAD_TAIL_TEST 0x84c67266

// Implementation of CRC32b

uint32_t crc32(const char *s, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFF;

    for (uint32_t i = 0; i < n; i++)
    {
        char ch = s[i];
        for (uint32_t j = 0; j < 8; j++)
        {
            uint32_t b = (ch ^ crc) & 1;
            crc >>= 1;
            if (b)
                crc = crc ^ 0xEDB88320;
            ch >>= 1;
        }
    }

    return ~crc;
}

//! Utility functions

// Parse parameters given to commands as integers, defaulting if parameter is invalid
// Pass in NULL as param to force default
uint32_t parseParamAsInt(char *param, uint32_t defaultParam, uint32_t maxValue)
{
    if (param == NULL)
    {
        return defaultParam;
    }

    char *endptr;
    unsigned long result = strtoul(param, &endptr, 10); // Convert str to unsigned integer (base 10)

    // Check for invalid input or out-of-range values
    if (*endptr != '\0' || result > UINT32_MAX)
    {
        printf("Parameter '%s' is invalid, expected a non-negative number within range\r\n", param);
        return defaultParam;
    }

    uint32_t intResult = (uint32_t)result;              // Safe cast to uint32_t
    return (intResult >= maxValue) ? maxValue : result; // Clamp value to maximum
}

// Checks if a given string is all spaces
bool isAllSpaces(char *str)
{
    // Iterate through each character in the string
    while (*str != '\0')
    {
        if (*str != ' ')
        {
            return false;
        }
        str++;
    }

    return true;
}

// Finds the head and tail of the flash and stores it into a circular buffer
void findHeadTail(circularBuffer *buf)
{
    bool headFound = false;      // Tracks whether the head has been found
    union pageStructure pageBuf; // Buffer to store the page data

    for (int p = 0; p < 3; p++)
    {
        FLASH_ReadPage(p);
        FLASH_ReadBuffer(0, 2048, pageBuf.bytes);

        // Check first byte of every packet
        for (int i = 0; i < 6; i++)
        {
            pkt packet = pageBuf.page.packetArray[i];
            // printf("\r\n%u, %u, %x\r\n", p, i, packet.dummy);
            if (packet.dummy != 0xFF)
            {
                if (!headFound)
                {
                    buf->head = p * 2048 + i * sizeof(pkt);
                    headFound = true;
                }
                buf->tail = p * 2048 + (i + 1) * sizeof(pkt);
            }
        }
    }
}

//! Command functions
volatile uint8_t receivedByte;
volatile uint8_t cmdIndex = 0;
volatile char cmdBuf[MAX_CMD_LENGTH];

// UART receive callback function for interrupt-driven polling
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (receivedByte == 0x0D) // Enter
    {
        // Null terminate and send command to queue
        cmdBuf[cmdIndex] = '\0';
        osMessageQueuePut(uartQueueHandle, cmdBuf, 0, 0);
        printf("\r\n");
        // Reset input tracking to prep for next command input
        cmdIndex = 0;
    }
    else if (receivedByte == 0x08) // Backspace
    {
        if (cmdIndex > 0)
        {
            printf("\b \b"); // Erase the last character on the terminal
            cmdIndex--;
        }
        HAL_UART_Receive_IT(&huart3, &receivedByte, 1);
    }
    else
    {
        // Add character to buffer and print it
        if (cmdIndex < MAX_CMD_LENGTH - 2)
        {
            cmdBuf[cmdIndex] = receivedByte;
            printf("%c", (char)receivedByte);
            cmdIndex++;
        }
        HAL_UART_Receive_IT(&huart3, &receivedByte, 1);
    }
}

// Begins listening for commands
void FLASH_ListenCommands(void)
{
    printf("cmd: ");
    HAL_UART_Receive_IT(&huart3, &receivedByte, 1); // Start receiving the first byte
}

// Parses a list of tokens as a command given to the flash memory drive
void FLASH_RunCommand(char *cmdStr)
{
    // Parse the command into tokens
    char *tokens[10]; // Max of 10 tokens
    int tokenCount = 0;
    if (cmdStr != NULL && cmdStr[0] != '\0' && strlen(cmdStr) != 0 && !isAllSpaces(cmdStr))
    {
        char *token = strtok(cmdStr, " "); // Sequentially get tokens
        while (token != NULL && tokenCount < 10)
        {
            tokens[tokenCount] = token;
            tokenCount++;
            token = strtok(NULL, " ");
        }
    }
    else
    {
        return;
    }

    // Parse and run each command
    char *cmd = tokens[0];
    uint32_t cmdCRC = crc32(cmd, strlen(cmd));
    uint8_t testData[4] = {0x34, 0x5b, 0x78, 0x68}; // Default test data

    switch (cmdCRC)
    {
    case HELP_CMD:
        FLASH_GetCommandHelp();
        break;
    case RESET_DEVICE_CMD:
        FLASH_ResetDeviceCmd();
        break;
    case REGISTER_TEST_CMD:
        FLASH_TestRegisters();
        break;
    case READ_WRITE_TEST_CMD:
        uint32_t testPageAddress = 1;
        if (tokenCount >= 2)
        {
            testPageAddress = parseParamAsInt(tokens[1], testPageAddress, 262143);
        }
        // Run the command
        FLASH_TestReadWrite(testData, testPageAddress);
        break;
    case ERASE_TEST_CMD:
        uint16_t testBlockAddress = 0;
        if (tokenCount >= 2)
        {
            testBlockAddress = parseParamAsInt(tokens[1], testBlockAddress, 4094);
        }
        // Run the command
        FLASH_TestErase(testData, testBlockAddress);
        break;
    case CYCLE_TEST_CMD:
        uint32_t cycleCount = 1;
        uint32_t pageCount = 262144;
        if (tokenCount >= 2)
        {
            cycleCount = parseParamAsInt(tokens[1], cycleCount, UINT32_MAX);
        }
        if (tokenCount >= 3)
        {
            pageCount = parseParamAsInt(tokens[2], pageCount, 262144);
        };
        // Run the command
        FLASH_TestCycle(testData, cycleCount, pageCount);
        break;
    case HEAD_TAIL_TEST:
        FLASH_TestHeadTail();
        break;
    default:
        printf("Invalid Command\r\n");
    }
}

//! Functions that run each command

// Print out man.txt to help users
void FLASH_GetCommandHelp(void)
{
    printf("\r\nCOMMANDS\r\n");
    printf("FORMAT:\t<command> [<args>...]\r\n\n");

    printf("help\r\n");
    printf("Displays available commands and descriptions.\r\n\n");

    printf("reset-device\r\n");
    printf("Resets the entire flash memory device.\r\n\n");

    printf("register-test\r\n");
    printf("Verifies the values of the memory status registers.\r\n\n");

    printf("read-write-test [testPageAddress]\r\n");
    printf("Defaults: Page Address 0.\r\n");
    printf("Performs read and write tests on the flash memory. Ensures buffer\r\n");
    printf("reads and writes are working, and that data can be written to and\r\n");
    printf("read from a page without modifying other pages\r\n\n");

    printf("erase-test [testBlockAddress]\r\n");
    printf("Defaults: Block address 0.\r\n");
    printf("Tests the erasure of data in flash memory. Block at testBlockAddress\r\n");
    printf("and its subsequent block are filled with data, but only the test block\r\n");
    printf("is erased. The data of both blocks is then checked.\r\n\n");

    printf("cycle-test [cycleCount] [pageCount]\r\n");
    printf("Defaults: 1 cycle, 262144 pages.\r\n");
    printf("Performs repeated write-erase cycles for the first 'pageCount' pages.\r\n\n");
}

// Handle command to reset entire device
void FLASH_ResetDeviceCmd(void)
{
    printf("\r\nPerforming software reset, changing registers to default\r\n");
    printf("Disabled write protection for all blocks\r\n");
    FLASH_ResetDeviceSoftware();
    printf("Erasing all blocks of device (this may take a minute)...\r\n");
    FLASH_EraseDevice();
    printf("Erase complete\r\n\n");
    return;
}

// Perform sequence to test registers
int FLASH_TestRegisters(void)
{
    uint8_t reg1 = FLASH_ReadRegister(1);
    uint8_t reg2 = FLASH_ReadRegister(2);
    uint8_t reg3 = FLASH_ReadRegister(3);

    // Check register 1
    printf("\r\nChecking register 1 (protection register), expecting 0x00 (All protections disabled)\r\n");
    printf("Register 1: 0x%02X\r\n", reg1);
    if (reg1 == 0)
    {
        printf("[PASSED] All protections are correctly disabled\r\n\n");
    }
    else
    {
        printf("[ERROR] Some memory blocks are still protected\r\n\n");
        return 0;
    }

    // Check register 2
    printf("Checking register 2 (configuration register), expecting 0x19 (Default configuration)\r\n");
    printf("Register 2: 0x%02X\r\n", reg2);
    if (reg2 == 0x19)
    {
        printf("[PASSED] Register 2 retains default configuration\r\n\n");
    }
    else
    {
        printf("[ERROR] Configurations have been modified\r\n\n");
        return 0;
    }

    // Check register 3
    printf("Checking register 3 (status register), expecting 0x00 (No failures or operations in progress)\r\n");
    printf("Register 3: 0x%02X\r\n", reg3);
    if (reg3 == 0)
    {
        printf("[PASSED] No failures detected; BUSY and WEL bits are clear as expected\r\n\n");
    }
    else
    {
        printf("[ERROR] Unexpected register value, possible program or erase failure\r\n");
        printf("[ERROR] Writes (WEL) may be enabled, or the program may be busy\r\n\n");
        return 0;
    }

    // Successfully checked all registers
    printf("All registers are correctly configured.\r\n\r\n");
    return 1;
}

// Perform sequence to test reads and writes
int FLASH_TestReadWrite(uint8_t testData[4], uint32_t testPageAddress)
{
    uint8_t readResponse[4];
    uint8_t emptyResponse[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t bitShiftedResponse[4] = {testData[2], testData[3], 0xFF, 0xFF};

    // Check if buffer is currently empty
    printf("\r\nTesting read and write capabilities, please ensure device is wiped before test\r\n");
    printf("Checking current buffer data\r\n");
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Buffer is empty as expected, continuing with test\r\n\n");
    }
    else
    {
        printf("[ERROR] Buffer not empty, reset device by running reset-device\r\n\n");
        return 0;
    }

    // Write some data to the buffer
    printf("Writing test data (0x%02X%02X%02X%02X) to buffer at bit position 0x00\r\n",
           testData[0], testData[1], testData[2], testData[3]);
    FLASH_WriteBuffer(testData, 4, 0);    //! Write to buf
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Buffer successfully filled with required data\r\n\n");
    }
    else
    {
        printf("[ERROR] Failed to write to the data buffer correctly\r\n\n");
        return 0;
    }

    // Read at other positions
    printf("Testing buffer read at data buffer bit address 2 (non-zero), expect 0x%02X%02XFFFF\r\n", readResponse[2], readResponse[3]);
    FLASH_ReadBuffer(2, 4, readResponse); //! Expect bitshifted
    printf("Buffer at bit address 0x02: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, bitShiftedResponse, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Buffer is read correctly at non-zero bit addresses\r\n\n");
    }
    else
    {
        printf("[ERROR] Failed to read data buffer correctly at a non-zero bit address\r\n\n");
        return 0;
    }

    // Execute the write
    printf("Executing write to page %u, flushing buffer\r\n", testPageAddress);
    FLASH_WriteExecute(testPageAddress);  //! Write to the provided page address
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Buffer successfully flushes data when writing to a page\r\n\n");
    }
    else
    {
        printf("[ERROR] Buffer fails to flush data when writing to a page\r\n\n");
        return 0;
    }

    // Test if other pages are still empty
    printf("Reading page 0 into data buffer, expecting it to be empty (0xFF) \r\n");
    FLASH_ReadPage(0);                    //! Read page 0
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Pages which haven't been written to remain empty\r\n\n");
    }
    else
    {
        printf("[ERROR] Never wrote to page zero but it is non-empty\r\n\n");
        return 0;
    }

    // Test reading of the page
    printf("Reading page %u into data buffer\r\n", testPageAddress);
    FLASH_ReadPage(testPageAddress);      //! Read the page at the provided address
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Successfully able to write to a page and read it into data buffer\r\n\n");
    }
    else
    {
        printf("[ERROR] Failed to write to a page and read it into data buffer\r\n\n");
        return 0;
    }

    // Reset pages for next test
    printf("All tests passed\r\n");
    FLASH_EraseBuffer();
    FLASH_WriteExecute(testPageAddress);
    printf("Wiped out page %u for next test\r\n\n", testPageAddress);
    return 1;
}

// Perform sequence to test erase
int FLASH_TestErase(uint8_t testData[4], uint32_t testBlockAddress)
{
    uint8_t readResponse[4];
    uint8_t emptyResponse[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    // Fillup all of blocks testBlockAddress and testBlockAddress+1 with test data
    printf("\r\nTesting erase capabilities, please ensure device is wiped before test\r\n");
    printf("\r\nFilling all pages of blocks %u and %u with test data (0x%02X%02X%02X%02XFFFF...)\r\n\n",
           testBlockAddress, testBlockAddress + 1, testData[0], testData[1], testData[2], testData[3]);
    for (uint32_t p = testBlockAddress * 64; p < testBlockAddress * 64 + 128; p++)
    {
        FLASH_WriteBuffer(testData, 4, 0);
        FLASH_WriteExecute(p);
        FLASH_ReadPage(p);
        FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
    }

    // Verify that page at test address has been filled
    printf("Loading first page of block %u into buffer\r\n", testBlockAddress);
    FLASH_ReadPage(testBlockAddress * 64);
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Data was correctly written to page %u (block %u)\r\n\n", testBlockAddress * 64, testBlockAddress + 1);
    }
    else
    {
        printf("[ERROR] Failed to write data to block %u\r\n\n", testBlockAddress);
        return 0;
    }

    // Verify that first page of subsequent block has been filled
    printf("Loading first page of block %u into buffer\r\n", testBlockAddress + 1);
    FLASH_ReadPage(testBlockAddress * 64 + 64);
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Data was correctly written to page %u (block %u)\r\n\n", testBlockAddress * 64 + 64, testBlockAddress + 1);
    }
    else
    {
        printf("[ERROR] Failed to write data to block %u\r\n\n", testBlockAddress + 1);
        return 0;
    }

    // Erase test block and verify that its first page has been erased
    printf("Erased block %u only and loaded its first page into data buffer\r\n", testBlockAddress);
    FLASH_EraseBlock(testBlockAddress);
    FLASH_ReadPage(testBlockAddress * 64);
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Data was correctly wiped from page %u (block %u)\r\n\n", testBlockAddress * 64, testBlockAddress);
    }
    else
    {
        printf("[ERROR] Failed to erase block %u\r\n\n", testBlockAddress);
        return 0;
    }

    // Verify that subsequent has also been erased
    printf("Loaded second page of first block into data buffer\r\n", testBlockAddress);
    FLASH_ReadPage(testBlockAddress * 64 + 1);
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Data was correctly wiped from page %u (block %u)\r\n\n", testBlockAddress * 64 + 1, testBlockAddress);
    }
    else
    {
        printf("[ERROR] Failed to erase block %u\r\n\n", testBlockAddress);
        return 0;
    }

    // Verify that subsequent block has not been erased
    printf("Loaded non-erased block %u's first page into data buffer\r\n", testBlockAddress + 1);
    FLASH_ReadPage(testBlockAddress * 64 + 64);
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Page %u (block %u) was not erroneously erased\r\n\n", testBlockAddress * 64 + 64, testBlockAddress + 1);
    }
    else
    {
        printf("[ERROR] Block %u was erroneously erased\r\n\n", testBlockAddress + 1);
        return 0;
    }

    FLASH_ReadPage(testBlockAddress * 64);
    printf("All tests passed\r\n\n");
    return 1;
}

// Perform sequence to perform "cycleCount" number of read-write cycles, testing the first "pageCount" pages
void FLASH_TestCycle(uint8_t testData[4], uint8_t cycleCount, uint32_t pageCount)
{
    printf("\r\nTest write and erase for %u cycle(s)\r\n", cycleCount);
    uint8_t writtenData[2048] = {0}; // Array of all 0 to overwrite 0xFF

    // Set some constants
    uint16_t sizeOfData = sizeof(writtenData) / sizeof(writtenData[0]);
    uint8_t writesPerPage = ceil(2048 / sizeOfData);
    uint16_t eraseCount = ceil((double)pageCount / 64);

    // Log the parameters of the cycle test
    printf("Data of size %u bytes requires %u write(s) per page.\r\nTest will write from page 0 to page %u\r\n", sizeOfData, writesPerPage, pageCount - 1);
    printf("Page currently being written to will be logged every 10,000 pages\r\n");
    printf("Block currently erased is logged every 100 blocks\r\n");

    // Iterate cycleCount number of times
    for (int c = 0; c < cycleCount; c++)
    {
        printf("\r\nCYCLE %u\r\n", c + 1);

        // Write into pages required
        printf("Starting write process\r\n");
        for (int p = 0; p < pageCount; p++)
        {
            // Write data to page in portions
            for (int w = 0; w < writesPerPage; w++)
            {
                FLASH_WriteBuffer(writtenData, sizeOfData, w * sizeOfData);
            }
            FLASH_WriteExecute(p);
            if ((p + 1) % 10000 == 0)
            {
                printf("[NOTICE] Written till %u-th page\r\n", p + 1);
            }
        }
        printf("Write complete\r\n\n");

        // Erase pages required
        printf("Starting erase process\r\n");
        for (int e = 0; e < eraseCount; e++)
        {
            FLASH_EraseBlock(e);
            if ((e + 1) % 100 == 0)
            {
                printf("[NOTICE] Erased till %u-th block\r\n", e + 1);
            }
        }
        printf("Erase complete\r\n");
    }

    printf("\r\nCompleted write-erase cycle, run \"read-write-test\" to check flash integrity\r\n\n");
}

// Test if the flash memory is able to find head and tail given data with gaps
void FLASH_TestHeadTail(void)
{
    // Test packet of data
    uint8_t testPacket[338] = {
        0x45, 0x8D, 0x35, 0x92, 0x3C, 0xA4, 0x1D, 0xC4, 0x79, 0xEB, 0x41, 0x5F, 0x4B, 0xB4, 0xCC, 0x49,
        0x02, 0x53, 0x24, 0x97, 0x0F, 0x15, 0x4E, 0x87, 0xD8, 0xA1, 0x31, 0xE4, 0x40, 0xDC, 0xF0, 0x6C,
        0x68, 0x36, 0xAA, 0x31, 0xC2, 0x59, 0x8C, 0x35, 0x44, 0xB5, 0x81, 0xB5, 0xE6, 0x92, 0x2A, 0x35,
        0x56, 0x0D, 0x43, 0x28, 0xF1, 0x6D, 0xB2, 0x54, 0xA4, 0x1F, 0xB2, 0xF3, 0x40, 0xA0, 0x83, 0x29,
        0x3C, 0x86, 0xE7, 0x93, 0x09, 0xCB, 0x3A, 0xA9, 0xEC, 0x40, 0xED, 0x26, 0x49, 0x9A, 0xDB, 0xF8,
        0x13, 0x35, 0x7B, 0x56, 0x52, 0x89, 0xC4, 0xB9, 0x51, 0xAB, 0x77, 0x38, 0xA3, 0xCD, 0xD0, 0xB8,
        0x6C, 0x8F, 0x42, 0x96, 0x27, 0x8A, 0xCE, 0xC1, 0x58, 0x2C, 0xE0, 0xAF, 0x2D, 0xF9, 0xAF, 0xAF,
        0xA3, 0xC2, 0x12, 0x22, 0x43, 0xBC, 0x72, 0x5F, 0x32, 0xA3, 0xA0, 0x66, 0xC2, 0xE7, 0xD1, 0x5C,
        0x59, 0xB5, 0x6C, 0xCB, 0x1D, 0x6D, 0x77, 0x4F, 0x39, 0x8F, 0x96, 0x5A, 0xE0, 0xD1, 0xF6, 0x24,
        0xEA, 0xBF, 0xFC, 0x81, 0xAF, 0xA6, 0xDB, 0x60, 0xA5, 0x3B, 0x00, 0xC7, 0xB1, 0x43, 0x2C, 0xB7,
        0xF5, 0xC7, 0xCE, 0x3B, 0x7F, 0x56, 0xDB, 0x7E, 0xCE, 0x8C, 0x34, 0xDF, 0x45, 0xCA, 0xCB, 0x42,
        0x97, 0x16, 0xB3, 0xA1, 0x14, 0x54, 0x0C, 0xB1, 0x96, 0xC7, 0x11, 0xF8, 0x24, 0xBE, 0x97, 0xFA,
        0x7C, 0x00, 0xF0, 0x7E, 0x73, 0x12, 0x24, 0xF7, 0xBD, 0xAA, 0xC5, 0xDC, 0x98, 0x64, 0x69, 0x7E,
        0xF3, 0x43, 0xF0, 0xE0, 0x21, 0xF6, 0xA1, 0xED, 0x39, 0xF3, 0x08, 0xC0, 0xEF, 0x98, 0x97, 0xCD,
        0x4E, 0xF2, 0x69, 0x38, 0xC7, 0x48, 0x15, 0x42, 0x7C, 0xCA, 0xB0, 0xA8, 0xF8, 0xF1, 0x97, 0x2E,
        0x3D, 0xD7, 0xA4, 0x4B, 0xB8, 0xC2, 0x92, 0xBF, 0xD8, 0x57, 0x64, 0xF2, 0x3A, 0xA4, 0x38, 0x3F,
        0x7C, 0x88, 0xEA, 0xB1, 0x1D, 0x66, 0xD9, 0x28, 0xC2, 0x4C, 0x2D, 0x1F, 0xD9, 0xBB, 0xFB, 0x73,
        0x3E, 0xE1, 0xAA, 0x73, 0x3B, 0x47, 0x4B, 0x3A, 0x2F, 0xFA, 0xF0, 0x47, 0x1D, 0x35, 0xC7, 0x9D,
        0x48, 0xCC, 0xF0, 0x5A, 0x5D, 0x50, 0xBA, 0x5F, 0xED, 0x7A, 0x05, 0x33, 0x90, 0x04, 0x14, 0x27,
        0xFB, 0xFC, 0x05, 0x75, 0x96, 0xD0, 0xF1, 0xAD, 0x62, 0x58, 0x8B, 0x5F, 0xFC, 0xDB, 0xE7, 0x8A,
        0x51, 0x59, 0x83, 0x7A, 0xB2, 0x29, 0x62, 0xC0, 0xFB, 0x71, 0xA1, 0x99, 0x84, 0x25, 0xB8, 0x11,
        0x48, 0x4A};

    // Data read buffer and test data
    circularBuffer buf = {0, 0, 338};

    // Write the test packet to some locations on page 0 (Contiguous packet case)
    FLASH_WriteBuffer(testPacket, 338, 0);
    FLASH_WriteBuffer(testPacket, 338, 338);
    FLASH_WriteBuffer(testPacket, 338, 338 * 2);
    FLASH_WriteExecute(1);
    FLASH_WriteBuffer(testPacket, 338, 0);
    FLASH_WriteExecute(2);

    // Apply algorithm to find head and tail
    findHeadTail(&buf);
    printf("\r\n%u, %u\r\n\r\n", buf.head, buf.tail);
    FLASH_EraseBlock(0);
}