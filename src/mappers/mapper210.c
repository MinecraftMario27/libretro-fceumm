/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
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
 */

#include "mapinc.h"

static uint16 IRQCount;
static uint8 IRQa;

static uint8 WRAM[8192];
static uint8 IRAM[128];

static uint8 AWRAM(uint32 A) { return(WRAM[A - 0x6000]); }
static void BWRAM(uint32 A, uint8 V) { WRAM[A - 0x6000] = V; }

void Mapper19_ESI(void);

static uint8 NTAPage[4];

static uint8 dopol;
static uint8 gorfus;
static uint8 gorko;

static void NamcoSound(int Count);
static void NamcoSoundHack(void);
static void DoNamcoSound(int32 *Wave, int Count);
static void DoNamcoSoundHQ(void);
static void SyncHQ(int32 ts);

static int is210;	/* Lesser mapper. */

static uint8 PRG[3];
static uint8 CHR[8];

static SFORMAT N106_StateRegs[] = {
	{ PRG, 3, "PRG" },
	{ CHR, 8, "CHR" },
	{ NTAPage, 4, "NTA" },
	{ &IRQCount, 2 | FCEUSTATE_RLSB, "IRQC" },
	{ &IRQa, 1, "IRQA" },
	{ &dopol, 1, "GORF" },
	{ &gorfus, 1, "DOPO" },
	{ &gorko, 1, "GORK" },
	{ 0 }
};

static void SyncPRG(void) {
	setprg8(0x8000, PRG[0]);
	setprg8(0xa000, PRG[1]);
	setprg8(0xc000, PRG[2]);
	setprg8(0xe000, 0x3F);
}

static void SyncMirror(void) {
	switch(gorko) {
	case 0: setmirror(MI_0); break;
	case 1: setmirror(MI_V); break;
	case 2: setmirror(MI_H); break;
	case 3: setmirror(MI_0); break;
	}
}

static void NamcoIRQHook(int a) {
	if (IRQa) {
		IRQCount += a;
		if (IRQCount >= 0x7FFF) {
			X6502_IRQBegin(FCEU_IQEXT);
			IRQa = 0;
			IRQCount = 0x7FFF;
		}
	}
}

static uint8 Namco_Read4800(uint32 A) {
	uint8 ret = IRAM[dopol & 0x7f];
	/* Maybe I should call NamcoSoundHack() here? */
	if (dopol & 0x80)
		dopol = (dopol & 0x80) | ((dopol + 1) & 0x7f);
	return ret;
}

static uint8 Namco_Read5000(uint32 A) { return(IRQCount); }
static uint8 Namco_Read5800(uint32 A) { return(IRQCount >> 8); }

static void DoNTARAMROM(int w, uint8 V) {
	NTAPage[w] = V;
	if (V >= 0xE0)
		setntamem(NTARAM + ((V & 1) << 10), 1, w);
	else {
		V &= CHRmask1[0];
		setntamem(CHRptr[0] + (V << 10), 0, w);
	}
}

static void FixNTAR(void) {
	int x;
	for (x = 0; x < 4; x++)
		DoNTARAMROM(x, NTAPage[x]);
}

static void DoCHRRAMROM(int x, uint8 V) {
	CHR[x] = V;
	if (!is210 && !((gorfus >> ((x >> 2) + 6)) & 1) && (V >= 0xE0)) {
	} else
		setchr1(x << 10, V);
}

static void FixCRR(void) {
	int x;
	for (x = 0; x < 8; x++)
		DoCHRRAMROM(x, CHR[x]);
}

static void Mapper19C0D8_write(uint32 A, uint8 V) {
	DoNTARAMROM((A - 0xC000) >> 11, V);
}

static uint32 FreqCache[8];
static uint32 EnvCache[8];
static uint32 LengthCache[8];

