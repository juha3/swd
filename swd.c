#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "swd.h"

/*
	links and docs:
	https://github.com/mgottschlag/rpi-swd/blob/master/MemoryAccessPort.cpp
	ARM Debug Interface Architecture Specification ADv5.0 to ADIv5.2


	LSB first
	even parity (number of bits is even => parity = 0)
	target samples on rising edge
	rarget outputs on rising edge
	host outputs on falling edge
	host samples on falling edge
	turnaround: high-Z for one clock
	

	ACK[2:0] 
	wait  010
	fault 100
	ok    001

	data on wire (DPIDR)

	start bit   1
	APnDP       0
	RnW         1
	A[2:3]      11
	parity      1
	stop        0
	park        1
	turnaround  Z
	turnaround  Z
	turnaround  Z
	turnaround  Z
	turnaround  Z
	data[0:32]  


	DP / MEM_AP


*/

enum {
	INPUT = 0,
	OUTPUT
};

enum {
	DP = 0,
	MEM_AP
};

#define DP_DPIDR      0
#define DP_ABORT      0
#define DP_CTRL_STAT  1
#define DP_SELECT     2
#define DP_RDBUFF     3

/* global variables */

static uint32_t current_ap = 0;
static uint32_t current_bank = 0;
static uint32_t apsel = 0;

/* gpio bit bang interface */

void set_swdio_dir(int dir)
{
	/* TODO: add pin direction change code here */
	printf("SWDIO dir %s\n", dir ? "OUTPUT" : "INPUT");
}

int32_t read_swdio()
{
	/* TODO: add pin state read code here */

}

void set_swdio(int state)
{
	/* TODO: add pin state setting code here */
	printf("SWDIO %d\n", state);
}

void set_swclk(int state)
{
	/* TODO: add pin state setting code here */
	printf("SWCLK %d\n", state);
}

void delay(int t)
{
	/* TODO: add time delay code here */
	printf("delay %d\n", t);
}

void cycle_bus(uint8_t *write_data, int len, uint8_t *read_data)
{
	int i;
	int val = 0;

	if (read_data) memset(read_data, 0, len / 8);
	for (i = 0; i < len; i++) {
		if (write_data)
			val = (write_data[i / 8] & (1 << (i % 8))) ? 1 : 0;
		set_swdio(val);
		delay(1);
		set_swclk(1);
		delay(1);
		set_swclk(0);
		val = read_swdio();
		if (read_data)
			read_data[i / 8] |= (val << (i % 8));
			
	}
}




/* swd functions */

void swd_write(uint32_t ap, uint32_t reg, uint32_t data)
{
	uint8_t cmd = 0;
	uint8_t parity;
	int32_t read = 0;
	uint8_t ack;
	uint8_t p[4];
	int i;

	if (ap) ap = 1;
	else ap = 0;

	cmd |= 1; /* start bit */
	cmd |= (ap << 1);
	cmd |= (read << 2); /* write op */
	cmd |= (reg << 3);

	parity = ap + read + (reg & 0x1) + (reg & 0x2) ? 1 : 0;
	parity = (parity & 0x1) ? 0 : 1;
	cmd |= (parity << 5);
	cmd |= (0x3 << 6); /* stop + park */

	set_swdio_dir(OUTPUT);
	cycle_bus(&cmd, 8, NULL);
	set_swdio_dir(INPUT);
	cycle_bus(NULL, 1, NULL); /* turnaround */
	cycle_bus(NULL, 3, &ack); /* read ack bits */

	//TODO: check ACK bits

	cycle_bus(NULL, 1, NULL); /* turnaround */

	p[0] = data & 0xff;
	p[1] = (data >> 8) & 0xff;
	p[2] = (data >> 16) & 0xff;
	p[3] = (data >> 24) & 0xff;
	set_swdio_dir(OUTPUT);
	cycle_bus(p + 0, 8, NULL);
	cycle_bus(p + 1, 8, NULL);
	cycle_bus(p + 2, 8, NULL);
	cycle_bus(p + 3, 8, NULL);

	parity = 0;
	for (i = 0; i < 32; i++)
		parity += (data & (1 << i)) ? 1 : 0;
	parity = (parity & 0x01) ? 0 : 1;
	cycle_bus(&parity, 1, NULL);

}

uint32_t swd_read(uint32_t ap, uint32_t reg)
{
	uint8_t cmd = 0;
	uint8_t parity, p2;
	int32_t read = 1;
	uint8_t ack;
	uint8_t p[4];
	int i;
	uint32_t data = 0;

	if (ap) ap = 1;
	else ap = 0;

	cmd |= 1; /* start bit */
	cmd |= (ap << 1);
	cmd |= (read << 2); /* read op */
	cmd |= (reg << 3);

	parity = ap + read + (reg & 0x1) + (reg & 0x2) ? 1 : 0;
	parity = (parity & 0x1) ? 0 : 1;
	cmd |= (parity << 5);
	cmd |= (0x3 << 6); /* stop + park */

	set_swdio_dir(OUTPUT);
	cycle_bus(&cmd, 8, NULL);
	set_swdio_dir(INPUT);
	cycle_bus(NULL, 1, NULL); /* turnaround */
	cycle_bus(NULL, 3, &ack); /* read ack bits */

	//TODO: check ACK bits

	cycle_bus(NULL, 1, NULL); /* turnaround */

	cycle_bus(NULL, 8, p + 0);
	cycle_bus(NULL, 8, p + 1);
	cycle_bus(NULL, 8, p + 2);
	cycle_bus(NULL, 8, p + 3);
	cycle_bus(NULL, 1, &p2);

	data = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);

	parity = 0;
	for (i = 0; i < 32; i++)
		parity += (data & (1 << i)) ? 1 : 0;
	parity = (parity & 0x01) ? 0 : 1;
	if (p2 != parity) {
		return 0;
	}

	return data;
		
}

