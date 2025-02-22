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
 * VRC-6
 *
 */

#include "mapinc.h"

static uint8 prg[2], chr[8], mirr;
static uint8 IRQLatch, IRQa, IRQd;
static int32 IRQCount, CycleCount;
static uint8 *WRAM = NULL;

#ifndef WRAM_SIZE
#define WRAM_SIZE 8192
#endif

static SFORMAT StateRegs[] =
{
	{ prg, 2, "PRG" },
	{ chr, 8, "CHR" },
	{ &mirr, 1, "MIRR" },
	{ &IRQa, 1, "IRQA" },
	{ &IRQd, 1, "IRQD" },
	{ &IRQLatch, 1, "IRQL" },
	{ &IRQCount, 4, "IRQC" },
	{ &CycleCount, 4, "CYCC" },
	{ 0 }
};

static void(*sfun[3]) (void);

static uint8 vpsg1[8];
static uint8 vpsg2[4];
static int32 cvbc[3];
static int32 vcount[3];
static int32 dcount[3];
static int32 phaseacc;

static SFORMAT SStateRegs[] =
{
	{ vpsg1, 8, "PSG1" },
	{ vpsg2, 4, "PSG2" },

/* Ignoring these sound state files for Wii since it causes states unable to load */
#ifndef GEKKO
	/* rw - 2018-11-28 Added */
	{ &cvbc[0], 4 | FCEUSTATE_RLSB, "BC01" },
	{ &cvbc[1], 4 | FCEUSTATE_RLSB, "BC02" },
	{ &cvbc[2], 4 | FCEUSTATE_RLSB, "BC03" },
	{ &dcount[0], 4 | FCEUSTATE_RLSB, "DCT0" },
	{ &dcount[1], 4 | FCEUSTATE_RLSB, "DCT1" },
	{ &dcount[2], 4 | FCEUSTATE_RLSB, "DCT2" },
	{ &vcount[0], 4 | FCEUSTATE_RLSB, "VCT0" },
	{ &vcount[1], 4 | FCEUSTATE_RLSB, "VCT1" },
	{ &vcount[2], 4 | FCEUSTATE_RLSB, "VCT2" },
	{ &phaseacc, 4 | FCEUSTATE_RLSB, "ACCU" },
#endif
	{ 0 }
};

static void Mapper26_VRC6Sync(void) {
	uint8 i;
	setprg8r(0x10, 0x6000, 0);
	setprg16(0x8000, prg[0]);
	setprg8(0xc000, prg[1]);
	setprg8(0xe000, ~0);
	for (i = 0; i < 8; i++)
		setchr1(i << 10, chr[i]);
	switch (mirr & 3) {
	case 0: setmirror(MI_V); break;
	case 1: setmirror(MI_H); break;
	case 2: setmirror(MI_0); break;
	case 3: setmirror(MI_1); break;
	}
}

static void VRC6SW(uint32 A, uint8 V) {
	A &= 0xF003;
	if (A >= 0x9000 && A <= 0x9002) {
		vpsg1[A & 3] = V;
		if (sfun[0]) sfun[0]();
	} else if (A >= 0xA000 && A <= 0xA002) {
		vpsg1[4 | (A & 3)] = V;
		if (sfun[1]) sfun[1]();
	} else if (A >= 0xB000 && A <= 0xB002) {
		vpsg2[A & 3] = V;
		if (sfun[2]) sfun[2]();
	}
}

