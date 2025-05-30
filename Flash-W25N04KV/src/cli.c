/*
 * cli.c
 *
 * Contains code to run the CLI, including interrupt driven UART inputs.
 * Also contains functions to parse and run commands inputted by user
 */

#include "cli.h"
#include "W25N04KV.h"
#include "cmsis_os.h"

//! Macros for CRC codes of different commands
#define HELP_CMD 0x8875cac
#define RESET_DEVICE_CMD 0xa730c915
#define REGISTER_TEST_CMD 0x8f0add03

#define DATA_TEST_CMD 0xe0220641
#define DUAL_LINE_SUBCMD 0xffd86266
#define DUAL_IO_SUBCMD 0x357f4428
#define QUAD_LINE_SUBCMD 0x96c44df9
#define QUAD_IO_SUBCMD 0xc52ddfae

#define HEAD_TAIL_TEST 0x84c67266

//! Utility functions

// Parses parameters into unsigned integers, clamping them into a given range.
// Invalid inputs are ignored.
void parseParamAsInt(char *paramStr, uint32_t *paramPtr, uint32_t range[2])
{
    // Check if string exists
    if (paramStr == NULL)
        return;

    // Parse using stroul, invalid conversions result in 0
    char *endptr;
    unsigned long result = strtoul(paramStr, &endptr, 10);

    // Check for invalid input or out-of-range values
    if (*endptr != '\0' || result >= UINT32_MAX)
    {
        printf("Parameter '%s' is invalid, expected a non-negative number within "
               "range\r\n",
               paramStr);
        return;
    }

    uint32_t intResult = (uint32_t)result;
    // Clamp into range
    *paramPtr = (intResult >= range[0]) ? intResult : range[0]; // Clamp value to minimum
    *paramPtr = (intResult <= range[1]) ? intResult : range[1]; // Clamp value to maximum
}

// Checks if a given string is all spaces
bool isAllSpaces(char *str)
{
    // Iterate through each character in the string
    while (*str != '\0')
    {
        if (*str != ' ')
            return false;
        str++;
    }

    return true;
}

// Implementation of CRC32b (LSB first)
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

//! CLI functions
uint8_t receivedByte; // Tracks last inputted UART byte
uint8_t cmdIndex = 0;
char cmdBuf[MAX_CMD_LENGTH];

// UART receive callback function for interrupt-driven polling
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (receivedByte == '\n' || receivedByte == '\r')
    {
        // Null terminate and send command to queue
        cmdBuf[cmdIndex] = '\0';
        osMessageQueuePut(uartQueueHandle, cmdBuf, 0, 0);
        printf("\r\n");
        // Reset input tracking to prep for next command input
        cmdIndex = 0;
    }
    else if (receivedByte == '\b')
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

// Task which runs the CLI
void W25N04KV_InitCLI(void)
{
    // Quick restart for the flash
    HAL_Delay(1000);
    W25N04KV_ReadJEDECID();
    W25N04KV_ResetDeviceSoftware();

    // Begin listening for user input
    char receivedCommand[64]; // Buffer to track received command
    W25N04KV_ListenCommands();

    /* Infinite loop */
    for (;;)
    {
        // Wait for command to be shifted into queue
        if (osMessageQueueGet(uartQueueHandle, receivedCommand, NULL, osWaitForever) == osOK)
        {
            FLASH_RunCommand(receivedCommand);
            W25N04KV_ListenCommands(); // Restart listening for next command
        }
    }

    // In case we accidentally exit from task loop
    osThreadTerminate(NULL);
}

// Begins listening for commands
void W25N04KV_ListenCommands(void)
{
    printf("cmd: ");
    HAL_UART_Receive_IT(&huart3, &receivedByte, 1);
}

