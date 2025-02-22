/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2011 CaH4e3
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
 * FDS Conversion
 *
 */

#include "mapinc.h"

static uint8 chr;
static SFORMAT StateRegs[] =
{
	{ &chr, 1, "CHR" },
	{ 0 }
};

static void LE05Sync(void) {
	setprg2r(0, 0xE000, 0);
	setprg2r(0, 0xE800, 0);
	setprg2r(0, 0xF000, 0);
	setprg2r(0, 0xF800, 0);

	setprg8r(1, 0x6000, 3);
	setprg8r(1, 0x8000, 0);
	setprg8r(1, 0xA000, 1);
	setprg8r(1, 0xC000, 2);

	setchr8(chr & 1);
	setmirror(MI_V);
}

static void LE05Write(uint32 A, uint8 V) {
	chr = V;
	LE05Sync();
}

static void LE05Power(void) {
	LE05Sync();
	SetReadHandler(0x6000, 0xFFFF, CartBR);
	SetWriteHandler(0x8000, 0xFFFF, LE05Write);
}

static void LE05StateRestore(int version) {
	LE05Sync();
}

void LE05_Init(CartInfo *info) {
	info->Power = LE05Power;
	GameStateRestore = LE05StateRestore;
	AddExState(&StateRegs, ~0, 0, 0);
}
