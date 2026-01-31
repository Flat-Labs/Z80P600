#include <avr/io.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/crc16.h>
#include <string.h>

#include "Z80P600.h"
#include "spi_fram.h"

#include "synth.h"

#define CLOBBERED(reg)	__asm__ __volatile__ ("" : : : reg);

#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))
#define CPU_16MHz       0x00
#define CPU_8MHz        0x01
#define CPU_4MHz        0x02
#define CPU_2MHz        0x03
#define CPU_1MHz        0x04
#define CPU_500kHz      0x05
#define CPU_250kHz      0x06
#define CPU_125kHz      0x07
#define CPU_62kHz       0x08

#define MEMZONE		0		//	/MREQ = 0	/RFSH = 1	/IORQ = 1
#define IOZONE		1		//	/MREQ = 1	/RFSH = 0	/IORQ = 0

// Writes to synth hardware

static FORCEINLINE void hardware_write(int8_t io, uint16_t addr, uint8_t data)
{
 // equivalent to setIdle(1);
	WR_PORT |= (1 << Z80_WR) | (1 << Z80_RD);  // /WR and /RD both high
	SIGNAL_PORT = (SIGNAL_PORT | (1 << Z80_MREQ)) & ~(1 << Z80_RFSH); // /RFSH = 0, /MREQ = 1 and /IORQ untouched
	
 // Prepare write : Data port as output
	DATA_DDR = 0xFF;

	switch (io) {
		case MEMZONE:	// io = 0 ->  /IORQ to 1 end /MREQ to 0
			SIGNAL_PORT = (SIGNAL_PORT | (1 << Z80_IORQ)) & ~(1 << Z80_MREQ);
		break;
		
		case IOZONE:	// io = 1 ->  /IORQ to 0 and /MREQ to 1 
			SIGNAL_PORT = (SIGNAL_PORT | (1 << Z80_MREQ)) & ~(1 << Z80_IORQ);
		break;
	}
		
 // Transfer data to port
	DATA_PORT = data;
	
 // Split 16-bit address into two bytes and output them on the ports
	HI_ADDR_PORT = (uint8_t) ((addr & 0xFF00) >> 8); 	// High byte on port C
	LO_ADDR_PORT = (uint8_t)  (addr & 0x00FF); 			// Low byte on port A
		
 // /RFSH = 1 and /WR = 0 (to write the data)
	SIGNAL_PORT = SIGNAL_PORT | (1 << Z80_RFSH); // /RFSH = 1
	WR_PORT &= ~(1 << Z80_WR);  
}

// Reads from synth hardware

static FORCEINLINE uint8_t hardware_read(int8_t io, uint16_t addr)
{
	uint8_t value;

 // equivalent to setIdle(1);
 	WR_PORT |= (1 << Z80_WR) | (1 << Z80_RD); // /WR and /RD both high
    SIGNAL_PORT = (SIGNAL_PORT | (1 << Z80_MREQ)) & ~(1 << Z80_RFSH); // /RFSH = 0, /MREQ = 1 and /IORQ untouched
	
 // Prepare read : Data port as input
	DATA_DDR = 0x00;

	switch (io) {
		case MEMZONE:	// io = 0 ->  /IORQ to 1 end /MREQ to 0
			SIGNAL_PORT = (SIGNAL_PORT | (1 << Z80_IORQ)) & ~(1 << Z80_MREQ);
		break;
		
		case IOZONE:	// io = 1 ->  /IORQ to 0 and /MREQ to 1 
			SIGNAL_PORT = (SIGNAL_PORT | (1 << Z80_MREQ)) & ~(1 << Z80_IORQ);
		break;
	}

 // Split 16-bit address into two bytes and output them on the ports
	HI_ADDR_PORT = (uint8_t) ((addr & 0xFF00) >> 8); 	// High byte on port C
	LO_ADDR_PORT = (uint8_t)  (addr & 0x00FF); 			// Low byte on port A

	SIGNAL_PORT = SIGNAL_PORT | (1 << Z80_RFSH); // /RFSH = 1
	WR_PORT &= ~(1 << Z80_RD);  				 // /RD to 0 (to read the data)

	CYCLE_WAIT(2);	//  Wait a bit

 // read data
	value = DATA_PINS;

 // Toggle back /WR and /RD high
 // equivalent to setIdle(0)
	WR_PORT |= (1 << Z80_WR) | (1 << Z80_RD);  
	SIGNAL_PORT = (SIGNAL_PORT | (1 << Z80_IORQ)) & ~(1 << Z80_RFSH); // /RFSH = 0, /IORQ = 1 and /MREQ untouched
	
 // Return to default: Data port as output
	DATA_DDR = 0xFF;

	return value;
}

