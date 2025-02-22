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

void VRC6Sound(int Count) {
	int x;

	DoSQV1();
	DoSQV2();
	DoSawV();
	for (x = 0; x < 3; x++)
		cvbc[x] = Count;
}

void VRC6SoundHQ(void) {
	DoSQV1HQ();
	DoSQV2HQ();
	DoSawVHQ();
}

void VRC6SyncHQ(int32 ts) {
	int x;
	for (x = 0; x < 3; x++) cvbc[x] = ts;
}

static void VRC6_ESI(void) {
	GameExpSound.RChange = VRC6_ESI;
	GameExpSound.Fill = VRC6Sound;
	GameExpSound.HiFill = VRC6SoundHQ;
	GameExpSound.HiSync = VRC6SyncHQ;

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

void NSFVRC6_Init(void) {
	VRC6_ESI();
	SetWriteHandler(0x8000, 0xbfff, VRC6SW);
	AddExState(&SStateRegs, ~0, 0, 0);
}
