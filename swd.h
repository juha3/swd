#include <stdint.h>


/* write 32 bit word to adress */
uint32_t write_word(uint32_t addr, uint32_t data);

/* read 32 bit word from adress */
uint32_t read_word(uint32_t addr);

uint32_t mem_ap_get_idcode();

/* Write block of memory */
void write_block(uint32_t addr, int len, uint32_t *buffer);

/* read block of memory */
void read_block(uint32_t addr, int len, uint32_t *buffer);

/* Set Control/Status Word */
void set_csw(uint32_t addr_inc, uint32_t size);

/* Halt CPU */
int halt();

/* Resume execution */
void cont();

/* trigger debug reset */
void reset();

/* reset CPU and halt */
void reset_and_halt();