// Write routines (mem, io, uart)
FORCEINLINE void mem_write(uint16_t address, uint8_t value)
{
	hardware_write(0, address, value);
}
FORCEINLINE void io_write(uint8_t address, uint8_t value)
{
	hardware_write(1, address, value);
}

// Read routines (mem, io, uart)
FORCEINLINE uint8_t mem_read(uint16_t address)
{
	return hardware_read(0, address);
}
FORCEINLINE uint8_t io_read(uint8_t address)
{
	return hardware_read(1, address);
}

FORCEINLINE int8_t hardware_getNMIState(void)
{
	return !(SIGNAL_PINS & (1 << Z80_NMI));
}

static FORCEINLINE void hardware_init(int8_t ints)
{
 
 // Z80P600 board pinout
 //
 // MSB -> LSB (NC = No Connection)
 // Port A : A7-A0  (Out)
 // Port B : NC, NC, LED (Out, active High), NC, /RAM_SPI_OUT (in to µC), /RAM_SPI_IN (out from µC), /RAM_SPI_CLK (Out), /RAM_SPI_CS (Out) 
 // Port C : A15-A8 (Out)
 // Port D : D7-D0 (In/Out)
 // Port E : /RFSH (Out), /IORQ (Out), /MREQ (Out), /NMI (In, mapped to INT4), NC, NC, PDO (Out), PDI (In)
 // Port F : Unused
 // Port G : NC, NC, NC, NC, NC, NC, /RD, /WR

 // Init Port Direction Registers and Values
	LO_ADDR_DDR = 0xFF; 							// Low Addresses = Out
	HI_ADDR_DDR = 0xFF;	 							// High Addresses = Out
	DATA_DDR    = 0xFF; 							// Data Lines as output for the moment
		
	SPI_DDR     = 0xFF & ~(1 << MISO); 				// Only SPI_IN as input
	WR_DDR      = 0xFF; 							// All outputs
		
	SIGNAL_DDR  = 0xFF & ~(1 << Z80_NMI);			// Signal port : All outputs except /NMI

	UNUSED_DDR  = 0x00; 							// Unused Ports -> out
	UNUSED_PORT = 0xFF; 							// Pullups on unused Ports
		
 // Init Port Values		
	LO_ADDR_PORT = 0x00;							// Low address = 0
	HI_ADDR_PORT = 0x00;							// High address = 0
	DATA_PORT    = 0x00;							// Data byte to 0

	SPI_PORT = 0x00 | (1 << LED) | (1 << CS);		// Led On and /CS high

	SIGNAL_PORT = ~(1 << Z80_NMI);					// All signals to 1 (Inactive) except /NMI (input)

	WR_PORT = 0xFF;									// /WR & /RD high (Inactive)

 // Init SPI stuff for F-RAM
	SPI_init(SPI_F4); 	// SPI Clock @ 4MHz (up to 40MHz according to FM25V05 datasheet)
	
	if (ints)
	{
		// prepare a 2Khz interrupt

		OCR0A = 124;
		TCCR0A |= (1 << WGM01); 				//Timer 0 Clear-Timer on Compare (CTC) 
		TCCR0B |= (1 << CS01) | (1 << CS00);	//Timer 0 prescaler = 64
		TIMSK0 |= (1 << OCIE0A); 				//Enable overflow interrupt for Timer0

#ifdef UART_USE_HW_INTERRUPT	
		EIMSK |= (1 << INT4); 	// enable INT4
//		EICRB  = 0x00;			// Low level on INT4 triggers the interrupt
#else
		// prepare a 5Khz interrupt

		OCR2A = 49;
		TCCR2A |= (1 <<WGM21); 	//Timer 2 Clear-Timer on Compare (CTC) 
		TCCR2B |= (1 <<CS22);  	//Timer 2 prescaler = 64
		TIMSK2 |= (1 <<OCIE2A); //Enable overflow interrupt for Timer2
#endif	
	}

	hardware_read(0,0); // init r/w system
}

// Writes to F-RAM
void storage_write(uint32_t pageIdx, uint8_t *buf)
{
	if(pageIdx < (STORAGE_SIZE/STORAGE_PAGE_SIZE))
		SPI_write_page(pageIdx, &buf[0]);
}

// Reads from F-RAM
void storage_read(uint32_t pageIdx, uint8_t *buf)
{
	if(pageIdx < (STORAGE_SIZE/STORAGE_PAGE_SIZE))
		SPI_read_page(pageIdx, &buf[0]);
}

#define NRWW_SECTION(sec) __attribute__((section (sec))) __attribute__((noinline)) __attribute__((optimize ("O1"))) __attribute__((used))