void swd_resync()
{
	/* 
		1. hold data HIGH for 50 cycles.
		2. 2 idle cycles (data LOW)
		3. read IDCODE (DP DPIDR read)
	*/

	int i;
	uint8_t ones = 0xff, zeros = 0;
	uint32_t val;
	int retval;

	for (i = 0; i < 7; i++)
		cycle_bus(&ones, 8, NULL);
	
	cycle_bus(&zeros, 2, NULL);
	
	val = swd_read(DP, DP_DPIDR);
	if (val != 0x0bb11477) printf("unknown IDCODE.\n");

	current_ap = 0;
	current_bank = 0;
}

/* DP functions */

uint32_t get_idcode()
{
	uint32_t val;
	
	val = swd_read(DP, DP_DPIDR);
	return val;
}


uint32_t get_status()
{
	uint32_t val;
	
	val = swd_read(DP, DP_CTRL_STAT);
	return val;
}

uint32_t read_rb()
{
	uint32_t val;
	
	val = swd_read(DP, DP_RDBUFF);
	return val;
}

void dp_select(uint32_t apsel, uint32_t apbank)
{
	if (apsel == current_ap && apbank == current_bank)
		return;

	swd_write(DP, DP_SELECT, ((apsel & 0xff) << 24) ||
		((apbank & 0xf) << 4));
	current_ap = apsel;
	current_bank = apbank;
}

uint32_t read_ap(uint32_t apsel, uint32_t addr)
{
	uint32_t bank = addr >> 4;
	uint32_t reg = (addr >> 2) & 0x3;
	dp_select(apsel, bank);
	return swd_read(MEM_AP, reg);
}

void write_ap(uint32_t apsel, uint32_t addr, uint32_t val)
{
	uint32_t bank = addr >> 4;
	uint32_t reg = (addr >> 2) & 0x3;
	dp_select(apsel, bank);
	swd_write(MEM_AP, reg, val);
}

/* MEM_AP functions */

uint32_t write_word(uint32_t addr, uint32_t data)
{
	write_ap(apsel, 0x04, addr);
	write_ap(apsel, 0x0c, data);
	
	return read_rb();
}

uint32_t read_word(uint32_t addr)
{
	write_ap(apsel, 0x04, addr);
	read_ap(apsel, 0x00);
	
	return read_rb();
}

uint32_t mem_ap_get_idcode()
{
	read_ap(apsel, 0xfc);
	return read_rb();
}

void write_block(uint32_t addr, int len, uint32_t *buffer) {

	int i;

	write_ap(apsel, 0x04, addr);
	read_ap(apsel, 0x0c);

	for (i = 0; i < len; i++) {
		write_ap(apsel, 0x0c, buffer[i]);
	}	
}

void read_block(uint32_t addr, int len, uint32_t *buffer) {

	int i;

	write_ap(apsel, 0x04, addr);
	read_ap(apsel, 0x0c);

	for (i = 0; i < len - 1; i++) {
		buffer[i] = read_ap(apsel, 0x0c);
	}	
	buffer[i] = read_rb();
}

void set_csw(uint32_t addr_inc, uint32_t size)
{
	uint32_t csw;

	read_ap(apsel, 0x00);
	csw = read_rb() & 0xffffff00;
	write_ap(apsel, 0x00, csw + (addr_inc << 4) + size);
	
}

int halt()
{
	while(1) {
		write_word(0xe000edf0, 0xa05f0001);
		if (read_word(0xe000edf0) & (1 << 0)) {
			break;
		}
	}
	while(1) {
		write_word(0xe000edf0, 0xa05f0003);
		if (read_word(0xe000edf0) & (1 << 1)) {
			break;
		}
	}
	if (read_word(0xe000edf0) & (1 << 17)) return 0;
	return -1;
}

void cont()
{
	write_word(0xe000edf0, 0xa05f0000);
}

void reset()
{
	write_word(0xe000ed0c, 0xa05f0004);
	while(1) {
		if ((read_word(0xe000edf0) & (1 << 25)) == 0) break;
	}
}

void reset_and_halt()
{
	uint32_t val;

	write_word(0xe000edf0, 0xa05f0001);
	write_word(0xe000edf0, 0xa05f0003);
	val = read_word(0xe000edfc);
	write_word(0xe000edfc, val | (1 << 0));

	write_word(0xe000ed0c, 0xa05f0004);
	while(1) {
		if ((read_word(0xe000edf0) & (1 << 25)) == 0) break;
	}

	
	while(1) {
		if (read_word(0xe000edf0) & (1 << 17)) {
			break;
		}
	}
	
}


