/*
 * test.c
 *
 *  Created on: Dec 16, 2024
 *      Author: niyaz
 */

#include "flash.h"

//! Utility functions

// Parse parameters given to commands as integers
uint32_t parseParamAsInt(char *param)
{
    char *endptr;
    unsigned long result = strtoul(param, &endptr, 10); // Convert str to unsigned integer (base 10)

    // Check for invalid input or out-of-range values
    if (*endptr != '\0' || result > UINT32_MAX)
    {
        printf("Parameter '%s' is invalid, expected a non-negative number within range\r\n", param);
        return UINT32_MAX;
    }

    return (uint32_t)result; // Safe cast to uint32_t
}

// Checks if a token is a valid integer
bool validInt(char *param)
{
    return parseParamAsInt(param) != UINT32_MAX; // Safe cast to uint32_t
}

// Helper function to clamp values
uint32_t clamp(uint32_t value, uint32_t max)
{
    return (value >= max) ? max : value;
}

//! Command functions

// Listens for user input and submits when enter is pressed, storing results in buffers
void UART_ListenInput(char *inputBuffer, int *inputLen)
{
    uint8_t receivedByte;
    uint8_t index = 0;
    char commandBuffer[MAX_INPUT_LEN]; //! == {0} here?

    while (true)
    {
        // Receive one byte
        if (HAL_UART_Receive(&huart3, &receivedByte, 1, HAL_MAX_DELAY) == HAL_OK)
        {
            if (receivedByte == 0x0D) // Enter
            {
                commandBuffer[index] = '\0';
                printf("\r\n");
                break;
            }
            else if (receivedByte == 0x08) // Backspace
            {
                if (index > 0)
                {
                    index--;
                    printf("\b \b"); // Erase the last character on the terminal
                }
            }
            else
            {
                // Add character to buffer and print it
                if (index < MAX_INPUT_LEN - 1)
                {
                    commandBuffer[index] = receivedByte;
                    printf("%c", (char)receivedByte);
                    index++;
                }
                else
                {
                    continue;
                }
            }
        }
    }

    // Copy input to the result buffer safely
    if (inputBuffer != NULL)
    {
        strncpy(inputBuffer, commandBuffer, MAX_INPUT_LEN);
    }

    // Return input length if resultLen is valid
    if (inputLen != NULL)
    {
        *inputLen = index;
    }
}

// Listens for commands transmitted to the MCU via UART for testing the flash
void FLASH_ListenCommands(void)
{
    printf("cmd: ");
    char cmdBuf[MAX_INPUT_LEN];
    int cmdLen;
    UART_ListenInput(cmdBuf, &cmdLen);
    // Ensure command is valid
    if (cmdBuf == NULL || cmdBuf[0] == '\0' || cmdLen == 0)
    {
        return;
    }

    // If valid, put cmd into another array of approppriate size
    char *cmd = (char *)malloc(cmdLen + 1); // +1 for null terminator
    if (cmd == NULL)
    {
        printf("Memory allocation failed\r\n");
        return;
    }
    strncpy(cmd, cmdBuf, cmdLen);
    cmd[cmdLen] = '\0';

    // Parse into command and arguments
    char *tokens[10]; // List to store tokens
    int tokenCount = 0;
    char *token = strtok(cmd, " ");
    while (token != NULL && tokenCount < 10)
    {
        tokens[tokenCount] = token;
        tokenCount++;
        token = strtok(NULL, " ");
    }

    // Clean up and run
    FLASH_ParseCommand(tokens, tokenCount);
    free(cmd);
}

