/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2012 CaH4e3
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
 * FFE Copier Mappers
 *
 */

#include "mapinc.h"

static uint8 preg[4], creg[8], latch, ffemode;
static uint8 IRQa, mirr;
static int32 IRQCount, IRQLatch;
static uint8 *WRAM = NULL;

#ifndef WRAM_SIZE
#define WRAM_SIZE 8192
#endif

static SFORMAT StateRegs[] =
{
	{ preg, 4, "PREG" },
	{ creg, 8, "CREG" },
	{ &mirr, 1, "MIRR" },
	{ &IRQa, 1, "IRQA" },
	{ &IRQCount, 4, "IRQC" },
	{ &IRQLatch, 4, "IRQL" },
	{ 0 }
};

static void Sync(void) {
	setprg8r(0x10, 0x6000, 0);
	if (ffemode) {
		int i;
		for (i = 0; i < 8; i++) setchr1(i << 10, creg[i]);
		setprg8(0x8000, preg[0]);
		setprg8(0xA000, preg[1]);
		setprg8(0xC000, preg[2]);
		setprg8(0xE000, preg[3]);
	} else {
		setchr8(latch & 3);
		setprg16(0x8000, (latch >> 2) & 0x3F);
		setprg16(0xc000, 0x7);
	}
	switch (mirr) {
	case 0: setmirror(MI_0); break;
	case 1: setmirror(MI_1); break;
	case 2: setmirror(MI_V); break;
	case 3: setmirror(MI_H); break;
	}
}

static void FFEWriteMirr(uint32 A, uint8 V) {
	mirr = ((A << 1) & 2) | ((V >> 4) & 1);
	Sync();
}

static void FFEWriteIRQ(uint32 A, uint8 V) {
	switch (A) {
	case 0x4501: IRQa = 0; X6502_IRQEnd(FCEU_IQEXT); break;
	case 0x4502: IRQCount &= 0xFF00; IRQCount |= V; X6502_IRQEnd(FCEU_IQEXT); break;
	case 0x4503: IRQCount &= 0x00FF; IRQCount |= V << 8; IRQa = 1; X6502_IRQEnd(FCEU_IQEXT); break;
	}
}

static void FFEWritePrg(uint32 A, uint8 V) {
	preg[A & 3] = V;
	Sync();
}

static void FFEWriteChr(uint32 A, uint8 V) {
	creg[A & 7] = V;
	Sync();
}

static void FFEWriteLatch(uint32 A, uint8 V) {
	latch = V;
	Sync();
}

static void FFEPower(void) {
	preg[3] = ~0;
	Sync();
	SetReadHandler(0x8000, 0xFFFF, CartBR);
	SetWriteHandler(0x42FE, 0x42FF, FFEWriteMirr);
	SetWriteHandler(0x4500, 0x4503, FFEWriteIRQ);
	SetWriteHandler(0x4504, 0x4507, FFEWritePrg);
	SetWriteHandler(0x4510, 0x4517, FFEWriteChr);
	SetWriteHandler(0x4510, 0x4517, FFEWriteChr);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);
	SetReadHandler(0x6000, 0x7FFF, CartBR);
	SetWriteHandler(0x8000, 0xFFFF, FFEWriteLatch);
	FCEU_CheatAddRAM(WRAM_SIZE >> 10, 0x6000, WRAM);
}

static void FFEIRQHook(int a) {
	if (IRQa) {
		IRQCount += a;
		if (IRQCount >= 0x10000) {
			X6502_IRQBegin(FCEU_IQEXT);
			IRQa = 0;
			IRQCount = 0;
		}
	}
}

static void FFEClose(void) {
	if (WRAM)
		FCEU_gfree(WRAM);
	WRAM = NULL;
}

static void StateRestore(int version) {
	Sync();
}

void Mapper6_Init(CartInfo *info) {
	ffemode = 0;
	mirr = ((info->mirror & 1) ^ 1) | 2;

	info->Power = FFEPower;
	info->Close = FFEClose;
	MapIRQHook = FFEIRQHook;
	GameStateRestore = StateRestore;

	WRAM = (uint8*)FCEU_gmalloc(WRAM_SIZE);
	SetupCartPRGMapping(0x10, WRAM, WRAM_SIZE, 1);
	AddExState(WRAM, WRAM_SIZE, 0, "WRAM");
	if (info->battery) {
		info->SaveGame[0] = WRAM;
		info->SaveGameLen[0] = WRAM_SIZE;
	}

	AddExState(&StateRegs, ~0, 0, 0);
}

void Mapper17_Init(CartInfo *info) {
	ffemode = 1;
	Mapper6_Init(info);
}
