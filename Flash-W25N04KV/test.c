/*
 * test.c
 *
 *  Created on: Dec 16, 2024
 *      Author: niyaz
 */

#include "flash.h"

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
    else if (strcmp(cmd, "registers-test") == 0) // Test reads and writes
    {
        FLASH_TestReadWrite(testData);
    }
    else if (strcmp(cmd, "read-write-test") == 0) // Test reads and writes
    {
        FLASH_TestReadWrite(testData);
    }
    else if (strcmp(cmd, "erase-test") == 0)
    {
        printf("Testing erase capabilities\r\n");
    }
    else if (strcmp(cmd, "cycle-test") == 0)
    {
        printf("Testing integrity over multiple write-erase cycles\r\n");
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
    FLASH_ResetDevice();
    printf("Erasing all blocks of device (this may take a minute)...\r\n");
    FLASH_EraseDevice();
    printf("Erase complete\r\n\n");
    return;
}

// Perform sequence to test registers
void FLASH_TestRegisters(void)
{
}

// Perform sequence to test reads and writes
void FLASH_TestReadWrite(uint8_t testData[4])
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
        return; //! Error here if run as first command
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
        return;
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
        return;
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
        return;
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
        return;
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
        return;
    }

    // Reset pages for next test
    printf("All tests passed\r\n");
    FLASH_EraseBuffer();
    FLASH_WriteExecute(5);
    printf("Reset page 5 for next test\r\n\n");
}

// TODO: Perform sequence to test erases
void FLASH_TestErase(uint8_t testData[4])
{
    return;
}

// TODO: Perform sequence to test cycles, accept a parameter input!
void FLASH_TestCycle(uint8_t testData[4])
{
    return;
}