void NRWW_SECTION(".bootspace") boot_program_page (uint32_t page, uint8_t *buf)
{
	uint16_t i;
	uint8_t sreg;

	// Disable interrupts.

	sreg = SREG;
	cli();

	eeprom_busy_wait ();

//	blHack_page_erase (page);
	boot_page_erase (page);
	boot_spm_busy_wait ();      // Wait until the memory is erased.

	for (i=0; i<SPM_PAGESIZE; i+=2)
	{
		// Set up little-endian word.

		uint16_t w = *buf++;
		w += (*buf++) << 8;

        boot_page_fill (page + i, w);
//		blHack_page_fill (page + i, w);
	}

//	blHack_page_write (page);     // Store buffer in flash page.
	boot_page_write (page);     // Store buffer in flash page.
	boot_spm_busy_wait();       // Wait until the memory is written.

	// Re-enable RWW-section again. We need this if we want to jump back
	// to the application after bootloading.

//	blHack_rww_enable ();
	boot_rww_enable ();

	// Re-enable interrupts (if they were ever enabled).

	SREG = sreg;
}


static int16_t NRWW_SECTION(".nrww_misc") getMidiByte(void)
{
	uint8_t data,status;

	CYCLE_WAIT(4);

//	while(PINC&0x10) // wait for NMI

	while (SIGNAL_PINS & (1 << Z80_NMI)) // wait for NMI
		CYCLE_WAIT(1);
	
	status = hardware_read(0, 0xe000); // read UART status
	CYCLE_WAIT(4);

	if ((status & 0b10110001) != 0b10000001) // detect errors
		return -1;

	data = hardware_read(0, 0xe001); // get byte
	CYCLE_WAIT(4);
	
	return data;
}

static uint16_t NRWW_SECTION(".nrww_misc") updateCRC(uint16_t crc,uint8_t data)
{
	return _crc_xmodem_update(crc, data);
}

#define UPDATER_GET_BYTE { b = getMidiByte(); if (b < 0) break; }
#define UPDATER_WAIT_BYTE(waited) { UPDATER_GET_BYTE; if(b!=(waited)) break; }
#define UPDATER_CRC_BYTE { UPDATER_GET_BYTE; crc=updateCRC(crc,b); }

void NRWW_SECTION(".updater") updater_main(void)
{
	int8_t i,needUpdate,success=0;
	uint8_t seg=1;
	int16_t b;
	uint16_t crc,pageIdx,pageSize,crcSent,byteIdx;
	uint8_t awaiting[4];
	static uint8_t page[SPM_PAGESIZE];
	
	// power supply still ramping up voltage	
	
	_delay_ms(100); // actual delay 800 ms

	// no interrupts while we update
	
	cli();
	
	// initialize low level

	hardware_init(0);
	
	// check if we need to go to update mode ("from tape" & "to tape" pressed)
	
	hardware_write(1,CSO0,0x01);
	CYCLE_WAIT(10);
	needUpdate = (hardware_read(1,CSI1)&0xc0)==0xc0;
	CYCLE_WAIT(10);
	needUpdate &= (hardware_read(1,CSI1)&0xc0)==0xc0;
	CYCLE_WAIT(10);
	
	if(!needUpdate)
		return;
	
	// select left 7seg 
	
	hardware_write(1,CSO0,0x20);
	CYCLE_WAIT(8);
	
	// show 'U'
	
	hardware_write(1,CSO1,0x3e);
	CYCLE_WAIT(8);
	
	// unclear NMI (UART IRQ)
	
	hardware_write(1,CS06,0b00110001);
	CYCLE_WAIT(8);

	// init 6850
	
	hardware_write(0,0x6000,0b00000011); // master reset
	MDELAY(1);
	
	hardware_write(0,0x6000,0b10010101); // clock/16 - 8N1 - receive int
	CYCLE_WAIT(8);
	
	hardware_read(0,0xe000); // read status & data to start the device
	CYCLE_WAIT(4);
	hardware_read(0,0xe001);
	CYCLE_WAIT(4);

	// main loop
	
	for(;;)
	{
		// init
		crc=0;
		
		// wait for sysex begin
		UPDATER_GET_BYTE;
		if (b != 0xf0) // "F0" is the SysEx start byte, so loop until that arrives
			continue;
		
		// check for my ID
		UPDATER_WAIT_BYTE(SYSEX_ID_0)
		UPDATER_WAIT_BYTE(SYSEX_ID_1)
		UPDATER_WAIT_BYTE(SYSEX_ID_2) 
		
		// check for update command
		UPDATER_WAIT_BYTE(SYSEX_COMMAND_UPDATE_FW) 
		
		// get page size, 0 indicates end of transmission
		UPDATER_CRC_BYTE
		pageSize = (b & 0x7f) << 7;
		UPDATER_CRC_BYTE
		pageSize |= b & 0x7f;
		
		if(pageSize!=SPM_PAGESIZE) // the last SysEx block comes with pagesize =0 and marks the end of the firmware stream
		{
			success=pageSize==0;
			break;
		}
		
		// get page ID
		UPDATER_CRC_BYTE
		pageIdx= (b & 0x7f) << 7;
		UPDATER_CRC_BYTE
		pageIdx |= b & 0x7f;
		
		// get data
		byteIdx = 0;
		for(i = 0; i < SPM_PAGESIZE/sizeof(awaiting); ++i)
		{
			// get low 7bits of 4 bytes
			UPDATER_CRC_BYTE
			awaiting[0] = b & 0x7f;
			UPDATER_CRC_BYTE
			awaiting[1] = b & 0x7f;
			UPDATER_CRC_BYTE
			awaiting[2] = b & 0x7f;
			UPDATER_CRC_BYTE
			awaiting[3] = b & 0x7f;

			// msbs of 4 bytes
			UPDATER_CRC_BYTE
			awaiting[0] |= (b & 1) << 7;
			awaiting[1] |= (b & 2) << 6;
			awaiting[2] |= (b & 4) << 5;
			awaiting[3] |= (b & 8) << 4;

			// copy to page
			page[byteIdx++] = awaiting[0];
			page[byteIdx++] = awaiting[1];
			page[byteIdx++] = awaiting[2];
			page[byteIdx++] = awaiting[3];
		}
		
		// check crc
		UPDATER_GET_BYTE
		crcSent = (b & 0x7f) << 9;
		UPDATER_GET_BYTE
		crcSent |= (b & 0x7f) << 2;
		UPDATER_GET_BYTE
		crcSent |= b & 0x03;
		
		if(crcSent != crc)
			break;
		
		// sysex termination
		UPDATER_WAIT_BYTE(0xf7)
		
		// spinning 7seg
		
		seg <<= 1;
		if(seg >= 0x40)
			seg=1;
		
		hardware_write(1,CSO1,seg);
		CYCLE_WAIT(8);

		// program page
//		blHack_program_page(pageIdx*STORAGE_PAGE_SIZE,page);
		boot_program_page(pageIdx*STORAGE_PAGE_SIZE,page);
	}
	
	// show 'S' or 'E' on the 7seg, depending on success or error
	hardware_write(1,CSO1,(success)?0x6d:0x79);

	// loop infinitely
	for(;;);
}

