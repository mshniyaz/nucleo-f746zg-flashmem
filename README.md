# W25N04KV Operation Set Implementation for STM32

Implementation of the instruction set for the Winbond W25N04KV 3V 4G-bit NAND flash memory with SPI. Designed for use with the Nucleo F746ZG. (Using this library with other boards will require additional configuration of the `.ioc` file.)

## Useful Links

1. [Pins for Nucleo F746ZG](https://os.mbed.com/platforms/ST-Nucleo-F746ZG/) (Find your board on mbed)
2. [Datasheet for W25N04KV](https://www.winbond.com/hq/product/code-storage-flash-memory/qspinand-flash/?__locale=en&partNo=W25N04KV)

## `.ioc` Configuration

The `.ioc` file must be configured correctly based on the board being used. The `.ioc` file included is preconfigured for the Nucleo F746ZG.

### Baud Rate

The baud rate has been set to 9 Mbits/s to facilitate fast writing and erasing of the flash. For more stability, increase the prescaler under **Connectivity > SPI > Parameter Settings > Clock Parameters**.

### FreeRTOS Task Stack Sizes

FreeRTOS has been enabled with a single task, `ListenCommands`, running at the lowest priority. This task constantly listens for input from a CLI transmitted to the MCU via UART.  

- The task is assigned a stack size of 1024, which can be adjusted under **Middleware > FreeRTOS > Tasks & Queues > ListenCommands**.  
- Note: A lower stack size may cause a HardFault.

### Pinout Configuration

Six pins on the W25N04KV (refer to the datasheet) must be connected to the MCU (refer to mbed). Update the default pin configurations in the `.ioc` file to match your setup. Default connections are as follows:

1. **Chip Select (CS)**: Connected to PA15, must be set to `GPIO_Output`.  
2. **Master Out Slave In (MOSI)**: Connected to PA7, set to `SPI1_MOSI`.  
3. **Master In Slave Out (MISO)**: Connected to PA6, set to `SPI1_MISO`.  
4. **Clock (CLK)**: Connected to PA5, set to `SPI1_SCK`.  
5. **Power (VCC)**: Connected to `3V3`.  
6. **Ground (GND)**: Connected to `GND`.

## Accessing the CLI for the Flash

After configuring the `.ioc` file, save it to update the generated code in the `main.c` file.  

To access the CLI provided, use a serial communication program like Minicom or Picocom (for Linux) or PuTTY and Tera Term (for Windows). The CLI has been tested with Minicom.  

### Using Minicom

1. Connect the MCU to the computer via USB.  
2. A new directory of the format `/dev/tty*` will appear (e.g., `/dev/ttyACM0`).  
3. Run `sudo minicom -s`.  
4. Select **Serial Port Setup** > Change **Option A** to the correct device path > Save as default > Exit minicom.  
5. Run `minicom` to access the CLI.

The CLI includes a `help` command for more details.

## Flash-W25N04KV Library

The FLASH-W25N04KV library contains functions for operating the flash memory. Detailed documentation can be found in the FLASH-25N04KV folder. Include it in your code with the below:  

```c
#include <flash.h>
```

## Additional Notes

The circular buffer has not yet been implemented. Notable potential issues include:

1. When writing to the data buffer, data exceeding the buffer size is ignored. Dummy bytes must be left at the end of pages to prevent packet splitting between pages.  
2. Since ECC is disabled, the data buffer may have additional bytes (up to 2176 from 2048). If additional bytes are available, `FLASH_TestCycle` (in `test.c`) must be updated appropriately.