static void VRC6Write(uint32 A, uint8 V) {
	A = (A & 0xFFFC) | ((A >> 1) & 1) | ((A << 1) & 2);
	if (A >= 0x9000 && A <= 0xB002) {
		VRC6SW(A, V);
		return;
	}
	switch (A & 0xF003) {
	case 0x8000: prg[0] = V; Mapper26_VRC6Sync(); break;
	case 0xB003: mirr = (V >> 2) & 3; Mapper26_VRC6Sync(); break;
	case 0xC000: prg[1] = V; Mapper26_VRC6Sync(); break;
	case 0xD000: chr[0] = V; Mapper26_VRC6Sync(); break;
	case 0xD001: chr[1] = V; Mapper26_VRC6Sync(); break;
	case 0xD002: chr[2] = V; Mapper26_VRC6Sync(); break;
	case 0xD003: chr[3] = V; Mapper26_VRC6Sync(); break;
	case 0xE000: chr[4] = V; Mapper26_VRC6Sync(); break;
	case 0xE001: chr[5] = V; Mapper26_VRC6Sync(); break;
	case 0xE002: chr[6] = V; Mapper26_VRC6Sync(); break;
	case 0xE003: chr[7] = V; Mapper26_VRC6Sync(); break;
	case 0xF000: IRQLatch = V; X6502_IRQEnd(FCEU_IQEXT); break;
	case 0xF001:
		IRQa = V & 2;
		IRQd = V & 1;
		if (V & 2)
			IRQCount = IRQLatch;
		CycleCount = 0;
		X6502_IRQEnd(FCEU_IQEXT);
		break;
	case 0xF002:
		IRQa = IRQd;
		X6502_IRQEnd(FCEU_IQEXT);
	}
}

static void VRC6Power(void) {
	Mapper26_VRC6Sync();
	SetReadHandler(0x6000, 0xFFFF, CartBR);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);
	SetWriteHandler(0x8000, 0xFFFF, VRC6Write);
	FCEU_CheatAddRAM(WRAM_SIZE >> 10, 0x6000, WRAM);
}

static void VRC6IRQHook(int a) {
	if (IRQa) {
		CycleCount += a * 3;
		while (CycleCount >= 341) {
			CycleCount -= 341;
			IRQCount++;
			if (IRQCount == 0x100) {
				IRQCount = IRQLatch;
				X6502_IRQBegin(FCEU_IQEXT);
			}
		}
	}
}

static void VRC6Close(void) {
	if (WRAM)
		FCEU_gfree(WRAM);
	WRAM = NULL;
}

static void VRC6StateRestore(int version) {
	Mapper26_VRC6Sync();
}

/* VRC6 Sound */

static void DoSQV1(void);
static void DoSQV2(void);
static void DoSawV(void);

static INLINE void DoSQV(int x) {
	int32 V;
	int32 amp = (((vpsg1[x << 2] & 15) << 8) * 6 / 8) >> 4;
	int32 start = cvbc[x];
	int32 end = (SOUNDTS << 16) / soundtsinc;
	if (end <= start) return;
	cvbc[x] = end;

	if (vpsg1[(x << 2) | 0x2] & 0x80) {
		if (vpsg1[x << 2] & 0x80) {
			for (V = start; V < end; V++)
				Wave[V >> 4] += amp;
		} else {
			int32 thresh = (vpsg1[x << 2] >> 4) & 7;
			int32 freq = ((vpsg1[(x << 2) | 0x1] | ((vpsg1[(x << 2) | 0x2] & 15) << 8)) + 1) << 17;
			int32 dc = dcount[x];
			int32 vc = vcount[x];

			for (V = start; V < end; V++) {
				if (dc > thresh)
					Wave[V >> 4] += amp;
				vc -= nesincsize;
				while (vc <= 0) {
					vc += freq;
					dc = (dc + 1) & 15;
				}
			}
			vcount[x] = vc;
			dcount[x] = dc;
		}
	}
}

static void DoSQV1(void) {
	DoSQV(0);
}

static void DoSQV2(void) {
	DoSQV(1);
}

static void DoSawV(void) {
	int V;
	int32 start = cvbc[2];
	int32 end = (SOUNDTS << 16) / soundtsinc;
	if (end <= start) return;
	cvbc[2] = end;

	if (vpsg2[2] & 0x80) {
		uint32 freq3;
		static uint32 duff = 0;

		freq3 = (vpsg2[1] + ((vpsg2[2] & 15) << 8) + 1);

		for (V = start; V < end; V++) {
			vcount[2] -= nesincsize;
			if (vcount[2] <= 0) {
				int32 t;
 rea:
				t = freq3;
				t <<= 18;
				vcount[2] += t;
				phaseacc += vpsg2[0] & 0x3f;
				dcount[2]++;
				if (dcount[2] == 7) {
					dcount[2] = 0;
					phaseacc = 0;
				}
				if (vcount[2] <= 0)
					goto rea;
				duff = (((phaseacc >> 3) & 0x1f) << 4) * 6 / 8;
			}
			Wave[V >> 4] += duff;
		}
	}
}

