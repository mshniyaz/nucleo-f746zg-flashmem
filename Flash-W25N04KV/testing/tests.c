#include "flash.h"

//! Custom assert macro to handle errors
#define ASSERT(condition, message)                                                                  \
    do                                                                                              \
    {                                                                                               \
        if (!(condition))                                                                           \
        {                                                                                           \
            printf("\r\nTest Failed: %s (File: %s, Line: %d)\r\n", #condition, __FILE__, __LINE__); \
            printf("[ERROR] %s\r\n", message);                                                      \
        }                                                                                           \
    } while (0)

//! Functions that run each command
// Print list of commands
void FLASH_GetHelpCmd(void)
{
    printf("\r\nCOMMANDS\r\n");
    printf("FORMAT:\t<command> [<args>...]\r\n\n");

    printf("help\r\n");
    printf("Displays available commands and descriptions.\r\n\n");

    printf("reset-device\r\n");
    printf("Resets the entire W25N04KV flash memory device.\r\n\n");

    printf("register-test\r\n");
    printf("Verifies the values and functionality of the flash status registers.\r\n\n");

    // TODO: Change the below as required
    printf("data-test [testPageAddress]\r\n");
    printf("Performs tests to ensure buffer, reads, writes, and erases work properly.\r\n");
    printf("Location of tests can be controlled via testPageAddress (page p).\r\n");
    printf("Note that test involves writing and erasing of page p, p+1, and p+64 (next block).\r\n");
    printf("Last block of a page will not be accepted as a valid input for testPageAddress.\r\n\n");

    printf("head-tail-test\r\n");
    printf("Ensures flash is able to correctly detect head and tail of circular data buffer.\r\n\n");

    // Print out status details about FreeRTOS
    printf("FREERTOS DETAILS\r\n");
    printf("Stack Remaining for current task: %u bytes\r\n", uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
    printf("Free heap: %lu bytes\r\n\n", xPortGetFreeHeapSize());
}

// Sequentially erases all blocks
void FLASH_ResetDeviceCmd(void)
{
    osDelay(10); // TODO: Check if buffer time before test begins is necessary
    printf("\r\nPerforming software and data reset...\r\n");
    FLASH_ResetDeviceSoftware();
    FLASH_EraseDevice();
    printf("Reset complete\r\n");

    osThreadExit(); // Safely exit thread
}

// Perform sequence to test registers
void FLASH_TestRegistersCmd(void)
{
    // Check values of each register
    osDelay(10); // TODO: Check if buffer time before test begins is necessary
    ASSERT(FLASH_ReadRegister(1) == 0, "Unexpected protection register value, some memory blocks are still protected");
    ASSERT(FLASH_ReadRegister(2) == 0x19, "Unexpected configuration register value, configurations are non-default");
    ASSERT(FLASH_ReadRegister(3) == 0, "Unexpected status register value, possible write program or erase failure");

    // Check if WEL bit can be set
    FLASH_WriteEnable();
    ASSERT(FLASH_ReadRegister(3) == 2, "Failed to set WEL bit in status register");

    // Check if BUSY bit correctly sets during erase
    int registerDuringErase = FLASH_EraseWithoutDelay(0);
    printf("%u", registerDuringErase);
    ASSERT(registerDuringErase == 3, "Failed to set BUSY bit in status register during erase operation");
    osDelay(10); // Ensure erase properly terminates
    ASSERT(FLASH_ReadRegister(3) == 0, "WEL and BUSY bits not cleared after erase operation");

    printf("[PASSED] All registers configured correctly\r\n");
    osThreadExit(); // Safely exit thread
}

// Performs sequence to test buffer, read, writes, and erase
void FLASH_TestDataCmd(void)
{
    // Fetch parameters from queue
    uint32_t testPageAddress;
    osMessageQueueGet(cmdParamQueueHandle, &testPageAddress, NULL, 0);

    // TODO: Perform the test

    osThreadExit(); // Return to listenCommands thread
}

// // Perform sequence to test reads and writes
// int FLASH_TestReadWriteCmd(uint32_t testPageAddress)
// {
//     uint8_t testData[4] = {0x34, 0x5b, 0x78, 0x68}; // Default test data
//     uint8_t readResponse[4];
//     uint8_t emptyResponse[4] = {0xFF, 0xFF, 0xFF, 0xFF};
//     uint8_t bitShiftedResponse[4] = {testData[2], testData[3], 0xFF, 0xFF};

//     // Check if buffer is currently empty
//     printf("\r\nTesting read and write capabilities, please ensure device is wiped before test\r\n");
//     printf("Checking current buffer data\r\n");
//     FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
//     printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Buffer is empty as expected, continuing with test\r\n\n");
//     }
//     else
//     {
//         printf("[ERROR] Buffer not empty, reset device by running reset-device\r\n\n");
//         return 0;
//     }

//     // Write some data to the buffer
//     printf("Writing test data (0x%02X%02X%02X%02X) to buffer at bit position 0x00\r\n",
//            testData[0], testData[1], testData[2], testData[3]);
//     FLASH_WriteBuffer(testData, 4, 0);    //! Write to buf
//     FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
//     printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Buffer successfully filled with required data\r\n\n");
//     }
//     else
//     {
//         printf("[ERROR] Failed to write to the data buffer correctly\r\n\n");
//         return 0;
//     }

//     // Read at other positions
//     printf("Testing buffer read at data buffer bit address 2 (non-zero), expect 0x%02X%02XFFFF\r\n", readResponse[2], readResponse[3]);
//     FLASH_ReadBuffer(2, 4, readResponse); //! Expect bitshifted
//     printf("Buffer at bit address 0x02: 0x%02X%02X%02X%02X\r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, bitShiftedResponse, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Buffer is read correctly at non-zero bit addresses\r\n\n");
//     }
//     else
//     {
//         printf("[ERROR] Failed to read data buffer correctly at a non-zero bit address\r\n\n");
//         return 0;
//     }

//     // Execute the write
//     printf("Executing write to page %u, flushing buffer\r\n", testPageAddress);
//     FLASH_WriteExecute(testPageAddress);  //! Write to the provided page address
//     FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
//     printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Buffer successfully flushes data when writing to a page\r\n\n");
//     }
//     else
//     {
//         printf("[ERROR] Buffer fails to flush data when writing to a page\r\n\n");
//         return 0;
//     }

//     // Test if other pages are still empty
//     printf("Reading page 0 into data buffer, expecting it to be empty (0xFF) \r\n");
//     FLASH_ReadPage(0);                    //! Read page 0
//     FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
//     printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Pages which haven't been written to remain empty\r\n\n");
//     }
//     else
//     {
//         printf("[ERROR] Never wrote to page zero but it is non-empty\r\n\n");
//         return 0;
//     }

//     // Test reading of the page
//     printf("Reading page %u into data buffer\r\n", testPageAddress);
//     FLASH_ReadPage(testPageAddress);      //! Read the page at the provided address
//     FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
//     printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X \r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Successfully able to write to a page and read it into data buffer\r\n\n");
//     }
//     else
//     {
//         printf("[ERROR] Failed to write to a page and read it into data buffer\r\n\n");
//         return 0;
//     }

//     // Reset pages for next test
//     printf("All tests passed\r\n");
//     FLASH_EraseBuffer();
//     FLASH_WriteExecute(testPageAddress);
//     printf("Wiped out page %u for next test\r\n\n", testPageAddress);
//     return 1;
// }

// // Perform sequence to test erase
// int FLASH_TestEraseCmd(uint32_t testBlockAddress)
// {
//     uint8_t testData[4] = {0x34, 0x5b, 0x78, 0x68}; // Default test data
//     uint8_t readResponse[4];
//     uint8_t emptyResponse[4] = {0xFF, 0xFF, 0xFF, 0xFF};

//     // Fillup all of blocks testBlockAddress and testBlockAddress+1 with test data
//     printf("\r\nTesting erase capabilities, please ensure device is wiped before test\r\n");
//     printf("\r\nFilling all pages of blocks %u and %u with test data (0x%02X%02X%02X%02XFFFF...)\r\n\n",
//            testBlockAddress, testBlockAddress + 1, testData[0], testData[1], testData[2], testData[3]);
//     for (uint32_t p = testBlockAddress * 64; p < testBlockAddress * 64 + 128; p++)
//     {
//         FLASH_WriteBuffer(testData, 4, 0);
//         FLASH_WriteExecute(p);
//         FLASH_ReadPage(p);
//         FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
//     }

//     // Verify that page at test address has been filled
//     printf("Loading first page of block %u into buffer\r\n", testBlockAddress);
//     FLASH_ReadPage(testBlockAddress * 64);
//     FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
//     printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Data was correctly written to page %u (block %u)\r\n\n", testBlockAddress * 64, testBlockAddress + 1);
//     }
//     else
//     {
//         printf("[ERROR] Failed to write data to block %u\r\n\n", testBlockAddress);
//         return 0;
//     }

//     // Verify that first page of subsequent block has been filled
//     printf("Loading first page of block %u into buffer\r\n", testBlockAddress + 1);
//     FLASH_ReadPage(testBlockAddress * 64 + 64);
//     FLASH_ReadBuffer(0, 4, readResponse); //! Expect full
//     printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Data was correctly written to page %u (block %u)\r\n\n", testBlockAddress * 64 + 64, testBlockAddress + 1);
//     }
//     else
//     {
//         printf("[ERROR] Failed to write data to block %u\r\n\n", testBlockAddress + 1);
//         return 0;
//     }

//     // Erase test block and verify that its first page has been erased
//     printf("Erased block %u only and loaded its first page into data buffer\r\n", testBlockAddress);
//     FLASH_EraseBlock(testBlockAddress);
//     FLASH_ReadPage(testBlockAddress * 64);
//     FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
//     printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Data was correctly wiped from page %u (block %u)\r\n\n", testBlockAddress * 64, testBlockAddress);
//     }
//     else
//     {
//         printf("[ERROR] Failed to erase block %u\r\n\n", testBlockAddress);
//         return 0;
//     }

//     // Verify that subsequent has also been erased
//     printf("Loaded second page of first block into data buffer\r\n", testBlockAddress);
//     FLASH_ReadPage(testBlockAddress * 64 + 1);
//     FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
//     printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, emptyResponse, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Data was correctly wiped from page %u (block %u)\r\n\n", testBlockAddress * 64 + 1, testBlockAddress);
//     }
//     else
//     {
//         printf("[ERROR] Failed to erase block %u\r\n\n", testBlockAddress);
//         return 0;
//     }

//     // Verify that subsequent block has not been erased
//     printf("Loaded non-erased block %u's first page into data buffer\r\n", testBlockAddress + 1);
//     FLASH_ReadPage(testBlockAddress * 64 + 64);
//     FLASH_ReadBuffer(0, 4, readResponse); //! Expect empty
//     printf("Buffer at bit address 0x00: 0x%02X%02X%02X%02X\r\n",
//            readResponse[0], readResponse[1], readResponse[2], readResponse[3]);
//     if (memcmp(readResponse, testData, sizeof(readResponse)) == 0)
//     {
//         printf("[PASSED] Page %u (block %u) was not erroneously erased\r\n\n", testBlockAddress * 64 + 64, testBlockAddress + 1);
//     }
//     else
//     {
//         printf("[ERROR] Block %u was erroneously erased\r\n\n", testBlockAddress + 1);
//         return 0;
//     }

//     FLASH_ReadPage(testBlockAddress * 64);
//     printf("All tests passed\r\n\n");
//     return 1;
// }

// // Test if the flash memory is able to find head and tail given data with gaps
// void FLASH_TestHeadTailCmd(void)
// {
//     // Test packet of data
//     uint8_t testPacket[338] = {
//         0x45, 0x8D, 0x35, 0x92, 0x3C, 0xA4, 0x1D, 0xC4, 0x79, 0xEB, 0x41, 0x5F, 0x4B, 0xB4, 0xCC, 0x49,
//         0x02, 0x53, 0x24, 0x97, 0x0F, 0x15, 0x4E, 0x87, 0xD8, 0xA1, 0x31, 0xE4, 0x40, 0xDC, 0xF0, 0x6C,
//         0x68, 0x36, 0xAA, 0x31, 0xC2, 0x59, 0x8C, 0x35, 0x44, 0xB5, 0x81, 0xB5, 0xE6, 0x92, 0x2A, 0x35,
//         0x56, 0x0D, 0x43, 0x28, 0xF1, 0x6D, 0xB2, 0x54, 0xA4, 0x1F, 0xB2, 0xF3, 0x40, 0xA0, 0x83, 0x29,
//         0x3C, 0x86, 0xE7, 0x93, 0x09, 0xCB, 0x3A, 0xA9, 0xEC, 0x40, 0xED, 0x26, 0x49, 0x9A, 0xDB, 0xF8,
//         0x13, 0x35, 0x7B, 0x56, 0x52, 0x89, 0xC4, 0xB9, 0x51, 0xAB, 0x77, 0x38, 0xA3, 0xCD, 0xD0, 0xB8,
//         0x6C, 0x8F, 0x42, 0x96, 0x27, 0x8A, 0xCE, 0xC1, 0x58, 0x2C, 0xE0, 0xAF, 0x2D, 0xF9, 0xAF, 0xAF,
//         0xA3, 0xC2, 0x12, 0x22, 0x43, 0xBC, 0x72, 0x5F, 0x32, 0xA3, 0xA0, 0x66, 0xC2, 0xE7, 0xD1, 0x5C,
//         0x59, 0xB5, 0x6C, 0xCB, 0x1D, 0x6D, 0x77, 0x4F, 0x39, 0x8F, 0x96, 0x5A, 0xE0, 0xD1, 0xF6, 0x24,
//         0xEA, 0xBF, 0xFC, 0x81, 0xAF, 0xA6, 0xDB, 0x60, 0xA5, 0x3B, 0x00, 0xC7, 0xB1, 0x43, 0x2C, 0xB7,
//         0xF5, 0xC7, 0xCE, 0x3B, 0x7F, 0x56, 0xDB, 0x7E, 0xCE, 0x8C, 0x34, 0xDF, 0x45, 0xCA, 0xCB, 0x42,
//         0x97, 0x16, 0xB3, 0xA1, 0x14, 0x54, 0x0C, 0xB1, 0x96, 0xC7, 0x11, 0xF8, 0x24, 0xBE, 0x97, 0xFA,
//         0x7C, 0x00, 0xF0, 0x7E, 0x73, 0x12, 0x24, 0xF7, 0xBD, 0xAA, 0xC5, 0xDC, 0x98, 0x64, 0x69, 0x7E,
//         0xF3, 0x43, 0xF0, 0xE0, 0x21, 0xF6, 0xA1, 0xED, 0x39, 0xF3, 0x08, 0xC0, 0xEF, 0x98, 0x97, 0xCD,
//         0x4E, 0xF2, 0x69, 0x38, 0xC7, 0x48, 0x15, 0x42, 0x7C, 0xCA, 0xB0, 0xA8, 0xF8, 0xF1, 0x97, 0x2E,
//         0x3D, 0xD7, 0xA4, 0x4B, 0xB8, 0xC2, 0x92, 0xBF, 0xD8, 0x57, 0x64, 0xF2, 0x3A, 0xA4, 0x38, 0x3F,
//         0x7C, 0x88, 0xEA, 0xB1, 0x1D, 0x66, 0xD9, 0x28, 0xC2, 0x4C, 0x2D, 0x1F, 0xD9, 0xBB, 0xFB, 0x73,
//         0x3E, 0xE1, 0xAA, 0x73, 0x3B, 0x47, 0x4B, 0x3A, 0x2F, 0xFA, 0xF0, 0x47, 0x1D, 0x35, 0xC7, 0x9D,
//         0x48, 0xCC, 0xF0, 0x5A, 0x5D, 0x50, 0xBA, 0x5F, 0xED, 0x7A, 0x05, 0x33, 0x90, 0x04, 0x14, 0x27,
//         0xFB, 0xFC, 0x05, 0x75, 0x96, 0xD0, 0xF1, 0xAD, 0x62, 0x58, 0x8B, 0x5F, 0xFC, 0xDB, 0xE7, 0x8A,
//         0x51, 0x59, 0x83, 0x7A, 0xB2, 0x29, 0x62, 0xC0, 0xFB, 0x71, 0xA1, 0x99, 0x84, 0x25, 0xB8, 0x11,
//         0x48, 0x4A};

//     // Data read buffer and test data
//     circularBuffer buf = {0, 0, 338};

//     // Write the test packet to some locations on page 0 (Contiguous packet case)
//     FLASH_WriteBuffer(testPacket, 338, 0);
//     FLASH_WriteBuffer(testPacket, 338, 338);
//     FLASH_WriteBuffer(testPacket, 338, 338 * 2);
//     FLASH_WriteExecute(1);
//     FLASH_WriteBuffer(testPacket, 338, 0);
//     FLASH_WriteExecute(2);

//     // Apply algorithm to find head and tail
//     FLASH_FindHeadTail(&buf);
//     printf("\r\n%u, %u\r\n\r\n", buf.head, buf.tail);
//     FLASH_EraseBlock(0);
// }