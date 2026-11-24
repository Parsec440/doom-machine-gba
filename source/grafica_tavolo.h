#ifndef GRAFICA_TAVOLO_H
#define GRAFICA_TAVOLO_H

#include "grafica.h"

/* Sfondo: griglia tecnologica + un condotto orizzontale e due verticali,
   su BG1, dietro al testo. Tile: 0 cella, 1 nodo acceso,
   2 tubo orizzontale, 3 tubo verticale, 4 incrocio. */

static unsigned short tav_tile_di(int tx, int ty) {
	/* condotti fissi: uno orizzontale a meta' altezza, due verticali */
	int oriz = (ty == 9);
	int vert = (tx == 5 || tx == 24);
	if (oriz && vert) return 4;
	if (oriz) return 2;
	if (vert) return 3;

	unsigned int h = ((unsigned int)tx * 374761393u) ^ ((unsigned int)ty * 668265263u);
	h = (h ^ (h >> 13)) * 1274126177u;
	return (unsigned short)((h % 19 == 0) ? 1 : 0);
}

static void tavolo_sfondo(void) {
	volatile unsigned short *map = (volatile unsigned short *)0x0600E000;
	for (int ty = 0; ty < 32; ty++) {
		for (int tx = 0; tx < 32; tx++) {
			map[ty * 32 + tx] = tav_tile_di(tx, ty) | (1 << 12);
		}
	}
}

static void carica_tavolo(void) {
	volatile unsigned short *pal = (volatile unsigned short *)0x05000000;
	volatile unsigned int *vram = (volatile unsigned int *)0x06008000;

	pal[16 + 1] = COL5(2, 3, 7);    /* blu notte di fondo */
	pal[16 + 2] = COL5(4, 15, 17);  /* linee della griglia */
	pal[16 + 3] = COL5(3, 5, 10);   /* leggera variazione */
	pal[16 + 4] = COL5(27, 23, 6);  /* nodo acceso, giallo */
	pal[16 + 5] = COL5(5, 9, 14);   /* corpo del tubo */
	pal[16 + 6] = COL5(7, 20, 22);  /* bordo del tubo, turchese */

	for (int t = 0; t < 5; t++) {
		for (int y = 0; y < 8; y++) {
			unsigned int w = 0;
			for (int x = 0; x < 8; x++) {
				int c = 1;
				if (t == 0) {
					if (x == 0 || y == 0) c = 2;
					else if (((x * 7 + y * 5) & 7) == 0) c = 3;
				} else if (t == 1) {
					if (x == 0 || y == 0) c = 2;
					else if (((x * 7 + y * 5) & 7) == 0) c = 3;
					if (x == 0 && y == 0) c = 4;
				} else if (t == 2) { /* tubo orizzontale */
					c = (y == 2 || y == 5) ? 6 : (y == 3 || y == 4 ? 5 : 1);
				} else if (t == 3) { /* tubo verticale */
					c = (x == 2 || x == 5) ? 6 : (x == 3 || x == 4 ? 5 : 1);
				} else { /* incrocio */
					int ino = (y == 2 || y == 5);
					int ive = (x == 2 || x == 5);
					int cno = (y == 3 || y == 4);
					int cve = (x == 3 || x == 4);
					if (ino || ive) c = 6;
					else if (cno || cve) c = 5;
					else c = 1;
				}
				w |= (unsigned int)c << (4 * x);
			}
			vram[t * 8 + y] = w;
		}
	}

	*(volatile unsigned short *)0x0400000A = 2 | (2 << 2) | (28 << 8);
	tavolo_sfondo();
}

#endif