static INLINE void DoSQVHQ(int x) {
	int32 V;
	int32 amp = ((vpsg1[x << 2] & 15) << 8) * 6 / 8;

	if (vpsg1[(x << 2) | 0x2] & 0x80) {
		if (vpsg1[x << 2] & 0x80) {
			for (V = cvbc[x]; V < (int)SOUNDTS; V++)
				WaveHi[V] += amp;
		} else {
			int32 thresh = (vpsg1[x << 2] >> 4) & 7;
			int32 dc = dcount[x];
			int32 vc = vcount[x];

			for (V = cvbc[x]; V < (int)SOUNDTS; V++) {
				if (dc > thresh)
					WaveHi[V] += amp;
				vc--;
				if (vc <= 0) {
					vc = (vpsg1[(x << 2) | 0x1] | ((vpsg1[(x << 2) | 0x2] & 15) << 8)) + 1;
					dc = (dc + 1) & 15;
				}
			}
			dcount[x] = dc;
			vcount[x] = vc;
		}
	}
	cvbc[x] = SOUNDTS;
}

static void DoSQV1HQ(void) {
	DoSQVHQ(0);
}

static void DoSQV2HQ(void) {
	DoSQVHQ(1);
}

static void DoSawVHQ(void) {
	int32 V;

	if (vpsg2[2] & 0x80) {
		for (V = cvbc[2]; V < (int)SOUNDTS; V++) {
			WaveHi[V] += (((phaseacc >> 3) & 0x1f) << 8) * 6 / 8;
			vcount[2]--;
			if (vcount[2] <= 0) {
				vcount[2] = (vpsg2[1] + ((vpsg2[2] & 15) << 8) + 1) << 1;
				phaseacc += vpsg2[0] & 0x3f;
				dcount[2]++;
				if (dcount[2] == 7) {
					dcount[2] = 0;
					phaseacc = 0;
				}
			}
		}
	}
	cvbc[2] = SOUNDTS;
}

void Mapper26_VRC6Sound(int Count) {
	int x;

	DoSQV1();
	DoSQV2();
	DoSawV();
	for (x = 0; x < 3; x++)
		cvbc[x] = Count;
}

void Mapper26_VRC6SoundHQ(void) {
	DoSQV1HQ();
	DoSQV2HQ();
	DoSawVHQ();
}

void Mapper26_VRC6SyncHQ(int32 ts) {
	int x;
	for (x = 0; x < 3; x++) cvbc[x] = ts;
}

static void Mapper26_VRC6_ESI(void) {
	GameExpSound.RChange = Mapper26_VRC6_ESI;
	GameExpSound.Fill = Mapper26_VRC6Sound;
	GameExpSound.HiFill = Mapper26_VRC6SoundHQ;
	GameExpSound.HiSync = Mapper26_VRC6SyncHQ;

	phaseacc = 0;
	memset(cvbc, 0, sizeof(cvbc));
	memset(vcount, 0, sizeof(vcount));
	memset(dcount, 0, sizeof(dcount));
	if (FSettings.SndRate) {
		if (FSettings.soundq >= 1) {
			sfun[0] = DoSQV1HQ;
			sfun[1] = DoSQV2HQ;
			sfun[2] = DoSawVHQ;
		} else {
			sfun[0] = DoSQV1;
			sfun[1] = DoSQV2;
			sfun[2] = DoSawV;
		}
	} else
		memset(sfun, 0, sizeof(sfun));
}

/* VRC6 Sound */

void Mapper26_Init(CartInfo *info) {
	info->Power = VRC6Power;
	info->Close = VRC6Close;
	MapIRQHook = VRC6IRQHook;
	Mapper26_VRC6_ESI();
	GameStateRestore = VRC6StateRestore;

	WRAM = (uint8*)FCEU_gmalloc(WRAM_SIZE);
	SetupCartPRGMapping(0x10, WRAM, WRAM_SIZE, 1);
	AddExState(WRAM, WRAM_SIZE, 0, "WRAM");
	if (info->battery) {
		info->SaveGame[0] = WRAM;
		info->SaveGameLen[0] = WRAM_SIZE;
	}
	AddExState(&StateRegs, ~0, 0, 0);
	AddExState(&SStateRegs, ~0, 0, 0);
}
