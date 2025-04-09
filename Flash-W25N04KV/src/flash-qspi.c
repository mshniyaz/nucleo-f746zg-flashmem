/*
 * flash-qspi.c
 *
 * Contains code which implements QSPI instructions which trasmit
 * or receive data on all 4 lines (IO0 to IO3).
 */

#include "W25N04KV.h"

//! Read instructions

// Read buffer on 4 lines
void W25N04KV_FastQuadReadBuffer(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
{
    FlashInstruction fastQuadReadBuffer = {
        .opCode = FAST_QUAD_READ_BUFFER,
        .address = columnAddress,
        .addressSize = 2,
        .dummyClocks = 8,
        .dataMode = RECEIVE,
        .dataBuf = readResponse,
        .dataSize = size,
        .dataLinesUsed = 4,
    };

    W25N04KV_AwaitNotBusy();
    if (W25N04KV_QSPIInstruct(&fastQuadReadBuffer) != 0)
    {
        printf("Error: Failed to read data buffer on 4 lines\r\n");
    }
}

// Read buffer on 4 lines, also send address on 4 lines
void W25N04KV_FastQuadReadIO(uint16_t columnAddress, uint16_t size, uint8_t *readResponse)
{
    FlashInstruction fastQuadReadIO = {
        .opCode = FAST_QUAD_READ_IO,
        .address = columnAddress,
        .addressSize = 2,
        .addressLinesUsed = 4,
        .dummyClocks = 4,
        .dataMode = RECEIVE,
        .dataBuf = readResponse,
        .dataSize = size,
        .dataLinesUsed = 4,
    };

    W25N04KV_AwaitNotBusy();
    if (W25N04KV_QSPIInstruct(&fastQuadReadIO) != 0)
    {
        printf("Error: Failed to send address and read data buffer on 4 lines\r\n");
    }
}

//! Write instructions

// Write to the flash memory's data buffer on 4 lines
void W25N04KV_QuadWriteBuffer(uint8_t *data, uint16_t size, uint16_t columnAddress)
{
    FlashInstruction quadWriteBuffer = {
        .opCode = QUAD_WRITE_BUFFER,
        .address = columnAddress,
        .addressSize = 2,
        .dataMode = TRANSMIT,
        .dataBuf = data,
        .dataSize = size,
        .dataLinesUsed = 4,
    };

    W25N04KV_AwaitNotBusy();
    W25N04KV_WriteEnable();
    if (W25N04KV_QSPIInstruct(&quadWriteBuffer) != 0)
    {
        printf("Error: Failed to write to data buffer on 4 lines\r\n");
    }
}
