# W25N04KV operation set implementation

Implementation of instruction set for Winbond W25N04KV 3V 4G-Bit NAND flash memory with SPI. For use with the Nucleo series of MCUs.

NOTE: Baud rate has been set high at 9 MBits/s to facilitate fast writing and erasing of the flash. For more stability, increase SPI prescaler in the .ioc file.
