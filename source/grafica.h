#ifndef GRAFICA_H
#define GRAFICA_H

#define N_ARTI 10
#define COL5(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

/* palette delle carte (banco 0) */
static const unsigned short PALETTE_CARTE[16] = {
	COL5(0, 0, 0),    /* 0 trasparente */
	COL5(8, 15, 17),  /* 1 sfondo carta */
	COL5(6, 22, 24),  /* 2 cornice turchese */
	COL5(31, 31, 31), /* 3 bianco */
	COL5(5, 3, 4),    /* 4 contorno */
	COL5(17, 14, 12), /* 5 grigio acciaio */
	COL5(8, 6, 6),    /* 6 striscia scura */
	COL5(30, 10, 14), /* 7 rosso segnale */
	COL5(31, 22, 8),  /* 8 oro */
	COL5(31, 28, 22), /* 9 crema */
	COL5(14, 22, 31), /* 10 azzurro ghiaccio */
	COL5(28, 6, 10),  /* 11 rosso reattore */
	COL5(27, 15, 4),  /* 12 arancio reattore */
	COL5(27, 27, 28), /* 13 grigio chiaro */
	COL5(6, 18, 20),  /* 14 blu petrolio */
	COL5(18, 4, 24),  /* 15 viola cupo */
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

static void cornice_tela(void) {
	for (int i = 0; i < 32; i++) {
		cv[0][i] = 2;
		cv[31][i] = 2;
		cv[i][0] = 2;
		cv[i][31] = 2;
	}
}

/* Scrap Sentinel: torretta corazzata con antenna e occhio rosso */
static void disegna_sentinella(void) {
	for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) cv[y][x] = 1;
	cv_rett(14, 4, 4, 10, 4);
	cv_px(16, 4, 7);
	cv_ell(32, 40, 24, 22, 4);
	cv_ell(32, 40, 20, 18, 5);
	cv_rett(8, 34, 16, 8, 6);
	cv_ell(32, 44, 10, 10, 4);
	cv_ell(32, 44, 6, 6, 7);
	cv_rett(10, 52, 6, 6, 13);
	cv_rett(48, 52, 6, 6, 13);
	cornice_tela();
}

/* Frost Relay: pannello col cristallo di ghiaccio */
static void disegna_relay(void) {
	for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) cv[y][x] = 1;
	cv_ell(32, 32, 26, 26, 4);
	cv_ell(32, 32, 22, 22, 2);
	cv_ell(32, 32, 16, 16, 5);
	cv_rett(14, 8, 4, 18, 10);
	cv_rett(16, 4, 4, 10, 3);
	cv_rett(29, 18, 4, 20, 10);
	cv_px(16, 8, 3);
	cornice_tela();
}

/* Gilt Capacitor: cilindro dorato a fasce */
static void disegna_capacitore(void) {
	for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) cv[y][x] = 1;
	cv_rett(9, 6, 14, 24, 4);
	cv_rett(10, 7, 12, 22, 8);
	cv_rett(10, 11, 12, 3, 6);
	cv_rett(10, 17, 12, 3, 6);
	cv_rett(10, 23, 12, 3, 6);
	cv_ell(32, 14, 12, 8, 9);
	cornice_tela();
}

/* Coolant Drip: goccia di refrigerante azzurra */
static void disegna_goccia(void) {
	for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) cv[y][x] = 1;
	cv_ell(32, 44, 20, 18, 4);
	cv_ell(32, 44, 16, 14, 14);
	cv_ell(26, 36, 5, 5, 3);
	cv_rett(14, 6, 4, 20, 5);
	cv_rett(15, 7, 2, 18, 13);
	cornice_tela();
}

/* Misery Extractor: trivella/artiglio viola con nucleo rosso */
static void disegna_estrattore(void) {
	for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) cv[y][x] = 1;
	cv_ell(32, 30, 24, 20, 4);
	cv_ell(32, 30, 20, 16, 15);
	cv_rett(8, 46, 4, 12, 4);
	cv_rett(52, 46, 4, 12, 4);
	cv_rett(9, 47, 2, 10, 15);
	cv_rett(53, 47, 2, 10, 15);
	cv_ell(32, 30, 8, 8, 4);
	cv_ell(32, 30, 5, 5, 11);
	cornice_tela();
}

/* Infernal Power Complex: reattore incandescente con picchi di energia */
static void disegna_complesso(void) {
	for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) cv[y][x] = 1;
	cv_ell(32, 32, 26, 26, 4);
	cv_ell(32, 32, 20, 20, 7);
	cv_ell(32, 32, 14, 14, 12);
	cv_ell(32, 32, 7, 7, 3);
	cv_rett(15, 2, 2, 8, 7);
	cv_rett(47, 2, 2, 8, 7);
	cv_rett(15, 52, 2, 8, 7);
	cv_rett(47, 52, 2, 8, 7);
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
		switch (c) {
		case 0: disegna_sentinella(); break;
		case 1: disegna_relay(); break;
		case 2: disegna_capacitore(); break;
		case 3: disegna_goccia(); break;
		case 4: disegna_estrattore(); break;
		case 5: disegna_complesso(); break;
		default: disegna_core(); break;
		}

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