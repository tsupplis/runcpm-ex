#include "defaults.h"
#include "ram.h"

#ifdef ARDUINO

#ifndef RAM_SPI
static uint8_t RAM[RAM_SIZE*1024]={0};			// Definition of the emulated RAM
#else
#include <SPI.h>
#include <SRAM_23LC.h>
SRAM_23LC SRAM(&RAM_SPI_PERIPHERAL, RAM_SPI_CS, RAM_SPI_CHIP);
#endif

uint8_t _ram_read(uint16_t address) {
#ifdef RAM_SPI
    return SRAM.readByte(address);
#else
	return(RAM[address]);
#endif
}

void _ram_write(uint16_t address, uint8_t value) {
#ifdef RAM_SPI
  SRAM.writeByte(address,value);
#else
	RAM[address] = value;
#endif
}

#endif