// override __init function to call updater, so that we can check if we need to go to update mode.
// if we don't, go back to the regular init process
// this function is in the vectors section to ensure it stays in the first SPM_PAGESIZE block of flash
void __attribute__((section(".vectors"),naked,used)) __init(void)
{
	asm volatile
	(
		// init status reg
		"clr r1 \n\t"
		"out 0x3f,r1 \n\t" // SREG
		// init stack
		"ldi r28,lo8(__stack) \n\t"
		"ldi r29,hi8(__stack) \n\t"
		"out 0x3e,r29 \n\t" // SPH
		"out 0x3d,r28 \n\t" // SPL
		// call updater
		"call 0x1e000 \n\t"
		// back to regular init
		"jmp __do_clear_bss \n\t"
	);
}

int main(void)
{
	CPU_PRESCALE(CPU_16MHz);  

#ifdef DEBUG
	// initialize the USB, and then wait for the host
	// to set configuration.  If the Teensy is powered
	// without a PC connected to the USB port, this 
	// will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;

	// wait an extra second for the PC's operating system
	// to load drivers and do whatever it does to actually
	// be ready for input
	_delay_ms(1000);

	print("p600firmware\n");
#endif

	// no interrupts while we init
	
	cli();
	
	// initialize low level

	hardware_init(1);

 // Enable NMI FlipFlop
	io_write(CS06, 0b00110001);

 // Just to be sure
//	SPI_test(); 
//	SPI_test_page();
	
	// initialize synth code

	synth_init();
	
	// all inited, enable ints and do periodical updates
	
	sei();
	
	for(;;)
	{
		synth_update();
	}
}

ISR(TIMER0_COMPA_vect)
{
	// use nested interrupts, because we must still handle synth_uartInterrupt
	// we need to ensure we won't try to recursively handle another synth_timerInterrupt!
	
	TIMSK0 &= ~(1 << OCIE0A); //Disable overflow interrupt for Timer0
	TIFR0 |= 7; // Clear any pending interrupt
	sei();

	synth_timerInterrupt();

	cli();
	TIMSK0 |= (1 << OCIE0A); //Re-enable overflow interrupt for Timer0
}

#ifdef UART_USE_HW_INTERRUPT
ISR(INT4_vect) 
#else
ISR(TIMER2_COMPA_vect) 
#endif
{ 
	synth_uartInterrupt();
}

