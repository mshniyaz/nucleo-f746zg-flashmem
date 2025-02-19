# W25N04KV Operation Set Implementation for STM32

Implementation of the instruction set for the Winbond W25N04KV 3V 4G-bit NAND flash memory with SPI. Designed for use with the Nucleo F746ZG (Using this library with other boards will require additional configuration of the `.ioc` file.).

Meant for use with the STM32CubeIDE 1.17.0, installed from [this link](https://www.st.com/en/development-tools/stm32cubeide.html), since this program can only be compiled with this IDE.

## Useful Links

1. [Pins for Nucleo F746ZG](https://os.mbed.com/platforms/ST-Nucleo-F746ZG/) (Find your board on mbed)
2. [Datasheet for W25N04KV](https://www.winbond.com/hq/product/code-storage-flash-memory/qspinand-flash/?__locale=en&partNo=W25N04KV)
3. [RM0410 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00224583-stm32f76xxx-and-stm32f77xxx-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf)

## `.ioc` Configuration

![](https://github.com/user-attachments/assets/428bf828-9ed9-429b-8cf4-32e1527f9b25)

The `.ioc` file must be configured correctly based on the board being used. The `.ioc` file included is preconfigured for the Nucleo F746ZG. Pinouts can be changed in the Pinout & Configuration tab, with peripheral config on the left.

### Baud Rate

The baud rate has been set to 9 Mbits/s to facilitate fast writing and erasing of the flash. For more stability, increase the prescaler under **Connectivity > SPI > Parameter Settings > Clock Parameters**.

### FreeRTOS

FreeRTOS has been enabled with a single task, `ListenCommands`, running at the lowest priority. This interrupt-driven task implements a CLI which listens to user input of commands.

- The task is assigned a stack size of 2048 (A lower stack size may cause a HardFault), which can be adjusted under **Middleware > FreeRTOS > Tasks & Queues > ListenCommands**.
- FreeRTOS uses SYSTICK, and hence a different timer, `TIM6` is used for HAL. The timer used for HAL can be changed under **System Core > SYS > Timebase Source**.
- The `ListenCommands` task may create other tasks for individual commands. To prevent hardfault, the total FreeRTOS heap size for all tasks has been increased to 65536 bytes under **Middleware > FreeRTOS > Config Params > TOTAL_HEAP_SIZE**

### Pinout Configuration

Six pins on the W25N04KV (refer to the datasheet for pin locations) must be connected to approppriate pins in the MCU (refer to mbed page for your model). Do not unplug any wires during operation. Update the default pin configurations in the `.ioc` file to match your setup. Default connections are as follows:

1. **Chip Select (CS)**: Connected to PA15, must be set to `GPIO_Output`.
2. **Master Out Slave In (MOSI)**: Connected to PA7, set to `SPI1_MOSI`.
3. **Master In Slave Out (MISO)**: Connected to PA6, set to `SPI1_MISO`.
4. **Clock (CLK)**: Connected to PA5, set to `SPI1_SCK`.
5. **Power (VCC)**: Connected to `3V3` (Minimum currents of 25mA active, 10μA standby, 1μA DPD).
6. **Ground (GND)**: Connected to `GND`.

## Accessing the CLI for the Flash

After configuring the `.ioc` file, save it to update the generated code in the `main.c` file.

To access the Command Line Interface (CLI) provided, use a serial communication program like Minicom or Picocom (for Linux) or PuTTY and Tera Term (for Windows). The CLI has been tested with Minicom on Ubuntu 22.04.5 LTS.

### Using Minicom

1. Connect the MCU to the computer via USB.
2. A new directory of the format `/dev/tty*` will appear (e.g., `/dev/ttyACM0`).
3. If you do not have minicom, install it with `sudo apt-get install minicom`.
4. Run `sudo minicom -s`.
5. Select **Serial Port Setup** > Change **Option A** to the correct device path > Save as default > Exit minicom.  
6. Run `minicom` to access the CLI.

The CLI includes a `help` command for more details. For other serial communication programs, consult their documentation to understand how to use them.

## Flash-W25N04KV Library

The FLASH-W25N04KV library contains functions for operating the flash memory. Detailed documentation can be found in the FLASH-25N04KV folder. Include it in your `main.c` code with the below:

```c
#include <flash.h>
```
