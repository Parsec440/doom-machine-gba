#ifndef TRANSIZIONE_H
#define TRANSIZIONE_H

/* Scorrimento tra due schermate (effetto "push"): la vecchia esce da un lato
   e la nuova entra dall'altro, insieme, con frenata finale.

   Come funziona: lo sfondo UI e' un solo livello con una mappa di 32 colonne
   di tile che si ripete in orizzontale. Immaginiamo una "striscia" di colonne
   [vecchia | nuova] e una finestra di 31 colonne che la attraversa. Ogni volta
   che una colonna vecchia esce dallo schermo, i suoi 20 tile vengono riusati
   per la colonna nuova che sta entrando: cosi' non serve nessun buffer extra
   (la vecchia schermata e' gia' in VRAM, la nuova e' in 'tela'). Il resto lo
   fa lo scorrimento hardware (registro HOFS). Gli sprite (carte, dadi)
   scorrono insieme alle schermate. */

#define TR_FRAME 13   /* durata in frame (circa 0,22 s) */
#define TR_SPRITE 32  /* voci OAM copiate per la vecchia e per la nuova schermata */
#define TR_VUOTO 99   /* colonna senza dati (fuori dalle due schermate) */
#define TR_RISERVA TILE_COL /* indice della colonna di riserva (tile 600-619) */

static unsigned short oam_a[TR_SPRITE * 4]; /* sprite della schermata vecchia */
static unsigned short oam_b[TR_SPRITE * 4]; /* sprite della schermata nuova */

static void tr_leggi_oam(unsigned short *dst) {
	volatile unsigned short *oam = (volatile unsigned short *)0x07000000;
	for (int i = 0; i < TR_SPRITE * 4; i++) dst[i] = oam[i];
}

static void tr_scrivi_oam(const unsigned short *src) {
	volatile unsigned short *oam = (volatile unsigned short *)0x07000000;
	for (int i = 0; i < TR_SPRITE * 4; i++) oam[i] = src[i];
}

/* scrive negli slot OAM da 'base' gli sprite di uno snapshot spostati di dx pixel */
static void tr_oam_sposta(const unsigned short *src, int base, int dx) {
	volatile unsigned short *oam = (volatile unsigned short *)0x07000000;
	for (int i = 0; i < TR_SPRITE; i++) {
		unsigned short a0 = src[i * 4 + 0];
		unsigned short a1 = src[i * 4 + 1];
		unsigned short a2 = src[i * 4 + 2];
		int x = a1 & 0x1FF;
		if (x >= 256) x -= 512;
		x += dx;
		if ((a0 & 0x0300) == 0x0200 || x <= -32 || x >= SCHERMO_W) {
			oam[(base + i) * 4 + 0] = 2 << 8; /* nascosto */
		} else {
			oam[(base + i) * 4 + 0] = a0;
			oam[(base + i) * 4 + 1] = (unsigned short)((a1 & ~0x1FF) | (x & 0x1FF));
			oam[(base + i) * 4 + 2] = a2;
		}
	}
}

/* per ogni colonna 's' della striscia (indice s + 32): colonna di tile che la
   contiene (0-29 posizione originale, 30 riserva), TR_VUOTO, oppure -1 */
static int tr_slot[96];
static int tr_libere[32];
static int tr_n_libere;

static int tr_tile(int ty, int slot) {
	if (slot == TR_VUOTO) return TILE_VUOTO;
	return slot < TR_RISERVA ? ty * TILE_COL + slot : TILE_COL * TILE_RIG + ty;
}

/* mappa: la colonna 's' della striscia sta nella colonna (s mod 32) della mappa */
static void tr_mappa(int s, int slot) {
	volatile unsigned short *map = (volatile unsigned short *)0x0600F000;
	int m = s & 31;
	for (int ty = 0; ty < TILE_RIG; ty++) map[ty * 32 + m] = (unsigned short)tr_tile(ty, slot);
}

