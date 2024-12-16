/*
 * test.c
 *
 *  Created on: Dec 16, 2024
 *      Author: niyaz
 */

#include "flash.h"

// Listens for commands transmitted to the MCU via UART for testing the flash
void FLASH_ListenCommands(void) {
    UART_Printf("cmd: ");
    char* cmd = UART_ListenInput();
    UART_Printf(cmd);
    UART_Printf("\r\n");
}