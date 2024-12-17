/*
 * test.c
 *
 *  Created on: Dec 16, 2024
 *      Author: niyaz
 */

#include "flash.h"

// Listens for commands transmitted to the MCU via UART for testing the flash
void FLASH_ListenCommands(void)
{
    UART_Printf("cmd: ");
    char cmdBuf[100];
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
        UART_Printf("Memory allocation failed\r\n");
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
        UART_Printf("HELP NOT IMPLEMENTED YET SRY\r\n");
    }
    else if (strcmp(cmd, "reset-device") == 0)
    {
        FLASH_ResetDeviceCmd();
    }
    else if (strcmp(cmd, "read-write-test") == 0) // Test reads and writes
    {
        FLASH_TestReadWrite(testData);
    }
    else if (strcmp(cmd, "erase-test") == 0)
    {
        UART_Printf("Testing erase capabilities\r\n");
    }
    else if (strcmp(cmd, "cycle-test") == 0)
    {
        UART_Printf("Testing integrity over multiple write-erase cycles\r\n");
    }
    else
    {
        UART_Printf("Invalid Command\r\n");
    }
}

// Handle command to reset entire device
void FLASH_ResetDeviceCmd(void)
{
    UART_Printf("\r\nPerforming software reset, changing registers to default\r\n");
    UART_Printf("Disabled write protection for all blocks\r\n");
    FLASH_ResetDevice();
    UART_Printf("Erasing all blocks of device...\r\n");
    FLASH_EraseDevice();
    UART_Printf("Erase complete\r\n\r\n");
    return;
}

// Perform sequence to test reads and writes
// TODO: Test last 2 units to ensure they detect errors
void FLASH_TestReadWrite(uint8_t testData[4])
{
    uint8_t readResponse[4];
    uint8_t emptyResponse[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t bitShiftedResponse[4] = {testData[2], testData[3], 0xFF, 0xFF};

    uint8_t errorResponse[4] = {1, 2, 3, 4}; // TODO: Remove after func done

    // Check if buffer is currently empty
    UART_Printf("\r\nTesting read and write capabilities\r\n");
    UART_Printf("Checking current buffer data\r\n");
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    UART_Printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
                readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
    {
        UART_Printf("[PASSED] Buffer is empty as expected, continuing with test\r\n\r\n");
    }
    else
    {
        UART_Printf("[ERROR] Buffer not empty, reset device by running reset-device\r\n\r\n");
        return; //! Error here if run as first command
    }

    // Write some data to the buffer
    UART_Printf("Writing test data (0x%02X%02X%02X%02X) to buffer at bit position 0x00\r\n",
                testData[0], testData[1], testData[2], testData[3]);
    FLASH_WriteBuffer(testData, 4, 0);    //! Write to buf
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
    UART_Printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
                readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
    {
        UART_Printf("[PASSED] Buffer successfully filled with required data\r\n\r\n");
    }
    else
    {
        UART_Printf("[ERROR] Failed to write to the data buffer correctly\r\n\r\n");
        return;
    }

    // Read at other positions
    UART_Printf("Testing buffer read at data buffer bit address 2 (non-zero), expect 0x%02X%02XFFFF\r\n", readResponse[2], readResponse[3]);
    FLASH_ReadBuffer(2, 4, readResponse); //! Expect bitshifted
    UART_Printf("Buffer at bit address 0x02: 0x%02X%02X%02X%02X\r\n",
                readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, bitShiftedResponse, sizeof(readResponse)) == 0)
    {
        UART_Printf("[PASSED] Buffer is read correctly at non-zero bit addresses\r\n\r\n");
    }
    else
    {
        UART_Printf("[ERROR] Failed to read data buffer correctly at a non-zero bit address\r\n\r\n");
        return;
    }

    // Execute the write
    UART_Printf("Executing write to page 5 (arbitrarily chosen), flushing buffer\r\n");
    FLASH_WriteExecute(5);                //! Write to page 5
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    UART_Printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
                readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
    {
        UART_Printf("[PASSED] Buffer successfully flushes data when writing to a page\r\n\r\n");
    }
    else
    {
        UART_Printf("[ERROR] Buffer fails to flush data when writing to a page\r\n\r\n");
        return;
    }

    // Test reading of the page
    UART_Printf("Reading page 5 into data buffer\r\n");
    FLASH_ReadPage(5);                    //! Read page 5
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
    UART_Printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
                readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
    {
        UART_Printf("[PASSED] Successfully able to write to a page and read it into data buffer\r\n\r\n");
    }
    else
    {
        UART_Printf("[ERROR] Failed to write to a page and read it into data buffer\r\n\r\n");
        return;
    }

    // Test if other pages are still empty
    UART_Printf("Reading page 0 into data buffer, expecting it to be empty (0xFF) \r\n");
    FLASH_ReadPage(0);                    //! Read page 0
    FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
    UART_Printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
                readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
    if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
    {
        UART_Printf("[PASSED] Pages which haven't been written to remain empty\r\n\r\n");
    }
    else
    {
        UART_Printf("[ERROR] Never wrote to page zero but it is non-empty\r\n\r\n");
        return;
    }

    UART_Printf("All tests passed\r\n");
}

void FLASH_TestErase(uint8_t testData[4])
{
    return 0;
}

void FLASH_TestCycle(uint8_t testData[4])
{
    return 0;
}