/* copia nella VRAM la colonna 'n' della nuova schermata, nei tile dello slot */
static void tr_copia_colonna(int n, int slot) {
	volatile unsigned int *vram = (volatile unsigned int *)0x06000000;
	for (int ty = 0; ty < TILE_RIG; ty++) {
		int dst = tr_tile(ty, slot) * 8;
		int src = (ty * TILE_COL + n) * 8;
		for (int r = 0; r < 8; r++) vram[dst + r] = tela[src + r];
	}
}

/* quale colonna e' la 's' della striscia: 0 = vecchia, 1 = nuova, 2 = nessuna */
static int tr_colonna(int s, int dir, int *n) {
	if (s >= 0 && s < TILE_COL) {
		*n = s;
		return 0;
	}
	int k = dir > 0 ? s - TILE_COL : s + TILE_COL;
	if (k >= 0 && k < TILE_COL) {
		*n = k;
		return 1;
	}
	return 2;
}

/* aggiorna mappa e tile in modo che le colonne [c0, c0 + 30] siano visibili */
static void tr_finestra(int c0, int dir) {
	for (int i = 0; i < 96; i++) {
		int s = i - 32;
		if (tr_slot[i] != -1 && (s < c0 || s > c0 + TILE_COL)) {
			if (tr_slot[i] != TR_VUOTO) tr_libere[tr_n_libere++] = tr_slot[i];
			tr_slot[i] = -1;
		}
	}
	for (int s = c0; s <= c0 + TILE_COL; s++) {
		int i = s + 32;
		if (i < 0 || i >= 96 || tr_slot[i] != -1) continue;
		int n = 0;
		int tipo = tr_colonna(s, dir, &n);
		if (tipo != 1 || tr_n_libere == 0) {
			tr_slot[i] = TR_VUOTO;
			tr_mappa(s, TR_VUOTO);
			continue;
		}
		int slot = tr_libere[--tr_n_libere];
		tr_copia_colonna(n, slot);
		tr_slot[i] = slot;
		tr_mappa(s, slot);
	}
}

/* 'tela' contiene la nuova schermata, la VRAM ha ancora la vecchia; oam_a e
   oam_b hanno gli sprite delle due schermate. dir = +1: la nuova entra da
   destra, -1: da sinistra. */
static void transizione(int dir) {
	for (int i = 0; i < 96; i++) tr_slot[i] = -1;
	for (int s = 0; s < TILE_COL; s++) tr_slot[s + 32] = s;
	tr_n_libere = 0;
	tr_libere[tr_n_libere++] = TR_RISERVA;

	for (int k = 1; k <= TR_FRAME; k++) {
		int d = TR_FRAME - k;
		int o = SCHERMO_W - (SCHERMO_W * d * d) / (TR_FRAME * TR_FRAME); /* frenata finale */
		int w = dir * o;
		/* tutto il lavoro sulla grafica va fatto dentro il VBlank, e le cose
		   che servono prima (scorrimento, sprite) vanno per prime: se lo
		   scorrimento arriva tardi le righe in alto sfarfallano */
		VBlankIntrWait();
		RG16(0x04000010) = (unsigned short)(w & 0x1FF);
		tr_oam_sposta(oam_a, 0, -dir * o);
		tr_oam_sposta(oam_b, 64, dir * (SCHERMO_W - o));
		tr_finestra(w >> 3, dir);
		au_aggiorna();
	}

	/* un frame col risultato finale, poi si rimette tutto nella forma normale
	   (identica a quella che si vede, quindi senza scatti) */
	VBlankIntrWait();
	RG16(0x04000010) = 0;
	tr_scrivi_oam(oam_b);
	for (int i = 64; i < 64 + TR_SPRITE; i++) sprite_nascondi(i);
	ui_carica();
	ui_mappa_base();
	au_aggiorna();
}

#endif
