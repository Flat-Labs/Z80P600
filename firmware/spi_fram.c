#include <avr/io.h>
#include <util/delay.h>
#include <string.h>

#include "Z80P600.h"
#include <hardware.h>
#include "spi_fram.h"

// Init SPI communications with F-RAM

void SPI_init(uint8_t ClockSpeed)
{

 // set CS, MOSI and SCK to output
    SPI_DDR |= (1 << CS) | (1 << MOSI) | (1 << SCK);

 // set MISO to input
	SPI_DDR &= ~(1 << MISO);
	
 // enable SPI, set as master, SPI Mode 3, MSB first (DORD = 0)
    SPCR = 0;
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << CPOL) | (1 << CPHA);

 // set SCK frequency
	SPCR |= ClockSpeed; 
}

// Send Write Enable command through SPI
// (Have to be done prior to any SPI write)

/*FORCEINLINE*/ void SPI_wren(void)
{
 // Enable Write
    SPI_PORT &= ~(1 << CS);			// Drive slave select low

	SPDR = WREN_CMD;				// Allow write opérations
    while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
	}
    SPI_PORT |= (1 << CS);			// Return slave select to high
}

// Writes a byte to F-RAM
/*FORCEINLINE*/ void SPI_write(uint16_t address, uint8_t data)
{
	uint8_t lo, hi;
	
	SPI_wren();
	
 // Split 16-bit address into two bytes
	hi = (uint8_t) ((address & 0xFF00) >> 8); 	// High byte of address
	lo = (uint8_t)  (address & 0x00FF); 		// Low byte of address

 // Send address & data
    SPI_PORT &= ~(1 << CS);			// Drive slave select low

	SPDR = WRITE_CMD;				// Write opcode SPI data register
    while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
	}

	SPDR = hi;						// High byte to SPI data register
    while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
	}

	SPDR = lo;						// Low byte to SPI data register
    while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
	}

	SPDR = data;					// Data byte to SPI data register
    while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
	}
	
    SPI_PORT |= (1 << CS);			// Return slave select to high
}

// Reads a byte from F-RAM
/*FORCEINLINE*/ uint8_t SPI_read(uint16_t address)
{
	uint8_t lo, hi;
	
 // Split 16-bit address into two bytes
	hi = (uint8_t) ((address & 0xFF00) >> 8); 	// High byte of address
	lo = (uint8_t)  (address & 0x00FF); 		// Low byte of address

 // Send address & data
    SPI_PORT &= ~(1 << CS);			// Drive slave select low

	SPDR = READ_CMD;				// Write opcode SPI data register
    while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
	}
	SPDR = hi;						// High byte to SPI data register
    while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
	}
	SPDR = lo;						// Low byte to SPI data register
    while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
	}
	SPDR = 0xFF;					// Dummy byte to SPI data register
    while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
	}
	
    SPI_PORT |= (1 << CS);			// Return slave select to high
	
	return SPDR;					// Return the data value from ram
	
}

// Writes a STORAGE_PAGE_SIZE (256 bytes) page to F-RAM
//
/*FORCEINLINE*/ void SPI_write_page(uint16_t address, const unsigned char *data)
{
	uint16_t i;
	
 // Write enable
	SPI_wren();
	
 // Send address & data
    SPI_PORT &= ~(1 << CS);				// Drive slave select low

	SPDR = WRITE_CMD;					// Write opcode SPI data register
    while (!(SPSR & (1 << SPIF))) {		// Wait for transmission complete
	}

 // High byte of address is the page number
 // Low address always start at 0x00
	SPDR = address;						// High byte to SPI data register
    while (!(SPSR & (1 << SPIF))) {		// Wait for transmission complete
	}

	SPDR = 0x00;						// Low byte to SPI data register
    while (!(SPSR & (1 << SPIF))) {		// Wait for transmission complete
	}

 // Continuously write data to F-RAM (Address counter is automatically incremented)
	for (i = 0; i < STORAGE_PAGE_SIZE; i++) {
		SPDR = (uint8_t)(*data++); 		// Data byte to SPI data register
		while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
		}
	}
	
    SPI_PORT |= (1 << CS);				// Return slave select to high
}

// Reads a STORAGE_PAGE_SIZE (256 bytes) page from F-RAM

/*FORCEINLINE*/ void SPI_read_page(uint16_t address, unsigned char *data)
{
	uint16_t i;

 // Send address & data
    SPI_PORT &= ~(1 << CS);				// Drive slave select low
	
	SPDR = READ_CMD;					// Write opcode SPI data register
    while (!(SPSR & (1 << SPIF))) {		// Wait for transmission complete
	}	

 // High byte of address is the page number
 // Low address always start at 0x00
	SPDR = address;						// High byte to SPI data register
    while (!(SPSR & (1 << SPIF))) {		// Wait for transmission complete
	}

	SPDR = 0x00;						// Low byte to SPI data register
    while (!(SPSR & (1 << SPIF))) {		// Wait for transmission complete
	}

 // Continuously read data from F-RAM (Address counter is automatically incremented)
	for(i = 0; i < STORAGE_PAGE_SIZE; i++) {
		SPDR = 0xFF;					// Dummy byte to SPI data register
		while (!(SPSR & (1 << SPIF))) {	// Wait for transmission complete
		}
		*data++ = SPDR;					// Received value to data
	}
	
    SPI_PORT |= (1 << CS);				// Return slave select to high
}

// Test routines for F-RAM

// Tests page write and read : Writes and read a page of values, and cheks for errors

void SPI_test_page(void)
{
	uint16_t i;
	uint8_t error, page;
	uint8_t buf[STORAGE_PAGE_SIZE];

	page = 0x10;

	for (i = 0; i < STORAGE_PAGE_SIZE; i++) {
		buf[i] = i;
	}
	
 // Write a page
	SPI_write_page(page, buf);
		
 // Set buffer to 0
	memset(buf, 0, sizeof(buf));

 // Read the page from F-RAM
	SPI_read_page(page, buf);
	
 // Check for errors
	error = 0;
	for (i = 0; i < STORAGE_PAGE_SIZE; i++) {
		if (buf[i] != i) {
			error++;
		}
	}

 // Blink led accordingly to results
	if (error > 0) {
		for (i = 0; i <= 15; i++) {
			_delay_ms(200);
			SPI_PORT = SPI_PORT ^ (1 << LED); // Toggles the led rapidly on error
		}
	}
	else {
		for (i = 0; i <= 5; i++) {
			_delay_ms(1000);
			SPI_PORT = SPI_PORT ^ (1 << LED); // Toggles the led slowly if test OK
		}
	}

}

// SPI Test : Writes the whole F-RAM memory and read it back checking for errors
void SPI_test(void)
{
	uint8_t  error, red;
	uint32_t i, n;
	
	n = 0x10000; // Max memory address

 // Write a value
	for (i = 0; i < n; i++) {
		SPI_write(i, (uint8_t) (i & 0x00FF));
	}
	
	error = 0;
 // Read the value and test
	for (i = 0; i < n; i++) {
		red = SPI_read(i);
		if (red != (uint8_t) (i & 0x00FF)) {
			error = 1;
		}
	}
	
 // Blink led accordingly to results
	if (error == 1) {
		for (i = 0; i <= 15; i++) {
			_delay_ms(200);
			SPI_PORT = SPI_PORT ^ (1 << LED); // Toggles the led rapidly on error
		}
	}
	else {
		for (i = 0; i <= 9; i++) {
			_delay_ms(1000);
			SPI_PORT = SPI_PORT ^ (1 << LED); // Toggles the led slowly if test OK
		}
	}
}

