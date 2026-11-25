#ifndef GRAFICA_TAVOLO_H
#define GRAFICA_TAVOLO_H

#include "grafica.h"

/* Sfondo: niente piu' griglia uniforme. Sei condotti (3 orizzontali,
   3 verticali) a righe/colonne diverse, alternati fra due schemi di
   colore (turchese e oro), piu' nodi luminosi sparsi di tre colori.
   Tile: 0 cella (solo lieve puntinatura, nessun bordo), 1/5/6 nodo
   acceso (oro/teal/ambra), 2/3/4 tubo oriz./vert./incrocio turchese,
   7/8/9 tubo oriz./vert./incrocio oro. */

#define TAV_N_ORIZ 3
#define TAV_N_VERT 3

static const int TAV_RIGHE[TAV_N_ORIZ] = {4, 9, 15};
static const int TAV_COLONNE[TAV_N_VERT] = {3, 14, 27};

static unsigned short tav_tile_di(int tx, int ty) {
	int io = -1, iv = -1;
	for (int i = 0; i < TAV_N_ORIZ; i++) if (ty == TAV_RIGHE[i]) io = i;
	for (int i = 0; i < TAV_N_VERT; i++) if (tx == TAV_COLONNE[i]) iv = i;

	if (io >= 0 && iv >= 0) {
		return (unsigned short)(((io & 1) || (iv & 1)) ? 9 : 4);
	}
	if (io >= 0) return (unsigned short)((io & 1) ? 7 : 2);
	if (iv >= 0) return (unsigned short)((iv & 1) ? 8 : 3);

	unsigned int h = ((unsigned int)tx * 374761393u) ^ ((unsigned int)ty * 668265263u);
	h = (h ^ (h >> 13)) * 1274126177u;
	if (h % 23 == 0) return 1;
	if (h % 29 == 0) return 5;
	if (h % 31 == 0) return 6;
	return 0;
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

	pal[16 + 1] = COL5(2, 3, 7);     /* blu notte di fondo */
	pal[16 + 2] = COL5(4, 15, 17);   /* (non piu' usato come bordo griglia) */
	pal[16 + 3] = COL5(3, 5, 10);    /* leggera puntinatura */
	pal[16 + 4] = COL5(27, 23, 6);   /* nodo acceso, oro */
	pal[16 + 5] = COL5(5, 9, 14);    /* corpo tubo turchese */
	pal[16 + 6] = COL5(7, 20, 22);   /* bordo tubo turchese */
	pal[16 + 7] = COL5(27, 20, 5);   /* bordo tubo oro */
	pal[16 + 8] = COL5(12, 9, 3);    /* corpo tubo oro */
	pal[16 + 9] = COL5(6, 27, 24);   /* nodo acceso, teal */
	pal[16 + 10] = COL5(27, 11, 6);  /* nodo acceso, ambra */

	for (int t = 0; t < 10; t++) {
		for (int y = 0; y < 8; y++) {
			unsigned int w = 0;
			for (int x = 0; x < 8; x++) {
				int c = 1;
				switch (t) {
				case 0: /* cella: solo un lieve puntino, niente bordi */
					if (((x * 7 + y * 5) & 7) == 0) c = 3;
					break;
				case 1: /* nodo oro */
					c = ((x == 3 || x == 4) && (y == 3 || y == 4)) ? 4 : 1;
					break;
				case 2: /* tubo orizzontale turchese */
					c = (y == 2 || y == 5) ? 6 : (y == 3 || y == 4 ? 5 : 1);
					break;
				case 3: /* tubo verticale turchese */
					c = (x == 2 || x == 5) ? 6 : (x == 3 || x == 4 ? 5 : 1);
					break;
				case 4: { /* incrocio turchese */
					int ino = (y == 2 || y == 5);
					int ive = (x == 2 || x == 5);
					int cno = (y == 3 || y == 4);
					int cve = (x == 3 || x == 4);
					if (ino || ive) c = 6;
					else if (cno || cve) c = 5;
					else c = 1;
					break;
				}
				case 5: /* nodo teal */
					c = ((x == 3 || x == 4) && (y == 3 || y == 4)) ? 9 : 1;
					break;
				case 6: /* nodo ambra */
					c = ((x == 3 || x == 4) && (y == 3 || y == 4)) ? 10 : 1;
					break;
				case 7: /* tubo orizzontale oro */
					c = (y == 2 || y == 5) ? 7 : (y == 3 || y == 4 ? 8 : 1);
					break;
				case 8: /* tubo verticale oro */
					c = (x == 2 || x == 5) ? 7 : (x == 3 || x == 4 ? 8 : 1);
					break;
				default: { /* incrocio oro */
					int ino = (y == 2 || y == 5);
					int ive = (x == 2 || x == 5);
					int cno = (y == 3 || y == 4);
					int cve = (x == 3 || x == 4);
					if (ino || ive) c = 7;
					else if (cno || cve) c = 8;
					else c = 1;
					break;
				}
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