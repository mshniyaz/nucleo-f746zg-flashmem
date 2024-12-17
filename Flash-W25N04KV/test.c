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
    uint8_t testPattern[4] = {0x34, 0x5b, 0x78, 0x68};

    // Parse and run each command
    if (strcmp(cmd, "help") == 0) // TODO: Display documentation
    {
        UART_Printf("HELP NOT IMPLEMENTED YET SRY\r\n");
    }
    else if (strcmp(cmd, "read-write-test") == 0) // Test reads and writes
    {
        UART_Printf("Testing read and write capabilities\r\n");
        FLASH_TestReadWrite(testPattern);
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

void FLASH_TestReadWrite(uint8_t testPattern[4])
{
    return 0;
}

void FLASH_TestErase(uint8_t testPattern[4])
{
    return 0;
}

void FLASH_TestCycle(uint8_t testPattern[4])
{
    return 0;
}