static void FixCache(int a, int V) {
	int w = (a >> 3) & 0x7;
	switch (a & 0x07) {
	case 0x00: FreqCache[w] &= ~0x000000FF; FreqCache[w] |= V; break;
	case 0x02: FreqCache[w] &= ~0x0000FF00; FreqCache[w] |= V << 8; break;
	case 0x04:
		FreqCache[w] &= ~0x00030000; FreqCache[w] |= (V & 3) << 16;
		/* something wrong here http://www.romhacking.net/forum/index.php?topic=21907.msg306903#msg306903 */
		/* LengthCache[w] = (8 - ((V >> 2) & 7)) << 2; */
		/* fix be like in https://github.com/SourMesen/Mesen/blob/cda0a0bdcb5525480784f4b8c71de6fc7273b570/Core/Namco163Audio.h#L61 */
		LengthCache[w] = 256 - (V & 0xFC);
		break;
	case 0x07: EnvCache[w] = (double)(V & 0xF) * 576716; break;
	}
}

static void Mapper19_write(uint32 A, uint8 V) {
	A &= 0xF800;
	if (A >= 0x8000 && A <= 0xb800)
		DoCHRRAMROM((A - 0x8000) >> 11, V);
	else
		switch (A) {
		case 0x4800:
			if (dopol & 0x40) {
				if (FSettings.SndRate) {
					NamcoSoundHack();
					GameExpSound.Fill = NamcoSound;
					GameExpSound.HiFill = DoNamcoSoundHQ;
					GameExpSound.HiSync = SyncHQ;
				}
				FixCache(dopol, V);
			}
			IRAM[dopol & 0x7f] = V;
			if (dopol & 0x80)
				dopol = (dopol & 0x80) | ((dopol + 1) & 0x7f);
			break;
		case 0xf800:
			dopol = V; break;
		case 0x5000:
			IRQCount &= 0xFF00; IRQCount |= V; X6502_IRQEnd(FCEU_IQEXT); break;
		case 0x5800:
			IRQCount &= 0x00ff; IRQCount |= (V & 0x7F) << 8;
			IRQa = V & 0x80;
			X6502_IRQEnd(FCEU_IQEXT);
			break;
		case 0xE000:
			gorko = V & 0xC0;
			PRG[0] = V & 0x3F;
			SyncPRG();
			if (is210) {
				gorko = V >> 6;
				SyncMirror();
			}
			break;
		case 0xE800:
			gorfus = V & 0xC0;
			FixCRR();
			PRG[1] = V & 0x3F;
			SyncPRG();
			break;
		case 0xF000:
			PRG[2] = V & 0x3F;
			SyncPRG();
			break;
		}
}

static int dwave = 0;

static void NamcoSoundHack(void) {
	int32 z, a;
	if (FSettings.soundq >= 1) {
		DoNamcoSoundHQ();
		return;
	}
	z = ((SOUNDTS << 16) / soundtsinc) >> 4;
	a = z - dwave;
	if (a) DoNamcoSound(&Wave[dwave], a);
	dwave += a;
}

static void NamcoSound(int Count) {
	int32 z, a;
	z = ((SOUNDTS << 16) / soundtsinc) >> 4;
	a = z - dwave;
	if (a) DoNamcoSound(&Wave[dwave], a);
	dwave = 0;
}

static uint32 PlayIndex[8];
static int32 vcount[8];
static int32 CVBC;

#define TOINDEX        (16 + 1)

