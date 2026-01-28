#ifndef Z80P600_H
#define	Z80P600_H

// Ports Assigment
#define DATA_PORT		PORTF
#define DATA_PINS		PINF
#define DATA_DDR		DDRF

#define HI_ADDR_PORT	PORTC
#define HI_ADDR_DDR		DDRC

#define LO_ADDR_PORT	PORTA
#define LO_ADDR_DDR		DDRA

#define SIGNAL_PORT		PORTE
#define SIGNAL_PINS		PINE
#define SIGNAL_DDR		DDRE

#define WR_PORT			PORTG
#define WR_DDR			DDRG

#define UNUSED_PORT		PORTD
#define UNUSED_DDR		DDRD

// Main signals Pins/Bits
#define Z80_IORQ 7	// Bit 7 of port E
#define Z80_MREQ 6	// Bit 6 of port E
#define Z80_RFSH 5	// Bit 5 of port E
#define Z80_NMI  4	// Bit 4 of port E

#define Z80_RD   1	// Bit 1 of port G
#define Z80_WR   0	// Bit 0 of port G
#define LED  	 5	// Bit 5 of port B

#define PDO	 1 		// PDO = Bit 1 of port E (Output)
#define PDI  0		// PDI = Bit 0 of port E (Input)

#endif	/* Z80P600_H */

