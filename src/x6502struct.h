#ifndef _X6502STRUCTH
#define _X6502STRUCTH

typedef struct __X6502 {
	int32 tcount;		/* Temporary cycle counter */
	uint16 PC;			/* I'll change this to uint32 later... */
						/* I'll need to AND PC after increments to 0xFFFF */
						/* when I do, though.  Perhaps an IPC() macro? */
	uint8 A, X, Y, S, P, mooPI;
	uint8 jammed;

	int32 count;
	uint32 IRQlow;		/* Simulated IRQ pin held low(or is it high?).
						And other junk hooked on for speed reasons.*/
	uint8 openbus;		/* Data bus "cache" for reads from certain areas */

	int preexec;		/* Pre-exec'ing for debug breakpoints. */
} X6502;

#endif
