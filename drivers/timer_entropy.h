#ifndef ENTROPY_TIMER_H
#define ENTROPY_TIMER_H

#include <stdint.h>

/* Latches and reads counter 0 of the i8254 PIT via the read-back command.
 * Used as a jitter / fine-grained timestamp source for the entropy pool. */
uint16_t timer_read_count(void);

#endif
