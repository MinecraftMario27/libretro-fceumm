/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2009 CaH4e3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * All regs access only by READ
 *
 * 5000 - I/O - when read, 0 goes to tape output
 *  ---m3210
 *  3210 - lower scancode nibble
 *  m         - tape input bit
 * 5001 - I
 *  ----7654
 *  7654 - higher scancode nibble
 * 5002 - I   - reset scancode buffer, ready to new input
 * 5004 - O   - when read, 1 goes to tape output
 *
 */

#include "mapinc.h"

static uint8 *WRAM = NULL;

#ifndef WRAM_SIZE
#define WRAM_SIZE 8192
#endif

char *GetKeyboard(void); /* forward declaration */

static char *TransformerKeys, oldkeys[256];
static int TransformerCycleCount, TransformerChar = 0;

static void TransformerIRQHook(int a) {
	TransformerCycleCount += a;
	if (TransformerCycleCount >= 1000) {
		uint32 i;
		TransformerCycleCount -= 1000;
		TransformerKeys = GetKeyboard();

		for (i = 0; i < 256; i++) {
			if (oldkeys[i] != TransformerKeys[i]) {
				if (oldkeys[i] == 0)
					TransformerChar = i;
				else
					TransformerChar = i | 0x80;
				X6502_IRQBegin(FCEU_IQEXT);
				memcpy((void*)&oldkeys[0], (void*)TransformerKeys, sizeof(oldkeys));
				break;
			}
		}
	}
}

static uint8 TransformerRead(uint32 A) {
	uint8 ret = 0;
	switch (A & 3) {
	case 0: ret = TransformerChar & 15; break;
	case 1: ret = (TransformerChar >> 4); break;
	case 2: X6502_IRQEnd(FCEU_IQEXT); break;
	case 4: break;
	}
	return ret;
}

static void TransformerPower(void) {
	setprg8r(0x10, 0x6000, 0);
	setprg16(0x8000, 0);
	setprg16(0xC000, ~0);
	setchr8(0);

	SetReadHandler(0x5000, 0x5004, TransformerRead);
	SetReadHandler(0x6000, 0x7FFF, CartBR);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);
	SetReadHandler(0x8000, 0xFFFF, CartBR);
	FCEU_CheatAddRAM(WRAM_SIZE >> 10, 0x6000, WRAM);

	MapIRQHook = TransformerIRQHook;
}

static void TransformerClose(void) {
	if (WRAM)
		FCEU_gfree(WRAM);
	WRAM = NULL;
}

void Transformer_Init(CartInfo *info) {
	info->Power = TransformerPower;
	info->Close = TransformerClose;
	WRAM = (uint8*)FCEU_gmalloc(WRAM_SIZE);
	SetupCartPRGMapping(0x10, WRAM, WRAM_SIZE, 1);
	if (info->battery) {
		info->SaveGame[0] = WRAM;
		info->SaveGameLen[0] = WRAM_SIZE;
	}
	AddExState(WRAM, WRAM_SIZE, 0, "WRAM");
}
