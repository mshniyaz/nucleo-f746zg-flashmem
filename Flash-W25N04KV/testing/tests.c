/*
 * tests.c
 *
 * Contains code which runs each test. Note that each test is a FreeRTOS task.
 * The tests accept parameters placed in cmdParamQueue by the function calling the test.
 * All tests should terminate themselves once complete.
 */

#include "W25N04KV.h"
#include "cli.h"

// Custom assert macro to handle errors without program exit
// NOTE: An error boolean variable must be defined prior to assert usage
#define ASSERT(condition, errMessage)                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            printf("[ERROR] %s\r\n\n", errMessage);                                                                    \
            printf("Test Failed: %s (File: %s, Line: %d)\r\n", #condition, __FILE__, __LINE__);                        \
            error = true;                                                                                              \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            printf("[PASSED] %s (File: %s, Line: %d)\r\n", #condition, __FILE__, __LINE__);                            \
        }                                                                                                              \
    } while (0)

// Generic read and write functions
void FLASH_GenericRead(uint16_t columnAddress, uint16_t size, uint8_t *readResponse, uint8_t linesUsed,
                       bool multilineAddress)
{
    if (linesUsed == 2)
    {
        if (multilineAddress)
        {
            FLASH_FastDualReadIO(columnAddress, size, readResponse);
        }
        else
        {
            FLASH_FastDualReadBuffer(columnAddress, size, readResponse);
        }
    }
    else if (linesUsed == 4)
    {
        if (multilineAddress)
        {
            FLASH_FastQuadReadIO(columnAddress, size, readResponse);
        }
        else
        {
            FLASH_FastQuadReadBuffer(columnAddress, size, readResponse);
        }
    }
    else
    {
        FLASH_FastReadBuffer(columnAddress, size, readResponse); // Use 1 line by default
    }
}

void FLASH_GenericWrite(uint8_t *data, uint16_t size, uint16_t columnAddress, uint8_t linesUsed)
{
    if (linesUsed == 4)
    {
        FLASH_QuadWriteBuffer(data, size, columnAddress);
    }
    else
    {
        FLASH_WriteBuffer(data, size, columnAddress); // Use 1 line as default
    }
}