static SFORMAT N106_SStateRegs[] =
{
	{ &PlayIndex[0], 4 | FCEUSTATE_RLSB, "IDX0" },
	{ &PlayIndex[1], 4 | FCEUSTATE_RLSB, "IDX1" },
	{ &PlayIndex[2], 4 | FCEUSTATE_RLSB, "IDX2" },
	{ &PlayIndex[3], 4 | FCEUSTATE_RLSB, "IDX3" },
	{ &PlayIndex[4], 4 | FCEUSTATE_RLSB, "IDX4" },
	{ &PlayIndex[5], 4 | FCEUSTATE_RLSB, "IDX5" },
	{ &PlayIndex[6], 4 | FCEUSTATE_RLSB, "IDX6" },
	{ &PlayIndex[7], 4 | FCEUSTATE_RLSB, "IDX7" },
	{ &vcount[0], 4 | FCEUSTATE_RLSB, "VCT0" },
	{ &vcount[1], 4 | FCEUSTATE_RLSB, "VCT1" },
	{ &vcount[2], 4 | FCEUSTATE_RLSB, "VCT2" },
	{ &vcount[3], 4 | FCEUSTATE_RLSB, "VCT3" },
	{ &vcount[4], 4 | FCEUSTATE_RLSB, "VCT4" },
	{ &vcount[5], 4 | FCEUSTATE_RLSB, "VCT5" },
	{ &vcount[6], 4 | FCEUSTATE_RLSB, "VCT6" },
	{ &vcount[7], 4 | FCEUSTATE_RLSB, "VCT7" },
	{ &CVBC, 4 | FCEUSTATE_RLSB, "BC00" },
	{ 0 }
};

/* 16:15 */
static void SyncHQ(int32 ts) { CVBC = ts; }

/* Things to do:
	1        Read freq low
	2        Read freq mid
	3        Read freq high
	4        Read envelope
	...?
*/

static INLINE uint32 FetchDuff(uint32 P, uint32 envelope) {
	uint32 duff = IRAM[((IRAM[0x46 + (P << 3)] + (PlayIndex[P] >> TOINDEX)) & 0xFF) >> 1];
	if ((IRAM[0x46 + (P << 3)] + (PlayIndex[P] >> TOINDEX)) & 1)
		duff >>= 4;
	duff &= 0xF;
	duff = (duff * envelope) >> 16;
	return(duff);
}

static void DoNamcoSoundHQ(void) {
	int32 P, V;
	int32 cyclesuck = (((IRAM[0x7F] >> 4) & 7) + 1) * 15;

	for (P = 7; P >= (7 - ((IRAM[0x7F] >> 4) & 7)); P--) {
		if ((IRAM[0x44 + (P << 3)] & 0xE0) && (IRAM[0x47 + (P << 3)] & 0xF)) {
			int32 vco   = vcount[P];
			uint32 freq = FreqCache[P];
			uint32 envelope = EnvCache[P];
			uint32 lengo = LengthCache[P];
			uint32 duff2 = FetchDuff(P, envelope);
			for (V = CVBC << 1; V < (int)SOUNDTS << 1; V++) {
				WaveHi[V >> 1] += duff2;
				if (!vco) {
					PlayIndex[P] += freq;
					while ((PlayIndex[P] >> TOINDEX) >= lengo) PlayIndex[P] -= lengo << TOINDEX;
					duff2 = FetchDuff(P, envelope);
					vco = cyclesuck;
				}
				vco--;
			}
			vcount[P] = vco;
		}
	}
	CVBC = SOUNDTS;
}


static void DoNamcoSound(int32 *Wave, int Count) {
	int P, V;
	for (P = 7; P >= 7 - ((IRAM[0x7F] >> 4) & 7); P--) {
		if ((IRAM[0x44 + (P << 3)] & 0xE0) && (IRAM[0x47 + (P << 3)] & 0xF)) {
			int32 inc;
			uint32 duff, duff2;
			int32 vco = vcount[P];
			uint32 freq = FreqCache[P];
			uint32 envelope = EnvCache[P];
			uint32 lengo = LengthCache[P];

			if (!freq)
				continue;

			{
				int c = ((IRAM[0x7F] >> 4) & 7) + 1;
				inc = (long double)(FSettings.SndRate << 15) / ((long double)freq * 21477272 / ((long double)0x400000 * c * 45));
			}

			duff = IRAM[(((IRAM[0x46 + (P << 3)] + PlayIndex[P]) & 0xFF) >> 1)];
			if ((IRAM[0x46 + (P << 3)] + PlayIndex[P]) & 1)
				duff >>= 4;
			duff &= 0xF;
			duff2 = (duff * envelope) >> 19;
			for (V = 0; V < Count * 16; V++) {
				if (vco >= inc) {
					PlayIndex[P]++;
					if (PlayIndex[P] >= lengo)
						PlayIndex[P] = 0;
					vco -= inc;
					duff = IRAM[(((IRAM[0x46 + (P << 3)] + PlayIndex[P]) & 0xFF) >> 1)];
					if ((IRAM[0x46 + (P << 3)] + PlayIndex[P]) & 1)
						duff >>= 4;
					duff &= 0xF;
					duff2 = (duff * envelope) >> 19;
				}
				Wave[V >> 4] += duff2;
				vco += 0x8000;
			}
			vcount[P] = vco;
		}
	}
}

