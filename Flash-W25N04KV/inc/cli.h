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

// Task which runs the CLI
void W25N04KV_InitCLI(void);

/// @brief Listens for and processes incoming commands for testing and debugging. Not meant to be called except within
/// the FreeRTOS "ListenCommands" task.
void W25N04KV_ListenCommands(void);

// Testing functions
void W25N04KV_ResetDeviceCmd(void);
void W25N04KV_TestRegistersCmd(void);
void W25N04KV_TestDataCmd(void);
void W25N04KV_TestHeadTailCmd(void);

#endif /* CLI_H_ */