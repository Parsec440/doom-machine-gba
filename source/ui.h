#ifndef UI_H
#define UI_H

#include <string.h>
#include "grafica.h"

#define SCHERMO_W 240
#define SCHERMO_H 160
#define TILE_COL 30
#define TILE_RIG 20
#define N_PAROLE (TILE_COL * TILE_RIG * 8)

enum {
	K_TRASP, K_PANNELLO, K_ORO, K_BORDO, K_TESTO, K_GIALLO, K_ROSSO,
	K_VERDE, K_BLU, K_GRIGIO, K_SEL, K_BARRA, K_VIOLA, K_TURCHESE,
	K_LUCE, K_OMBRA
};

static unsigned int tela[N_PAROLE];

static const unsigned char FONT[91][5] = {
	{0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00},
	{0x00, 0x07, 0x00, 0x07, 0x00}, {0x14, 0x7F, 0x14, 0x7F, 0x14},
	{0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
	{0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00},
	{0x00, 0x1C, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1C, 0x00},
	{0x08, 0x2A, 0x1C, 0x2A, 0x08}, {0x08, 0x08, 0x3E, 0x08, 0x08},
	{0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08},
	{0x00, 0x60, 0x60, 0x00, 0x00}, {0x20, 0x10, 0x08, 0x04, 0x02},
	{0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
	{0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
	{0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
	{0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
	{0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
	{0x00, 0x36, 0x36, 0x00, 0x00}, {0x00, 0x56, 0x36, 0x00, 0x00},
	{0x00, 0x08, 0x14, 0x22, 0x41}, {0x14, 0x14, 0x14, 0x14, 0x14},
	{0x41, 0x22, 0x14, 0x08, 0x00}, {0x02, 0x01, 0x51, 0x09, 0x06},
	{0x32, 0x49, 0x79, 0x41, 0x3E}, {0x7E, 0x11, 0x11, 0x11, 0x7E},
	{0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
	{0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41},
	{0x7F, 0x09, 0x09, 0x01, 0x01}, {0x3E, 0x41, 0x41, 0x51, 0x32},
	{0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
	{0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41},
	{0x7F, 0x40, 0x40, 0x40, 0x40}, {0x7F, 0x02, 0x04, 0x02, 0x7F},
	{0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
	{0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E},
	{0x7F, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31},
	{0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
	{0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x7F, 0x20, 0x18, 0x20, 0x7F},
	{0x63, 0x14, 0x08, 0x14, 0x63}, {0x03, 0x04, 0x78, 0x04, 0x03},
	{0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x00, 0x7F, 0x41, 0x41},
	{0x02, 0x04, 0x08, 0x10, 0x20}, {0x41, 0x41, 0x7F, 0x00, 0x00},
	{0x04, 0x02, 0x01, 0x02, 0x04}, {0x40, 0x40, 0x40, 0x40, 0x40},
	{0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78},
	{0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20},
	{0x38, 0x44, 0x44, 0x48, 0x7F}, {0x38, 0x54, 0x54, 0x54, 0x18},
	{0x08, 0x7E, 0x09, 0x01, 0x02}, {0x08, 0x14, 0x54, 0x54, 0x3C},
	{0x7F, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7D, 0x40, 0x00},
	{0x20, 0x40, 0x44, 0x3D, 0x00}, {0x00, 0x7F, 0x10, 0x28, 0x44},
	{0x00, 0x41, 0x7F, 0x40, 0x00}, {0x7C, 0x04, 0x18, 0x04, 0x78},
	{0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38},
	{0x7C, 0x14, 0x14, 0x14, 0x08}, {0x08, 0x14, 0x14, 0x18, 0x7C},
	{0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
	{0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C},
	{0x1C, 0x20, 0x40, 0x20, 0x1C}, {0x3C, 0x40, 0x30, 0x40, 0x3C},
	{0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
	{0x44, 0x64, 0x54, 0x4C, 0x44},
};

static void ui_pulisci(void) {
	for (int i = 0; i < N_PAROLE; i++) tela[i] = 0;
}

static void ui_px(int x, int y, int c) {
	if (x < 0 || y < 0 || x >= SCHERMO_W || y >= SCHERMO_H) return;
	int idx = ((y >> 3) * TILE_COL + (x >> 3)) * 8 + (y & 7);
	int sh = (x & 7) * 4;
	tela[idx] = (tela[idx] & ~(0xFu << sh)) | ((unsigned int)c << sh);
}

static void ui_rett(int x, int y, int w, int h, int c) {
	if (w <= 0 || h <= 0) return;
	int x1 = x + w, y1 = y + h;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x1 > SCHERMO_W) x1 = SCHERMO_W;
	if (y1 > SCHERMO_H) y1 = SCHERMO_H;
	if (x >= x1 || y >= y1) return;
	unsigned int pieno = (unsigned int)c * 0x11111111u;
	for (int yy = y; yy < y1; yy++) {
		for (int tx = x >> 3; tx <= ((x1 - 1) >> 3); tx++) {
			int a = (tx * 8 > x) ? 0 : x - tx * 8;
			int b = (tx * 8 + 8 > x1) ? x1 - tx * 8 : 8;
			unsigned int maschera = 0xFFFFFFFFu << (4 * a);
			if (b < 8) maschera &= ~(0xFFFFFFFFu << (4 * b));
			int idx = ((yy >> 3) * TILE_COL + tx) * 8 + (yy & 7);
			tela[idx] = (tela[idx] & ~maschera) | (pieno & maschera);
		}
	}
}

static void ui_cornice(int x, int y, int w, int h, int c) {
	ui_rett(x, y, w, 1, c);
	ui_rett(x, y + h - 1, w, 1, c);
	ui_rett(x, y, 1, h, c);
	ui_rett(x + w - 1, y, 1, h, c);
}

/* inset per riga, per un angolo arrotondato (quasi un quarto di cerchio, raggio 3) */
static const int PAN_INSET3[3] = {3, 1, 0};

/* pannello con angoli arrotondati (piu' morbido del taglio diagonale) */
static void ui_pannello(int x, int y, int w, int h) {
	int r = 3;
	if (h < 2 * r) r = h / 2;
	if (w < 2 * r) r = w / 2;
	if (r < 0) r = 0;

	for (int yy = 0; yy < h; yy++) {
		int ins = 0;
		if (r >= 3) {
			if (yy < 3) ins = PAN_INSET3[yy];
			else if (yy >= h - 3) ins = PAN_INSET3[h - 1 - yy];
		} else if (r > 0) {
			if (yy < r) ins = r - yy;
			else if (yy >= h - r) ins = r - (h - 1 - yy);
		}
		int rw = w - 2 * ins;
		if (rw <= 0) continue;
		ui_rett(x + ins, y + yy, rw, 1, K_PANNELLO);
		ui_px(x + ins, y + yy, K_ORO);
		ui_px(x + ins + rw - 1, y + yy, K_ORO);
	}
	if (w - 2 * r > 0) {
		ui_rett(x + r, y, w - 2 * r, 1, K_ORO);
		ui_rett(x + r, y + h - 1, w - 2 * r, 1, K_ORO);
	}
	int ins1 = (r >= 3) ? PAN_INSET3[1] : (r > 1 ? r - 1 : 0);
	if (w - 2 * ins1 - 2 > 0) ui_rett(x + ins1 + 1, y + 1, w - 2 * ins1 - 2, 1, K_LUCE);
}

/* barra orizzontale a tacche separate */
static void ui_barra(int x, int y, int w, int h, int val, int max, int col) {
	if (max <= 0) return;
	if (val < 0) val = 0;
	if (val > max) val = max;
	int n = max < 10 ? max : 10;
	if (n < 1) n = 1;
	int filled = (val * n) / max;
	int gap = n > 1 ? 1 : 0;
	int segw = (w - (n - 1) * gap) / n;
	if (segw < 1) segw = 1;
	int usedw = segw * n + gap * (n - 1);
	int ox = x + (w - usedw) / 2;
	for (int i = 0; i < n; i++) {
		int sx = ox + i * (segw + gap);
		int on = i < filled;
		ui_rett(sx, y, segw, h, on ? col : K_BARRA);
		ui_cornice(sx, y, segw, h, K_BORDO);
		if (on && h > 2) ui_rett(sx, y, segw, 1, K_LUCE);
	}
}

/* barra verticale a tacche, si riempie dal basso */
static void ui_barra_v(int x, int y, int w, int h, int val, int max, int col) {
	if (max <= 0) return;
	int show = val;
	if (show < 0) show = 0;
	if (show > max) show = max;
	int n = max < 10 ? max : 10;
	if (n < 1) n = 1;
	int filled = (show * n) / max;
	int gap = n > 1 ? 1 : 0;
	int segh = (h - (n - 1) * gap) / n;
	if (segh < 1) segh = 1;
	int usedh = segh * n + gap * (n - 1);
	int oy = y + (h - usedh);
	for (int i = 0; i < n; i++) {
		int sy = oy + (n - 1 - i) * (segh + gap);
		int on = i < filled;
		ui_rett(x, sy, w, segh, on ? col : K_BARRA);
		ui_cornice(x, sy, w, segh, K_BORDO);
	}
}

static void ui_freccia_giu(int cx, int y, int c) {
	ui_rett(cx - 2, y, 5, 1, c);
	ui_rett(cx - 1, y + 1, 3, 1, c);
	ui_px(cx, y + 2, c);
}

static void ui_freccia_su(int cx, int y, int c) {
	ui_px(cx, y, c);
	ui_rett(cx - 1, y + 1, 3, 1, c);
	ui_rett(cx - 2, y + 2, 5, 1, c);
}

static int ui_largh_glifo(int ch, int *prima) {
	if (ch < 32 || ch > 122) ch = 32;
	const unsigned char *g = FONT[ch - 32];
	int lo = 0, hi = 4;
	while (lo <= 4 && g[lo] == 0) lo++;
	if (lo > 4) {
		*prima = 0;
		return 3; /* spazio piu' largo: le parole si staccano meglio */
	}
	while (hi >= 0 && g[hi] == 0) hi--;
	*prima = lo;
	return hi - lo + 1;
}

static int ui_larghezza(const char *s, int scala) {
	int w = 0;
	for (; *s; s++) {
		int p;
		w += (ui_largh_glifo((unsigned char)*s, &p) + 1) * scala;
	}
	return w > 0 ? w - scala : 0;
}

static int ui_testo(int x, int y, const char *s, int c, int scala) {
	for (; *s; s++) {
		int p;
		int ch = (unsigned char)*s;
		if (ch < 32 || ch > 122) ch = 32;
		int w = ui_largh_glifo(ch, &p);
		if (ch != 32) {
			const unsigned char *g = FONT[ch - 32];
			for (int col = 0; col < w; col++) {
				for (int riga = 0; riga < 7; riga++) {
					if ((g[p + col] >> riga) & 1) {
						if (scala == 1) ui_px(x + col, y + riga, c);
						else ui_rett(x + col * scala, y + riga * scala, scala, scala, c);
					}
				}
			}
		}
		x += (w + 1) * scala;
	}
	return x;
}

static int ui_testo_c(int cx, int y, const char *s, int c, int scala) {
	return ui_testo(cx - ui_larghezza(s, scala) / 2, y, s, c, scala);
}

static int ui_testo_d(int xr, int y, const char *s, int c, int scala) {
	return ui_testo(xr - ui_larghezza(s, scala), y, s, c, scala);
}

static void ui_carica(void) {
	volatile unsigned int *vram = (volatile unsigned int *)0x06000000;
	for (int i = 0; i < N_PAROLE; i++) vram[i] = tela[i];
}

static void ui_init(void) {
	volatile unsigned short *pal = (volatile unsigned short *)0x05000000;
	volatile unsigned short *map = (volatile unsigned short *)0x0600F000;

	pal[0] = 0;
	pal[K_PANNELLO] = COL5(1, 3, 6);
	pal[K_ORO] = COL5(29, 24, 4);
	pal[K_BORDO] = COL5(3, 9, 13);
	pal[K_TESTO] = COL5(21, 30, 31);
	pal[K_GIALLO] = COL5(30, 26, 4);
	pal[K_ROSSO] = COL5(30, 8, 8);
	pal[K_VERDE] = COL5(6, 28, 19);
	pal[K_BLU] = COL5(6, 15, 30);
	pal[K_GRIGIO] = COL5(12, 17, 22);
	pal[K_SEL] = COL5(3, 10, 17);
	pal[K_BARRA] = COL5(2, 5, 9);
	pal[K_VIOLA] = COL5(19, 9, 30);
	pal[K_TURCHESE] = COL5(4, 28, 26);
	pal[K_LUCE] = COL5(30, 30, 28);
	pal[K_OMBRA] = COL5(1, 1, 3);

	for (int ty = 0; ty < 32; ty++) {
		for (int tx = 0; tx < 32; tx++) {
			map[ty * 32 + tx] = (tx < TILE_COL && ty < TILE_RIG) ? ty * TILE_COL + tx : 0;
		}
	}

	*(volatile unsigned short *)0x04000008 = (30 << 8);
}

#endif