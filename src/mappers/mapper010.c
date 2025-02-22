/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2012 CaH4e3
 *  Copyright (C) 2002 Xodnizel
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
 */

#include "mapinc.h"

static uint8 creg[4], latch0, latch1, preg, mirr;
static uint8 *WRAM = NULL;

#ifndef WRAM_SIZE
#define WRAM_SIZE 8192
#endif

static SFORMAT StateRegs[] =
{
	{ creg, 4, "CREG" },
	{ &preg, 1, "PREG" },
	{ &mirr, 1, "MIRR" },
	{ &latch0, 1, "LAT0" },
	{ &latch1, 1, "LAT1" },
	{ 0 }
};

static void Sync(void) {
	setprg16(0x8000, preg);
	setprg16(0xC000, ~0);
	setprg8r(0x10, 0x6000, 0);
	setchr4(0x0000, creg[latch0]);
	setchr4(0x1000, creg[latch1 + 2]);
	setmirror(mirr);
}

static void MMC2and4Write(uint32 A, uint8 V) {
	switch (A & 0xF000) {
	case 0xA000: preg = V & 0xF; Sync(); break;
	case 0xB000: creg[0] = V & 0x1F; Sync(); break;
	case 0xC000: creg[1] = V & 0x1F; Sync(); break;
	case 0xD000: creg[2] = V & 0x1F; Sync(); break;
	case 0xE000: creg[3] = V & 0x1F; Sync(); break;
	case 0xF000: mirr = (V & 1) ^ 1; Sync(); break;
	}
}

static void MMC2and4PPUHook(uint32 A) {
	uint8 l, h = A >> 8;
	if (h >= 0x20 || ((h & 0xF) != 0xF))
		return;
	l = A & 0xF0;
	if (h < 0x10) {
		if (l == 0xD0) {
			latch0 = 0;
			setchr4(0x0000, creg[0]);
		} else if (l == 0xE0) {
			latch0 = 1;
			setchr4(0x0000, creg[1]);
		}
	} else {
		if (l == 0xD0) {
			latch1 = 0;
			setchr4(0x1000, creg[2]);
		} else if (l == 0xE0) {
			latch1 = 1;
			setchr4(0x1000, creg[3]);
		}
	}
}

static void MMC2and4Power(void) {
	preg = 0;
	latch0 = latch1 = 1;
	Sync();
	SetReadHandler(0x6000, 0x7FFF, CartBR);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);
	FCEU_CheatAddRAM(WRAM_SIZE >> 10, 0x6000, WRAM);
	SetReadHandler(0x8000, 0xFFFF, CartBR);
	SetWriteHandler(0xA000, 0xFFFF, MMC2and4Write);
}

static void StateRestore(int version) {
	Sync();
}

static void MMC2and4Close(void) {
	if (WRAM)
		FCEU_gfree(WRAM);
	WRAM = NULL;
}

void Mapper10_Init(CartInfo *info) {
	info->Power = MMC2and4Power;
	info->Close = MMC2and4Close;
	PPU_hook = MMC2and4PPUHook;
	WRAM = (uint8*)FCEU_gmalloc(WRAM_SIZE);
	SetupCartPRGMapping(0x10, WRAM, WRAM_SIZE, 1);
	AddExState(WRAM, WRAM_SIZE, 0, "WRAM");
	if (info->battery) {
		info->SaveGame[0] = WRAM;
		info->SaveGameLen[0] = WRAM_SIZE;
	}
	GameStateRestore = StateRestore;
	AddExState(&StateRegs, ~0, 0, 0);
}