// Parses a list of tokens as a command given to the flash memory drive
void FLASH_ParseCommand(char *tokens[10], uint8_t tokenCount)
{
    char *cmd = tokens[0];
    uint8_t testData[4] = {0x34, 0x5b, 0x78, 0x68};

    // Parse and run each command
    if (strcmp(cmd, "help") == 0) // TODO: Display documentation
    {
        printf("HELP NOT IMPLEMENTED YET SRY\r\n");
    }
    else if (strcmp(cmd, "reset-device") == 0)
    {

        FLASH_ResetDeviceCmd();
    }
    else if (strcmp(cmd, "register-test") == 0) // Test reads and writes
    {

        FLASH_TestRegisters();
    }
    else if (strcmp(cmd, "read-write-test") == 0) // Test reads and writes
    {

        FLASH_TestReadWrite(testData);
    }
    else if (strcmp(cmd, "erase-test") == 0)
    {

        FLASH_TestErase(testData);
    }
    else if (strcmp(cmd, "cycle-test") == 0)
    {
        uint32_t cycleCount = (tokenCount >= 2 && validInt(tokens[1])) ? parseParamAsInt(tokens[1]) : 1;
        uint32_t pageCount = (tokenCount >= 3 && validInt(tokens[2])) ? parseParamAsInt(tokens[2]) : 262144;

        // Run the command
        FLASH_TestCycle(testData, cycleCount, pageCount);
    }
    else
    {
        printf("Invalid Command\r\n");
    }
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

// Perform sequence to test registers, returning success (1) or failure (0)
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

// Perform sequence to test reads and writes, returning a success (1) or failure (0)
int FLASH_TestReadWrite(uint8_t testData[4])
{
    uint8_t readResponse[4];
    uint8_t emptyResponse[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t bitShiftedResponse[4] = {testData[2], testData[3], 0xFF, 0xFF};

    // Check if buffer is currently empty
    printf("\r\nTesting read and write capabilities\r\n");
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
    printf("Executing write to page 5 (arbitrarily chosen), flushing buffer\r\n");
    FLASH_WriteExecute(5);                //! Write to page 5
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
    printf("Reading page 5 into data buffer\r\n");
    FLASH_ReadPage(5);                    //! Read page 5
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
    FLASH_WriteExecute(5);
    printf("Reset page 5 for next test\r\n\n");
    return 1;
}

// Perform sequence to test erases, return success (1) or failure (0)
int FLASH_TestErase(uint8_t testData[4])
{
    uint8_t readResponse[4];
    uint8_t emptyResponse[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    // Fillup all of blocks 0 and 1 with test data
    printf("\r\nFilling all of pages 0 to 127 (blocks 0 and 1) with test data (0x%02X%02X%02X%02X)\r\n\n",
           testData[0], testData[1], testData[2], testData[3]);
    for (uint32_t p = 0; p < 128; p++)
    {
        for (int i = 0; i < 2024; i += 4)
        {
            FLASH_WriteBuffer(testData, 4, i);
        }
        FLASH_WriteExecute(p);
    }

    // Verify that page 0 has been filled
    printf("Loading page 0 (block 0) into buffer\r\n");
    FLASH_ReadPage(0);
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Data was correctly written to page 0 (in block 0)\r\n\n");
    }
    else
    {
        printf("[ERROR] Failed to write data to page 0\r\n\n");
        return 0;
    }

    // Verify that page 64 has been filled
    printf("Loading page 64 (block 1) into buffer\r\n");
    FLASH_ReadPage(64);
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Data was correctly written to page 64 (in block 1)\r\n\n");
    }
    else
    {
        printf("[ERROR] Failed to write data to page 64\r\n\n");
        return 0;
    }

    // Erase block 0 and verify that page 0 has been erased
    printf("Erased block 0 only and loaded erased page 0 into data buffer\r\n");
    FLASH_EraseBlock(0);
    FLASH_ReadPage(0);
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Data was correctly wiped from page 0, leaving only 0xFF\r\n\n");
    }
    else
    {
        printf("[ERROR] Failed to erase block 0, page 0 still holds data\r\n\n");
        return 0;
    }

    // Verify that block 1 has not been erased
    printf("Loaded non-erased page 64 into data buffer\r\n");
    FLASH_ReadPage(64);
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
           readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
    {
        printf("[PASSED] Data remains in page 64, so block 1 was not erased\r\n\n");
    }
    else
    {
        printf("[ERROR] Block 1 was erroneously erased\r\n\n");
        return 0;
    }

    // TODO: Allow for changing of blocks written to
    // TODO: Test read-write integrity after erase

    printf("All tests passed\r\n\n");
    return 1;
}

// Perform sequence to perform "cycleCount" number of read-write cycles, testing the first "pageCount" pages
void FLASH_TestCycle(uint8_t testData[4], uint8_t cycleCount, uint32_t pageCount)
{
    printf("\r\nTest write and erase for %u cycle(s)\r\n", cycleCount);
    uint8_t writtenData[1088] = {0}; // Array of all 0 to overwrite 0xFF
    
    // Set some constants
    uint16_t sizeOfData = sizeof(writtenData) / sizeof(writtenData[0]);
    uint8_t writesPerPage = ceil(2176 / sizeOfData);
    uint16_t eraseCount = ceil(pageCount/64);

    // Log the parameters of the cycle test
    printf("Data of size %u bytes requires %u write(s) per page. Test will write from page 0 to page %u\r\n", sizeOfData, writesPerPage, pageCount - 1);
    printf("Page currently being written to will be logged every 10,000 pages\r\n");
    printf("Block currently erased is logged every 100 blocks\r\n");

    // Iterate cycleCount number of times
    for (int c = 0; c < cycleCount; c++)
    {
        printf("\r\nCYCLE %u\r\n", c+1);

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
            if ((p+1) % 10000 == 0)
            {
                printf("[NOTICE] Written till %u-th page\r\n", p+1);
            }
            HAL_Delay(1);
        }
        printf("Write complete\r\n\n");
        HAL_Delay(100);

        // Erase pages required
        printf("Starting erase process\r\n");
        for (int e=0; e< eraseCount; e++) {
            FLASH_EraseBlock(e);
            if ( (e+1)%100 == 0) {
                printf("[NOTICE] Erased till %u-th block\r\n", e+1);
            }
        }
        printf("Erase complete\r\n");
    }

    printf("\r\nCompleted write-erase cycle, run \"read-write-test\" to check flash integrity\r\n\n");
}
