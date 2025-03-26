#ifndef CLI_H_
#define CLI_H_

#ifndef FLASH_STDLIB_
#define FLASH_STDLIB_
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#endif

/// @brief Listens for and processes incoming commands for testing and debugging. Not meant to be called except within
/// the FreeRTOS "ListenCommands" task.
void FLASH_ListenCommands(void);

// Testing functions
void FLASH_ResetDeviceCmd(void);
void FLASH_TestRegistersCmd(void);
void FLASH_TestDataCmd(void);
void FLASH_TestHeadTailCmd(void);

#endif /* CLI_H_ */