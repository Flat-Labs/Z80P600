#ifndef SPI_FRAM_H
#define SPI_FRAM_H

// SPI port and pins

#define SPI_PORT	PORTB
#define SPI_DDR 	DDRB

#define CS  		0	// Pin B0
#define SCK 		1   // Pin B1
#define MOSI		2	// Pin B2
#define MISO		3	// Pin B3

#define SPI_F4   	0 	//SCK frequency = Fosc/4
#define SPI_F16  	1 	//SCK frequency = Fosc/16
#define SPI_F64  	2 	//SCK frequency = Fosc/64
#define SPI_F128 	3 	//SCK frequency = Fosc/128

// F-RAM opcodes
#define WRITE_CMD	2
#define READ_CMD 	3
#define WREN_CMD 	6 // Write Enable opcode

// prototypes
void SPI_init(uint8_t ClockSpeed);
void SPI_wren(void);
void SPI_write(uint16_t address, uint8_t data);
uint8_t SPI_read(uint16_t address);
void SPI_write_page(uint16_t address, const unsigned char *data);
void SPI_read_page(uint16_t address, unsigned char *data);
void SPI_test_page(void);
void SPI_test(void);

#endif /* SPI_FRAM_H */

