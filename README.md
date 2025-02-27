# W25N04KV Operation Set Implementation for STM32

Implementation of the instruction set for the Winbond W25N04KV 3V 4G-bit NAND flash memory with SPI/QSPI. Designed for use with the Nucleo F746ZG (Using this library with other boards will require additional configuration of the **.ioc** file.).

Meant for use with the [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html), since this program can only be compiled with this IDE. The project has been tested in the following environment:

1. Ubuntu, 22.04.5 LTS
2. STM32CubeIDE, 1.17.0
   - Found in Help > About STM32CubeIDE
3. STM32CubeMX, 6.13.0
   - Found in .ioc file (raw text) > MxCube.Version
4. STM32Cube Firmware Package FW_F7, 1.17.2 
   - Found in .ioc file > Project > MCU and Firmware Package
5. GNU Tools for STM32 (MCU toolchain), 12.3.rel1
   - Found by right clicking project > Properties > C/C++ build > Settings > MCU/MPU toolchain
6. Nucleo F746ZG board

## Useful Links

1. [Pins for Nucleo F746ZG](https://os.mbed.com/platforms/ST-Nucleo-F746ZG/) (Find your board on mbed)
2. [Datasheet for W25N04KV](https://www.winbond.com/hq/product/code-storage-flash-memory/qspinand-flash/?__locale=en&partNo=W25N04KV)
3. [RM0410 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00224583-stm32f76xxx-and-stm32f77xxx-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf)

## .ioc File Configuration

![CubeMX .ioc file editor](https://github.com/user-attachments/assets/428bf828-9ed9-429b-8cf4-32e1527f9b25)

The **.ioc** file must be configured correctly based on the board being used. The **.ioc** file included is preconfigured for the Nucleo F746ZG. After opening the **.ioc** file in the STM32CubeIDE, pinouts can be changed in the Pinout & Configuration tab, with peripheral configuration on the left. 

Configurations for different peripherals are listed below. After configuring the **.ioc** file, save it to update the generated code in the `main.c` file.

### Pinouts

Eight pins (or 6 if using SPI instead of QSPI) on the W25N04KV (refer to its datasheet for pin locations) must be connected to approppriate pins in the MCU (refer to mbed page for your MCU model).

Update the default pin configurations in the **.ioc** file to match your setup. Default connections from the flash to MCU are as follows. Note that 

1. **Power (VCC)**: Connected to any `3V3` (Minimum currents of 25mA active, 10μA standby, 1μA DPD).
2. **Ground (GND)**: Connected to any `GND`.
3. **Chip Select (CS)**: Connected to PB6, set to `QUADSPI_BK1_NCS`
4.  **Clock (CLK)**: Connected to PB2, set to `QUADSPI_CLK`.
5. **Master Out Slave In (MOSI/IO1)**: Connected to PD11, set to `QUADSPI_BK1_IO0` (Since our QSPI is bank 1, BK1).
6. **Master In Slave Out (MISO/IO1)**: Connected to PD12, set to `QUADSPI_BK1_IO1`.
7. **Input/Output 2 (IO2)**: Connected to PD13, set to `QUADSPI_BK1_IO2` (Only required for QSPI).
8. **Input/Output 2 (IO2)**: Connected to PD13, set to `QUADSPI_BK1_IO2` (Only required for QSPI).

### QSPI (Quad-Standard Peripheral Interface)

QSPI, which is used to communicate with the flash, is enabled under **Connectivity > QUADSPI > QuadSPI Mode: Bank1 with Quad SPI Lines`. CubeMX should autodetect the pinouts (which can be verified in `QUADSPI > GPIO Settings`), but if the pinouts are incorrect (cross-check with mbed), they can be reassigned.

The clock used by QSPI is `HCLK`, which has been configured to a frequency of 216MHz. This can be changed in `.ioc > Clock Configuration > HCLK`. Since the flash only functions reliably at 104MHz and below, a prescaler of 3 is applied so effective clock frequency is 216/3 = `72MHz`. To decrease the baud rate (for stability purposes), go to `QUADSPI > Parameter Settings > Clock Prescaler`.

### USART3

UART, which is used by the MCU to communicate with other computers, has been enabled under `Connectivity > USART3 > Mode: Asynchronous`.

- Baud rate has been set to 2Mbit/s. Any higher baud rate may result in the CLI freezing.
- To view UART output or send UART input, a serial communication programme is required. More details in "Accessing the CLI"

### FreeRTOS

FreeRTOS has been enabled under `Middleware > FreeRTOS > Interface: CMSIS_V2`. It has a single single task, `ListenCommands`, running at a high priority priority. This interrupt-driven task implements a CLI which listens to user-issued commands.

- The task is assigned a stack size of 2048 words (A lower stack size may cause a HardFaul due to insufficient memory). The stack size and other task settings can be configured under `FreeRTOS > Tasks & Queues > ListenCommands`.
- FreeRTOS uses SYSTICK, and hence a different timer, `TIM6` is used for HAL. The timer used for HAL can be changed under `System Core > SYS > Timebase Source`.
- The `ListenCommands` task may create other tasks for individual commands. To prevent hardfault, the total FreeRTOS heap size for all tasks has been increased to 65536 bytes under `FreeRTOS > Config Params > TOTAL_HEAP_SIZE`.
- 2 queues have been created under `FreeRTOS > Tasks and Queues`:
  1. `uartQueue`: 64 character buffer which holds user input. When enter is pressed, the queue is read and the containing command is run
  2. `cmdParamQueue`: A buffer able to hold up to 8 32-bit values. Used to pass parameters to tests running as separate FreeRTOS threads.

### Others

Additional code has been added to main.c.

All printf function calls are redirected to use UART by the below:

```c
/* USER CODE BEGIN 4 */
/**
 * @brief  Retargets the C library printf function to the USART.
 *   None
 * @retval None
 */
PUTCHAR_PROTOTYPE
{
    HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, 0xFFFF);

    return ch;
}
/* USER CODE END 4 */
```

To ensure TIM6 does not interrupt debugging, it is frozen during debug halts.
```c
/* USER CODE BEGIN Init */
__HAL_DBGMCU_FREEZE_TIM6();       // Freeze TIM6 during debug halt
setvbuf(stdout, NULL, _IONBF, 0); // Disables buffering for stdout
/* USER CODE END Init */
```

## Accessing the CLI

![Minicom CLI](https://github.com/user-attachments/assets/b8b4b4be-482a-41cf-86c6-1767e4d7e08f)

To access the Command Line Interface (CLI) provided, use a serial communication program like Minicom or Picocom (for Linux) or PuTTY and Tera Term (for Windows). The CLI has been tested with Minicom 2.8 on Ubuntu 22.04.5 LTS.

### Using Minicom

1. Connect the MCU to the computer via USB.
2. A new directory of the format `/dev/tty*` will appear (e.g., `/dev/ttyACM0`).
3. If you do not have minicom, install it with `sudo apt-get install minicom`.
4. Run `sudo minicom -s` to open up settings.
5. Select `Serial Port Setup > Update serial device to /dev/ttyACM0 > Update Bps/Par/Bits to 2000000 8N1 > Save as default > Exit minicom`.
6. Run `minicom` to access the CLI.

The CLI includes a `help` command for more details. For other serial communication programs, consult their documentation to understand how to use them.

## Flash-W25N04KV Library

The FLASH-W25N04KV library contains functions for operating the flash memory. Detailed documentation can be found in the FLASH-25N04KV folder. Include it in your `main.c` code with the below, provided it has been set up correctly:

```c
#include <flash-spi.h>
```
