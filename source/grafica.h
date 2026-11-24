#ifndef GRAFICA_H
#define GRAFICA_H

#define N_ARTI 6
#define COL5(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

/* palette delle carte (banco 0) */
static const unsigned short PALETTE_CARTE[16] = {
	COL5(0, 0, 0),    /* 0 trasparente */
	COL5(8, 15, 17),  /* 1 sfondo carta */
	COL5(6, 22, 24),  /* 2 cornice turchese */
	COL5(31, 31, 31), /* 3 bianco */
	COL5(5, 3, 4),    /* 4 contorno */
	COL5(17, 14, 12), /* 5 Oleg: bruno-grigio */
	COL5(8, 6, 6),    /* 6 striscia scura */
	COL5(31, 19, 22), /* 7 rosa */
	COL5(31, 22, 8),  /* 8 Lala: dorato */
	COL5(31, 28, 22), /* 9 crema */
	COL5(21, 23, 27), /* 10 Gocciola: perla */
	COL5(28, 6, 10),  /* 11 occhio rosso / reattore interno */
	COL5(27, 15, 4),  /* 12 arancio scuro / anello reattore */
	COL5(27, 27, 28), /* 13 grigio chiaro */
	COL5(11, 13, 18), /* 14 striscia blu-grigia */
	COL5(26, 10, 14), /* 15 naso */
};

/* ---------- piccola tela 32x32 per disegnare le carte ---------- */
static unsigned char cv[32][32];

static void cv_px(int x, int y, int c) {
	if (x >= 0 && x < 32 && y >= 0 && y < 32) cv[y][x] = (unsigned char)c;
}

static void cv_rett(int x, int y, int w, int h, int c) {
	for (int j = y; j < y + h; j++) {
		for (int i = x; i < x + w; i++) cv_px(i, j, c);
	}
}

static void cv_ell(int cx2, int cy2, int rx2, int ry2, int c) {
	for (int y = 0; y < 32; y++) {
		for (int x = 0; x < 32; x++) {
			int dx = 2 * x + 1 - cx2;
			int dy = 2 * y + 1 - cy2;
			if (dx * dx * ry2 * ry2 + dy * dy * rx2 * rx2 <= rx2 * rx2 * ry2 * ry2) {
				cv[y][x] = (unsigned char)c;
			}
		}
	}
}

typedef struct {
	int corpo, pancia, orecchio, occhio, stria, guance, naso;
	int or_rx, or_ry, or_cy;
} Aspetto;

static const Aspetto ASPETTI[4] = {
	/* Oleg */     {5, 9, 7, 4, 6, 7, 15, 9, 9, 16},
	/* Yuki */     {3, 13, 7, 11, 0, 7, 7, 10, 13, 14},
	/* Lala */     {8, 9, 7, 4, 0, 12, 15, 9, 9, 16},
	/* Gocciola */ {10, 3, 7, 4, 14, 7, 15, 9, 9, 16},
};

static void cornice_tela(void) {
	for (int i = 0; i < 32; i++) {
		cv[0][i] = 2;
		cv[31][i] = 2;
		cv[i][0] = 2;
		cv[i][31] = 2;
	}
}

static void disegna_roditore(const Aspetto *a) {
	for (int y = 0; y < 32; y++) {
		for (int x = 0; x < 32; x++) cv[y][x] = 1;
	}

	cv_ell(17, a->or_cy, a->or_rx + 2, a->or_ry + 2, 4);
	cv_ell(47, a->or_cy, a->or_rx + 2, a->or_ry + 2, 4);
	cv_ell(17, a->or_cy, a->or_rx, a->or_ry, a->corpo);
	cv_ell(47, a->or_cy, a->or_rx, a->or_ry, a->corpo);
	cv_ell(17, a->or_cy + 1, a->or_rx - 4, a->or_ry - 4, a->orecchio);
	cv_ell(47, a->or_cy + 1, a->or_rx - 4, a->or_ry - 4, a->orecchio);

	cv_ell(32, 38, 26, 22, 4);
	cv_ell(32, 38, 24, 20, a->corpo);
	cv_ell(32, 46, 14, 10, a->pancia);
	cv_ell(15, 44, 5, 4, a->guance);
	cv_ell(49, 44, 5, 4, a->guance);

	if (a->stria) cv_rett(15, 10, 2, 6, a->stria);

	cv_ell(22, 36, 4, 5, a->occhio);
	cv_ell(42, 36, 4, 5, a->occhio);
	cv_px(10, 16, 3);
	cv_px(20, 16, 3);

	cv_px(15, 20, a->naso);
	cv_px(16, 20, a->naso);
	cv_px(14, 22, 4);
	cv_px(15, 23, 4);
	cv_px(16, 23, 4);
	cv_px(17, 22, 4);

	cornice_tela();
}

/* segnaposto del reattore per "Doom Core" (il Nucleo finale) */
static void disegna_core(void) {
	for (int y = 0; y < 32; y++) {
		for (int x = 0; x < 32; x++) cv[y][x] = 1;
	}
	cv_ell(32, 32, 26, 26, 2);
	cv_ell(32, 32, 22, 22, 4);
	cv_ell(32, 32, 18, 18, 11);
	cv_ell(32, 32, 12, 12, 12);
	cv_ell(32, 32, 6, 6, 3);
	cornice_tela();
}

/* copia palette e tile delle carte nella memoria video */
static void carica_grafica(void) {
	volatile unsigned short *pal = (volatile unsigned short *)0x05000200;
	volatile unsigned int *vram = (volatile unsigned int *)0x06010000;

	for (int i = 0; i < 16; i++) pal[i] = PALETTE_CARTE[i];

	for (int c = 0; c < N_ARTI; c++) {
		if (c < 4) disegna_roditore(&ASPETTI[c]);
		else disegna_core(); /* indici 4 e 5: entrambi il reattore, per ora */

		for (int ty = 0; ty < 4; ty++) {
			for (int tx = 0; tx < 4; tx++) {
				int tile = c * 16 + ty * 4 + tx;
				for (int r = 0; r < 8; r++) {
					unsigned int w = 0;
					for (int p = 0; p < 8; p++) {
						w |= (unsigned int)cv[ty * 8 + r][tx * 8 + p] << (4 * p);
					}
					vram[tile * 8 + r] = w;
				}
			}
		}
	}
}

static void sprite_nascondi(int slot) {
	volatile unsigned short *oam = (volatile unsigned short *)0x07000000;
	oam[slot * 4 + 0] = 2 << 8;
}

static void sprite_nascondi_tutti(void) {
	for (int i = 0; i < 128; i++) sprite_nascondi(i);
}

static void sprite_set(int slot, int x, int y, int tile) {
	volatile unsigned short *oam = (volatile unsigned short *)0x07000000;
	oam[slot * 4 + 0] = (unsigned short)(y & 0xFF);
	oam[slot * 4 + 1] = (unsigned short)((x & 0x1FF) | (2 << 14));
	oam[slot * 4 + 2] = (unsigned short)tile;
}

#endif