// Print list of commands
void FLASH_GetHelpCmd(void)
{
    printf("\r\n------COMMANDS------\r\n");
    printf("FORMAT:\t<command> [<args>...]\r\n\n");

    printf("help\r\n");
    printf("Displays available commands and descriptions.\r\n\n");

    printf("reset-device\r\n");
    printf("Resets the entire W25N04KV flash memory device.\r\n\n");

    printf("register-test\r\n");
    printf("Verifies the values and functionality of the flash status registers.\r\n\n");

    printf("data-test [SPIType]\r\n");
    printf("[SPIType]: Subcommand, type of SPI to use (dual, dual-io, quad, quad-io),\r\n"
           "where \"-io\" indicates address is multiline. Uses single line if not provided.\r\n");
    printf("Tests whether the read, write, and erase functionality of a flash is working");

    printf("head-tail-test\r\n");
    printf("Ensures flash is able to correctly detect head and tail of circular data buffer.\r\n\n");

    // Print out status details about FreeRTOS
    printf("------FREERTOS DETAILS------\r\n");
    printf("Stack Remaining for current task: %u bytes\r\n", uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
    printf("Free heap: %lu bytes\r\n\n", xPortGetFreeHeapSize());
}

// Sequentially erases all blocks
void FLASH_ResetDeviceCmd(void)
{
    uint32_t startTime = xTaskGetTickCount();
    printf("\r\nPerforming software and data reset...\r\n");
    FLASH_ResetDeviceSoftware();
    FLASH_EraseDevice();
    printf("Reset complete, time taken: %ums\r\n", xTaskGetTickCount() - startTime);

    osThreadExit(); // Safely exit thread
}

// Perform sequence to test registers
void FLASH_TestRegistersCmd(void)
{
    uint32_t startTime = xTaskGetTickCount();
    bool error = false; // Set error flag to default
    printf("\r\nTesting flash's register values and functionality\r\n\n");

    // Check values of each register
    ASSERT(FLASH_ReadRegister(1) == 0, "Unexpected protection register value, some memory blocks are still protected");
    ASSERT(FLASH_ReadRegister(2) == 0x19, "Unexpected configuration register value, configurations are non-default");
    ASSERT(FLASH_ReadRegister(3) == 0, "Unexpected status register value, possible write program or erase failure");

    // Check if WEL bit can be set
    FLASH_WriteEnable();
    ASSERT(FLASH_ReadRegister(3) == 2, "Failed to set WEL bit in status register");

    // Check if BUSY bit correctly sets during erase
    FlashInstruction eraseBlock = {
        .opCode = ERASE_BLOCK,
        .address = 0,
        .addressSize = 3,
    };
    FLASH_QSPIInstruct(&eraseBlock);
    ASSERT(FLASH_IsBusy() == true, "Failed to set BUSY bit in status register during erase operation");
    osDelay(10); // Ensure erase properly terminates
    ASSERT(FLASH_ReadRegister(3) == 0, "WEL and BUSY bits not cleared after erase operation");

    if (!error)
        printf("\r\n[PASSED] All registers configured correctly\r\n");
    else
        printf("\r\n[FAILED] Some tests failed\r\n");
    printf("Time taken: %ums\r\n", xTaskGetTickCount() - startTime);
    osThreadExit(); // Safely exit thread
}

// Performs sequence to test buffer, read, writes, and erase
void FLASH_TestDataCmd(void)
{
    uint32_t startTime = xTaskGetTickCount();
    bool error = false; // Set error flag to default

    // Fetch parameters from queue
    uint32_t linesUsed, multilineAddress, testPageAddress;
    osMessageQueueGet(cmdParamQueueHandle, &linesUsed, NULL, 0);
    osMessageQueueGet(cmdParamQueueHandle, &multilineAddress, NULL, 0);
    osMessageQueueGet(cmdParamQueueHandle, &testPageAddress, NULL, 0);
    printf("\r\nTesting read, write, and erase functionality around page %u\r\n", testPageAddress);

    // Print out type of test
    char *readType = (linesUsed == 4) ? "Quad" : (linesUsed == 2) ? "Dual" : "Single";
    char *addressType = (linesUsed == 4 && multilineAddress)   ? "Quad"
                        : (linesUsed == 2 && multilineAddress) ? "Dual"
                                                               : "Single";
    char *writeType = (linesUsed == 4) ? "Quad" : "Single";
    printf("Using %s read, %s address, %s writes\r\n\n", readType, addressType, writeType);

    // Data buffers
    uint8_t testData[4] = {0x34, 0x5b, 0x78, 0x68};
    uint8_t emptyResponse[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t readResponse[4];

    // Check if data buffer is properly filled
    FLASH_GenericWrite(testData, 4, 0, linesUsed);
    FLASH_GenericRead(0, 4, readResponse, linesUsed, multilineAddress); // Read the buffer from position 0
    ASSERT(memcmp(readResponse, testData, 4) == 0, "Failed to write to the data buffer correctly");

    FLASH_GenericRead(2, 4, readResponse, linesUsed, multilineAddress); // Read the buffer from position 2
    uint8_t bitShiftedResponse[4] = {testData[2], testData[3], 0xFF, 0xFF};
    ASSERT(memcmp(readResponse, bitShiftedResponse, 4) == 0,
           "Failed to read data buffer correctly at a non-zero bit address");

    // Execute write to first 4 bytes of page testPageAddress, testPageAddress+1, testPageAddress+64
    FLASH_WriteExecute(testPageAddress);
    FLASH_GenericWrite(testData, 4, 0, linesUsed);
    FLASH_WriteExecute(testPageAddress + 1);
    FLASH_GenericWrite(testData, 4, 0, linesUsed);
    FLASH_WriteExecute(testPageAddress + 64);
    FLASH_GenericRead(0, 4, readResponse, linesUsed, multilineAddress); // Expect empty buffer after write execute
    ASSERT(memcmp(readResponse, emptyResponse, 4) == 0, "Buffer fails to flush data when writing to a page");

    // Reads page testPageAddress, expects buffer to be full
    FLASH_ReadPage(testPageAddress);
    FLASH_GenericRead(0, 4, readResponse, linesUsed, multilineAddress);
    ASSERT(memcmp(readResponse, testData, 4) == 0, "Failed to write to page and read it into data buffer");

    // Reads page testPageAddress+2, expects buffer to be empty
    FLASH_ReadPage(testPageAddress + 2);
    FLASH_GenericRead(0, 4, readResponse, linesUsed, multilineAddress);
    ASSERT(memcmp(readResponse, emptyResponse, 4) == 0, "Never wrote to page but it is non-empty");

    // Check that block with testPageAddress is properly erased
    FLASH_EraseBlock(testPageAddress / 64);
    FLASH_ReadPage(testPageAddress);
    FLASH_GenericRead(0, 4, readResponse, linesUsed, multilineAddress);
    ASSERT(memcmp(readResponse, emptyResponse, 4) == 0, "Failed to erase block");

    FLASH_ReadPage(testPageAddress + 1);
    FLASH_GenericRead(0, 4, readResponse, linesUsed, multilineAddress);
    ASSERT(memcmp(readResponse, emptyResponse, 4) == 0, "Failed to erase block");

    // Check that page testPageAddress+64 is not erased
    FLASH_ReadPage(testPageAddress + 64);
    FLASH_GenericRead(0, 4, readResponse, linesUsed, multilineAddress);
    ASSERT(memcmp(readResponse, testData, 4) == 0, "Page was erroneously erased");

    // Erase block containing testPageAddress+64 to prep for next test
    FLASH_EraseBlock((testPageAddress / 64) + 1);

    if (!error)
        printf("\r\n[PASSED] Data tests completed successfully\r\n");
    else
        printf("\r\n[FAILED] Some tests failed, ensure tested blocks are empty\r\n");
    printf("Time taken: %ums\r\n", xTaskGetTickCount() - startTime);
    osThreadExit(); // Safely exit thread
}

// Test if the flash memory is able to find head and tail given data with gaps
void FLASH_TestHeadTailCmd(void)
{
    uint32_t startTime = xTaskGetTickCount();
    // Data read buffer and test Packet
    uint8_t testPacket[338] = {
        0x45, 0x8D, 0x35, 0x92, 0x3C, 0xA4, 0x1D, 0xC4, 0x79, 0xEB, 0x41, 0x5F, 0x4B, 0xB4, 0xCC, 0x49, 0x02, 0x53,
        0x24, 0x97, 0x0F, 0x15, 0x4E, 0x87, 0xD8, 0xA1, 0x31, 0xE4, 0x40, 0xDC, 0xF0, 0x6C, 0x68, 0x36, 0xAA, 0x31,
        0xC2, 0x59, 0x8C, 0x35, 0x44, 0xB5, 0x81, 0xB5, 0xE6, 0x92, 0x2A, 0x35, 0x56, 0x0D, 0x43, 0x28, 0xF1, 0x6D,
        0xB2, 0x54, 0xA4, 0x1F, 0xB2, 0xF3, 0x40, 0xA0, 0x83, 0x29, 0x3C, 0x86, 0xE7, 0x93, 0x09, 0xCB, 0x3A, 0xA9,
        0xEC, 0x40, 0xED, 0x26, 0x49, 0x9A, 0xDB, 0xF8, 0x13, 0x35, 0x7B, 0x56, 0x52, 0x89, 0xC4, 0xB9, 0x51, 0xAB,
        0x77, 0x38, 0xA3, 0xCD, 0xD0, 0xB8, 0x6C, 0x8F, 0x42, 0x96, 0x27, 0x8A, 0xCE, 0xC1, 0x58, 0x2C, 0xE0, 0xAF,
        0x2D, 0xF9, 0xAF, 0xAF, 0xA3, 0xC2, 0x12, 0x22, 0x43, 0xBC, 0x72, 0x5F, 0x32, 0xA3, 0xA0, 0x66, 0xC2, 0xE7,
        0xD1, 0x5C, 0x59, 0xB5, 0x6C, 0xCB, 0x1D, 0x6D, 0x77, 0x4F, 0x39, 0x8F, 0x96, 0x5A, 0xE0, 0xD1, 0xF6, 0x24,
        0xEA, 0xBF, 0xFC, 0x81, 0xAF, 0xA6, 0xDB, 0x60, 0xA5, 0x3B, 0x00, 0xC7, 0xB1, 0x43, 0x2C, 0xB7, 0xF5, 0xC7,
        0xCE, 0x3B, 0x7F, 0x56, 0xDB, 0x7E, 0xCE, 0x8C, 0x34, 0xDF, 0x45, 0xCA, 0xCB, 0x42, 0x97, 0x16, 0xB3, 0xA1,
        0x14, 0x54, 0x0C, 0xB1, 0x96, 0xC7, 0x11, 0xF8, 0x24, 0xBE, 0x97, 0xFA, 0x7C, 0x00, 0xF0, 0x7E, 0x73, 0x12,
        0x24, 0xF7, 0xBD, 0xAA, 0xC5, 0xDC, 0x98, 0x64, 0x69, 0x7E, 0xF3, 0x43, 0xF0, 0xE0, 0x21, 0xF6, 0xA1, 0xED,
        0x39, 0xF3, 0x08, 0xC0, 0xEF, 0x98, 0x97, 0xCD, 0x4E, 0xF2, 0x69, 0x38, 0xC7, 0x48, 0x15, 0x42, 0x7C, 0xCA,
        0xB0, 0xA8, 0xF8, 0xF1, 0x97, 0x2E, 0x3D, 0xD7, 0xA4, 0x4B, 0xB8, 0xC2, 0x92, 0xBF, 0xD8, 0x57, 0x64, 0xF2,
        0x3A, 0xA4, 0x38, 0x3F, 0x7C, 0x88, 0xEA, 0xB1, 0x1D, 0x66, 0xD9, 0x28, 0xC2, 0x4C, 0x2D, 0x1F, 0xD9, 0xBB,
        0xFB, 0x73, 0x3E, 0xE1, 0xAA, 0x73, 0x3B, 0x47, 0x4B, 0x3A, 0x2F, 0xFA, 0xF0, 0x47, 0x1D, 0x35, 0xC7, 0x9D,
        0x48, 0xCC, 0xF0, 0x5A, 0x5D, 0x50, 0xBA, 0x5F, 0xED, 0x7A, 0x05, 0x33, 0x90, 0x04, 0x14, 0x27, 0xFB, 0xFC,
        0x05, 0x75, 0x96, 0xD0, 0xF1, 0xAD, 0x62, 0x58, 0x8B, 0x5F, 0xFC, 0xDB, 0xE7, 0x8A, 0x51, 0x59, 0x83, 0x7A,
        0xB2, 0x29, 0x62, 0xC0, 0xFB, 0x71, 0xA1, 0x99, 0x84, 0x25, 0xB8, 0x11, 0x48, 0x4A};
    CircularBuffer buf = {0, 0};
    bool error = false; // Set error flag to default
    printf("\r\nTesting flash's detection of circular buffer head & tail\r\n\n");

    // Packets to contiguous locations in page 0
    FLASH_WriteBuffer(testPacket, 338, 0);
    FLASH_WriteBuffer(testPacket, 338, 338);
    FLASH_WriteBuffer(testPacket, 338, 338 * 2);
    FLASH_WriteExecute(0);
    FLASH_FindHeadTail(&buf, (uint8_t[]){0, 3});
    ASSERT((buf.head == 0 && buf.tail == 1014), "Failed to detect head and tail of contiguous packets in page 0");

    // Packets to contiguous locations in page 1, starting at non-zero position
    FLASH_EraseBlock(0);
    FLASH_WriteBuffer(testPacket, 338, 338);
    FLASH_WriteBuffer(testPacket, 338, 338 * 2);
    FLASH_WriteBuffer(testPacket, 338, 338 * 3);
    FLASH_WriteExecute(1);
    FLASH_FindHeadTail(&buf, (uint8_t[]){0, 3});
    ASSERT((buf.head == 2386 && buf.tail == 3400), "Failed to detect head and tail of contiguous packets in page 1");

    // Additional packet at end of page 2, non-contiguous buffer
    FLASH_WriteBuffer(testPacket, 338, 338 * 4);
    FLASH_WriteExecute(2);
    FLASH_FindHeadTail(&buf, (uint8_t[]){0, 3});
    ASSERT((buf.head == 2386 && buf.tail == 5786),
           "Failed to detect head and tail of non-contiguous packets in page 1 & 2");

    // Erase block where test was conducted to prep for next test
    FLASH_EraseBlock(0);

    if (!error)
        printf("\r\n[PASSED] Head and tail tests completed successfully\r\n");
    else
        printf("\r\n[FAILED] Some tests failed, circular buffer not working properly\r\n");
    printf("Time taken: %ums\r\n", xTaskGetTickCount() - startTime);
    osThreadExit(); // Safely exit thread
}
