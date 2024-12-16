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
    char *cmd = (char *)malloc(cmdLen + 1); // +1 for null terminator
    if (cmd == NULL)
    {
        UART_Printf("Memory allocation failed\r\n");
        return;
    }
    if (cmdBuf == NULL || cmdBuf[0] == '\0' || cmdLen == 0)
    {
        UART_Printf("Invalid or empty command\r\n");
        free(cmdBuf);
        free(cmd);
        return;
    }

    // If valid, put cmd into another array of approppriate size
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

    // char *tokenBuf[tokenCount];
    // for (int i = 0; i < tokenCount; i++)
    // {
    //     tokenBuf[i] = tokens[i];
    // }

    // Print the tokens
    for (int i = 0; i < tokenCount; i++)
    {
        UART_Printf("Token %d: %s\r\n", i + 1, tokens[i]);
    }

    // Run the command with parameters here
    // if (strcmp(cmd, "help") == 0)
    // {
    //     UART_Printf("HELP ME\r\n");
    // }

    free(cmd);
}