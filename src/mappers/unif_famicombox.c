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
 */

#include "mapinc.h"

static uint8 regs[8];
static uint8 *WRAM = NULL;

#ifndef WRAM_SIZE
#define WRAM_SIZE 16384
#endif

static SFORMAT StateRegs[] =
{
	{ regs, 8, "REGS" },
	{ 0 }
};

static void Sync(void) {
	setprg2r(0x10, 0x0800, 0);
	setprg2r(0x10, 0x1000, 1);
	setprg2r(0x10, 0x1800, 2);
	setprg8r(0x10, 0x6000, 1);
	setprg16(0x8000, 0);
	setprg16(0xC000, ~0);
	setchr8(0);
}

static void SSSNROMWrite(uint32 A, uint8 V) { }

static uint8 SSSNROMRead(uint32 A) {
	switch (A & 7) {
	case 0: return regs[0] = 0xff; /* clear all exceptions */
	case 2: return 0xc0;	/* DIP selftest + freeplay */
	case 7: return 0x22;	/* TV type, key not turned, relay B */
	case 3: 		/* 0, 1 - attract 
				 * 2
				 * 4    - menu
				 * 8    - self check and game casette check
				 * 10   - lock?
				 * 20   - game title & count display
				 */
	default: break;
	}
	return 0;
}

static void SSSNROMPower(void) {
	regs[0] = regs[1] = regs[2] = regs[3] = regs[4] = regs[5] = regs[6] = 0;
	regs[7] = 0xff;
	Sync();
	SetReadHandler(0x0800, 0x1FFF, CartBR);
	SetWriteHandler(0x0800, 0x1FFF, CartBW);
	SetReadHandler(0x5000, 0x5FFF, SSSNROMRead);
	SetWriteHandler(0x5000, 0x5FFF, SSSNROMWrite);
	SetReadHandler(0x6000, 0x7FFF, CartBR);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);
	SetReadHandler(0x8000, 0xFFFF, CartBR);
	FCEU_CheatAddRAM(WRAM_SIZE >> 10, 0x6000, WRAM);
}

static void SSSNROMReset(void) {
	regs[1] = regs[2] = regs[3] = regs[4] = regs[5] = regs[6] = 0;
}

static void SSSNROMClose(void) {
	if (WRAM)
		FCEU_gfree(WRAM);
	WRAM = NULL;
}

static void SSSNROMIRQHook(void) { }
static void StateRestore(int version) { Sync(); }

void SSSNROM_Init(CartInfo *info) {
	info->Reset = SSSNROMReset;
	info->Power = SSSNROMPower;
	info->Close = SSSNROMClose;
	GameHBIRQHook = SSSNROMIRQHook;
	GameStateRestore = StateRestore;

	WRAM = (uint8*)FCEU_gmalloc(WRAM_SIZE);
	SetupCartPRGMapping(0x10, WRAM, WRAM_SIZE, 1);
	AddExState(WRAM, WRAM_SIZE, 0, "WRAM");
	AddExState(&StateRegs, ~0, 0, 0);
}