// Parses and runs given command
void FLASH_RunCommand(char *cmdStr)
{
    // Check if command is valid first
    if (cmdStr == NULL || cmdStr[0] == '\0' || strlen(cmdStr) == 0 || isAllSpaces(cmdStr))
    {
        return;
    }

    // Parse the command into tokens
    char *params[4]; // Max of 4 params
    int paramCount = 0;
    char *token, *cmd;
    cmd = token = strtok(cmdStr, " "); // Get first token (command word)
    while (token != NULL && paramCount < 4)
    {
        token = strtok(NULL, " ");
        if (token != NULL)
        {
            params[paramCount] = token;
            paramCount++;
        }
    }

    // Parse and run each command
    uint32_t cmdHash = crc32(cmd, strlen(cmd));
    // osMessageQueueReset(cmdParamQueueHandle); // Clear any previous params
    switch (cmdHash)
    {
    case HELP_CMD:
        FLASH_GetHelpCmd();
        break;
    case RESET_DEVICE_CMD:
        // Create a new thread to run the reset-device command
        const osThreadAttr_t resetTaskAttr = {.priority = osPriorityHigh};
        if (osThreadNew(W25N04KV_ResetDeviceCmd, NULL, &resetTaskAttr) == NULL)
            printf("Failed to generate reset-device thread\r\n");
        break;
    case REGISTER_TEST_CMD:
        // Create a new thread to run the register-test command
        const osThreadAttr_t registerTaskAttr = {.priority = osPriorityHigh, .stack_size = 512 * 4};
        if (osThreadNew(W25N04KV_TestRegistersCmd, NULL, &registerTaskAttr) == NULL)
            printf("Failed to generate register-test task\r\n");
        break;
    case DATA_TEST_CMD:
        // Default parameter values
        uint32_t testTypeHash, linesUsed, multilineAddress;
        uint32_t testPageAddress = 0; //! This parameter is left as default

        // Parse the params
        if (paramCount >= 1)
        {
            testTypeHash = crc32(params[0], strlen(params[0]));
            // Determine actual params based on hash
            switch (testTypeHash)
            {
            case DUAL_LINE_SUBCMD:
                linesUsed = 2;
                multilineAddress = false;
                break;
            case DUAL_IO_SUBCMD:
                linesUsed = 2;
                multilineAddress = true;
                break;
            case QUAD_LINE_SUBCMD:
                linesUsed = 4;
                multilineAddress = false;
                break;
            case QUAD_IO_SUBCMD:
                linesUsed = 4;
                multilineAddress = true;
                break;
            default:
                linesUsed = 1;
                multilineAddress = false;
                break;
            }
        }
        // if (paramCount >= 2)
        // {
        //     uint32_t pageRange[] = {1, 262144};
        //     parseParamAsInt(params[1], &testPageAddress, pageRange);
        // }

        // Enqueue parameters
        osMessageQueuePut(cmdParamQueueHandle, &linesUsed, 0, 0);
        osMessageQueuePut(cmdParamQueueHandle, &multilineAddress, 0, 0);
        osMessageQueuePut(cmdParamQueueHandle, &testPageAddress, 0, 0);

        // Create a new thread to run the register-test command
        const osThreadAttr_t dataTaskAttr = {.priority = osPriorityHigh, .stack_size = 3000 * 4};
        if (osThreadNew(W25N04KV_TestDataCmd, NULL, &dataTaskAttr) == NULL)
            printf("Failed to generate data-test task\r\n");
        break;
    case HEAD_TAIL_TEST:
        // Craete a new thread to run the head-tail-test command
        const osThreadAttr_t headTailTaskAttr = {.priority = osPriorityHigh, .stack_size = 2048 * 4};
        if (osThreadNew(W25N04KV_TestHeadTailCmd, NULL, &headTailTaskAttr) == NULL)
            printf("Failed to generate head-tail-test task\r\n");
        break;
    default:
        printf("Invalid Command \"%s\" (CRC32: 0x%x)\r\n", cmdStr, cmdHash);
    }
}
