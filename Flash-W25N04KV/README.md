# Flash-W25N04KV Library Documentation

The FLASH-W25N04KV library contains functions for operating the flash memory. Detailed documentation can be found in the FLASH-25N04KV folder. To ensure your project recognises the library, **Right click your project folder in STM32CubeIDE > Properties > C/C++ General > Paths and Symbols > Add "/${ProjName}/Flash-W25N04KV" as a workspace path**.

Use the library with the below:

```c
#include <W25N04KV.h>
```

This library relies on some of its variables being defined externally, within the file it is included in. All of the below must be configured within the .ioc file of the project. If the given SPI peripherals are already being used, the library must be updated to use the new `SPI_HandleTypeDef` and `UART_HandleTypeDef`.

```c
extern SPI_HandleTypeDef hspi1; // Under connectivity > SPI1
extern UART_HandleTypeDef huart3; // Under connectivity > USART3

// Middleware > FreeRTOS > Tasks and Queues > Add Queue
extern osMessageQueueId_t uartQueueHandle; // uartQueue
extern osMessageQueueId_t cmdParamQueueHandle; // cmdParamQueue
```

All included functions fall into one of the below types:

1. Register Management
2. Read
3. Write
4. Erase
5. Circular Buffer Management
6. Testing

For detailed documentation, check W25N04KV.h, which contains Doxygen style comments.