static void Mapper19_StateRestore(int version) {
	int x;
	SyncPRG();
	SyncMirror();
	FixNTAR();
	FixCRR();
	for (x = 0x40; x < 0x80; x++)
		FixCache(x, IRAM[x]);
}

static void M19SC(void) {
	if (FSettings.SndRate)
		Mapper19_ESI();
}

void Mapper19_ESI(void) {
	GameExpSound.RChange = M19SC;
	memset(vcount, 0, sizeof(vcount));
	memset(PlayIndex, 0, sizeof(PlayIndex));
	CVBC = 0;
}

void NSFN106_Init(void) {
	SetWriteHandler(0xf800, 0xffff, Mapper19_write);
	SetWriteHandler(0x4800, 0x4fff, Mapper19_write);
	SetReadHandler(0x4800, 0x4fff, Namco_Read4800);
	Mapper19_ESI();
}

static int battery = 0;

static void N106_Power(void) {
	int x;
	SetReadHandler(0x8000, 0xFFFF, CartBR);
	SetWriteHandler(0x8000, 0xffff, Mapper19_write);
	SetWriteHandler(0x4020, 0x5fff, Mapper19_write);
	if (!is210) {
		SetWriteHandler(0xc000, 0xdfff, Mapper19C0D8_write);
		SetReadHandler(0x4800, 0x4fff, Namco_Read4800);
		SetReadHandler(0x5000, 0x57ff, Namco_Read5000);
		SetReadHandler(0x5800, 0x5fff, Namco_Read5800);
		NTAPage[0] = NTAPage[1] = NTAPage[2] = NTAPage[3] = 0xFF;
		FixNTAR();
	}

	SetReadHandler(0x6000, 0x7FFF, AWRAM);
	SetWriteHandler(0x6000, 0x7FFF, BWRAM);
	FCEU_CheatAddRAM(8, 0x6000, WRAM);

	gorfus = 0xFF;
	SyncPRG();
	FixCRR();

	if (!battery) {
		FCEU_dwmemset(WRAM, 0, 8192);
		FCEU_dwmemset(IRAM, 0, 128);
	}
	for (x = 0x40; x < 0x80; x++)
		FixCache(x, IRAM[x]);
}

void Mapper19_Init(CartInfo *info) {
	is210 = 0;
	battery = info->battery;
	info->Power = N106_Power;

	MapIRQHook = NamcoIRQHook;
	GameStateRestore = Mapper19_StateRestore;
	GameExpSound.RChange = M19SC;

	if (FSettings.SndRate)
		Mapper19_ESI();

	AddExState(WRAM, 8192, 0, "WRAM");
	AddExState(IRAM, 128, 0, "IRAM");
	AddExState(N106_StateRegs, ~0, 0, 0);
	AddExState(N106_SStateRegs, ~0, 0, 0);

	if (info->battery) {
		info->SaveGame[0] = WRAM;
		info->SaveGameLen[0] = 8192;
		info->SaveGame[1] = IRAM;
		info->SaveGameLen[1] = 128;
	}
}

static void Mapper210_StateRestore(int version) {
	SyncPRG();
	FixCRR();
}

void Mapper210_Init(CartInfo *info) {
	is210 = 1;
	GameStateRestore = Mapper210_StateRestore;
	info->Power = N106_Power;
	AddExState(WRAM, 8192, 0, "WRAM");
	AddExState(N106_StateRegs, ~0, 0, 0);
}
