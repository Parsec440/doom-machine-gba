#ifndef GRAFICA_DADI_H
#define GRAFICA_DADI_H

#include "grafica.h"

/* subito dopo i tile delle carte (N_ARTI * 16), cosi' non si sovrappongono */
#define TILE_DADI (N_ARTI * 16)

static const unsigned short MASCHERE_DADO[7] = {
	0,
	(1 << 4),
	(1 << 0) | (1 << 8),
	(1 << 0) | (1 << 4) | (1 << 8),
	(1 << 0) | (1 << 2) | (1 << 6) | (1 << 8),
	(1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8),
	(1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8),
};

static const unsigned short CORPO_DADO[6] = {
	COL5(30, 30, 29),
	COL5(10, 20, 29),
	COL5(13, 13, 14),
	COL5(29, 26, 8),
	COL5(13, 27, 13),
	COL5(24, 17, 29),
};

static unsigned short schiarisci(unsigned short c) {
	int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
	r += (31 - r) / 2;
	g += (31 - g) / 2;
	b += (31 - b) / 2;
	return (unsigned short)(r | (g << 5) | (b << 10));
}

static unsigned short scurisci(unsigned short c) {
	int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
	r = r * 2 / 3;
	g = g * 2 / 3;
	b = b * 2 / 3;
	return (unsigned short)(r | (g << 5) | (b << 10));
}

/* angoli piu' arrotondati (raggio 4 invece di 3) */
static int dado_dentro(int x, int y, int w, int h) {
	if (x < 0 || y < 0 || x >= w || y >= h) return 0;
	int r = 4;
	int cx = x < r ? r : (x > w - 1 - r ? w - 1 - r : x);
	int cy = y < r ? r : (y > h - 1 - r ? h - 1 - r : y);
	int dx = x - cx, dy = y - cy;
	return dx * dx + dy * dy <= r * r + r;
}

static void carica_dadi(void) {
	volatile unsigned short *pal = (volatile unsigned short *)0x05000200;
	volatile unsigned int *vram = (volatile unsigned int *)0x06010000;
	static const int POS[3] = {2, 6, 10};
	unsigned char img[16][16];

	for (int b = 1; b <= 6; b++) {
		unsigned short corpo = CORPO_DADO[b - 1];
		pal[b * 16 + 0] = 0;
		pal[b * 16 + 1] = corpo;
		pal[b * 16 + 2] = scurisci(scurisci(corpo));
		pal[b * 16 + 3] = COL5(2, 2, 3);
		pal[b * 16 + 4] = schiarisci(corpo);
		pal[b * 16 + 5] = scurisci(corpo);
		pal[b * 16 + 6] = COL5(1, 1, 2);
		pal[b * 16 + 7] = COL5(15, 15, 18);
	}

	for (int f = 0; f < 7; f++) {
		for (int y = 0; y < 16; y++) {
			for (int x = 0; x < 16; x++) {
				int c = 0;
				if (dado_dentro(x, y, 15, 15)) {
					int bordo = !dado_dentro(x - 1, y, 15, 15) ||
					            !dado_dentro(x + 1, y, 15, 15) ||
					            !dado_dentro(x, y - 1, 15, 15) ||
					            !dado_dentro(x, y + 1, 15, 15);
					if (bordo) c = 2;
					else if (y <= 2 && x >= 3 && x <= 11) c = 4;
					else if (x <= 2 && y >= 3 && y <= 11) c = 4;
					else if (y >= 12 && x >= 3 && x <= 11) c = 5;
					else if (x >= 12 && y >= 3 && y <= 11) c = 5;
					else c = 1;
				} else if (dado_dentro(x - 1, y - 1, 15, 15)) {
					c = 6;
				}
				img[y][x] = (unsigned char)c;
			}
		}
		for (int r = 0; r < 3; r++) {
			for (int c = 0; c < 3; c++) {
				if (!((MASCHERE_DADO[f] >> (r * 3 + c)) & 1)) continue;
				for (int dy = 0; dy < 3; dy++) {
					for (int dx = 0; dx < 3; dx++) {
						img[POS[r] + dy][POS[c] + dx] = (dx == 0 && dy == 0) ? 7 : 3;
					}
				}
			}
		}
		for (int ty = 0; ty < 2; ty++) {
			for (int tx = 0; tx < 2; tx++) {
				int tile = TILE_DADI + f * 4 + ty * 2 + tx;
				for (int r = 0; r < 8; r++) {
					unsigned int w = 0;
					for (int p = 0; p < 8; p++) {
						w |= (unsigned int)img[ty * 8 + r][tx * 8 + p] << (4 * p);
					}
					vram[tile * 8 + r] = w;
				}
			}
		}
	}
}

static void sprite_dado(int slot, int x, int y, int valore, int banco) {
	volatile unsigned short *oam = (volatile unsigned short *)0x07000000;
	oam[slot * 4 + 0] = (unsigned short)(y & 0xFF);
	oam[slot * 4 + 1] = (unsigned short)((x & 0x1FF) | (1 << 14));
	oam[slot * 4 + 2] = (unsigned short)((TILE_DADI + valore * 4) | (banco << 12));
}

#endif