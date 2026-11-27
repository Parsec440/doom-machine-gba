#ifndef AUDIO_H
#define AUDIO_H

/* Effetti sonori a due voci piu' rumore (canali 1, 2 e 4 del GBA).
   Una sequenza e' un elenco di note; ogni nota dura 'fr' frame (1/60 s).
     hz  : voce principale (canale 2, onda quadra al 50%, piena e dolce)
     hz2 : seconda voce (canale 1, onda quadra al 25%, piu' sottile) usata
           come armonia/basso; 0 = nessuna
     dec : decadimento del volume (1 = rapido e secco ... 7 = lento)
     rum : 1 = rumore (canale 4): hz e' il valore del registro (shift/divisore) */
typedef struct {
	unsigned short hz;
	unsigned short hz2;
	unsigned char fr;
	unsigned char dec;
	unsigned char rum;
} Nota;

/* volume generale 0-7 (consigliati 3-5) */
#define AU_VOLUME 4

#define RG16(a) (*(volatile unsigned short *)(a))
#define NT(hz, fr)       {hz, 0, fr, 2, 0}   /* nota breve e nitida */
#define NL(hz, fr)       {hz, 0, fr, 5, 0}   /* nota che risuona piu' a lungo */
#define ND(hz, hz2, fr)  {hz, hz2, fr, 2, 0} /* due voci insieme */
#define NDL(hz, hz2, fr) {hz, hz2, fr, 5, 0} /* due voci, risuonano a lungo */
#define NR(p, fr, dec)   {p, 0, fr, dec, 1}  /* rumore */
#define NP(fr)           {0, 0, fr, 0, 0}    /* pausa */
#define SUONA(s) au_suona(s, (int)(sizeof(s) / sizeof((s)[0])))

#include "audio_campioni.h"
#define AU_DS_CNT (2 | (1 << 2) | (1 << 8) | (1 << 9)) /* PSG 100%, DS A 100% su L+R, timer 0 */
#define CAMPIONE(id) au_campione(id)

static const Nota *au_seq = 0;
static int au_n = 0;
static int au_i = 0;
static int au_t = 0;

static void au_init(void) {
	RG16(0x04000084) = 0x80; /* alimenta l'audio */
	/* tutti i canali PSG su sinistra e destra, volume generale AU_VOLUME */
	RG16(0x04000080) = 0xFF00 | (AU_VOLUME << 4) | AU_VOLUME;
	RG16(0x04000082) = AU_DS_CNT; /* PSG al 100% + Direct Sound A */
	RG16(0x04000060) = 0;    /* canale 1 senza sweep */
}

/* ---- campioni (Direct Sound A): timer 0 + DMA 1 che riempie la FIFO ---- */
static int au_cm_t = 0; /* frame rimasti al campione in corso (0 = fermo) */

static void au_campione_stop(void) {
	*(volatile unsigned int *)0x040000C4 = 0; /* DMA1 off */
	RG16(0x04000102) = 0;                     /* timer 0 off */
	au_cm_t = 0;
}

static void au_campione(int id) {
	au_campione_stop();
	RG16(0x04000082) = AU_DS_CNT | (1 << 11); /* svuota la FIFO */
	*(volatile unsigned int *)0x040000BC = (unsigned int)(unsigned long)CAMP_DATI[id];
	*(volatile unsigned int *)0x040000C0 = 0x040000A0;
	*(volatile unsigned int *)0x040000C4 = 0xB6400000; /* FIFO, ripetuto, 32 bit */
	RG16(0x04000100) = (unsigned short)(65536 - 1024); /* 16384 campioni/s */
	RG16(0x04000102) = 0x80;
	au_cm_t = CAMP_FRAME[id];
}

static void au_freq(unsigned long reg, int hz) {
	/* frequenza del canale = 131072 / (2048 - f); si arrotonda al piu' vicino */
	int f = 2048 - (131072 + hz / 2) / hz;
	if (f < 0) f = 0;
	if (f > 2047) f = 2047;
	RG16(reg) = f | (1 << 15);
}

static void au_silenzio(void) {
	RG16(0x04000068) = 0; /* canale 2 */
	RG16(0x0400006C) = 1 << 15;
	RG16(0x04000062) = 0; /* canale 1 */
	RG16(0x04000064) = 1 << 15;
	RG16(0x04000078) = 0; /* canale 4 */
	RG16(0x0400007C) = 1 << 15;
}

static void au_nota(const Nota *n) {
	if (n->rum) {
		/* scoppio di rumore: schiocchi dei dadi, colpi */
		RG16(0x04000078) = (9 << 12) | (n->dec << 8);
		RG16(0x0400007C) = (n->hz & 0xFF) | (1 << 15);
		return;
	}
	if (n->hz) {
		RG16(0x04000068) = (9 << 12) | (n->dec << 8) | (2 << 6);
		au_freq(0x0400006C, n->hz);
	} else {
		RG16(0x04000068) = 0;
		RG16(0x0400006C) = 1 << 15;
	}
	if (n->hz2) {
		RG16(0x04000062) = (6 << 12) | (n->dec << 8) | (1 << 6);
		au_freq(0x04000064, n->hz2);
	} else {
		RG16(0x04000062) = 0;
		RG16(0x04000064) = 1 << 15;
	}
}

static void au_prossima(void) {
	if (au_i >= au_n) {
		au_seq = 0;
		return;
	}
	au_nota(&au_seq[au_i]);
	au_t = au_seq[au_i].fr;
	au_i++;
}

static void au_suona(const Nota *s, int n) {
	au_silenzio(); /* un nuovo suono taglia quello precedente */
	au_seq = s;
	au_n = n;
	au_i = 0;
	au_prossima();
}

static void au_aggiorna(void) {
	if (au_cm_t > 0 && --au_cm_t == 0) au_campione_stop();
	if (!au_seq) return;
	if (--au_t <= 0) au_prossima();
}

/* ---- effetti: tonalita' di Do maggiore, bip nitidi e arpeggi allegri ---- */

/* cursore e menu */
static const Nota S_SU_GIU[] = { NT(1175, 2) };
static const Nota S_SX_DX[] = { NT(1319, 2) };
static const Nota S_OK[] = { NT(1047, 3), NL(1568, 10) };
static const Nota S_INDIETRO[] = { NT(988, 3), NT(659, 8) };
static const Nota S_L[] = { NT(880, 3), NT(659, 7) };
static const Nota S_R[] = { NT(659, 3), NT(880, 7) };
static const Nota S_PAUSA[] = { NT(784, 3), NT(1047, 3), NL(1319, 10) };
static const Nota S_RIPRENDI[] = { NT(1319, 3), NT(1047, 3), NL(784, 10) };
static const Nota S_SELECT[] = { NT(880, 3), NL(1175, 9) };
static const Nota S_ERRORE[] = { NT(262, 4), NL(196, 9) };

/* dadi: schiocchi di rumore e un ultimo colpo */
static const Nota S_DADI[] = { NR(0x42, 3, 1), NR(0x52, 3, 1), NR(0x33, 3, 1),
                               NR(0x52, 3, 1), NR(0x42, 3, 1), NR(0x62, 4, 2),
                               NR(0x53, 6, 3) };
static const Nota S_DADO[] = { NR(0x42, 3, 1), NR(0x52, 3, 1), NR(0x62, 5, 2) };
static const Nota S_RITOCCO[] = { NT(1175, 3), NL(1568, 8) };
static const Nota S_DIFESA[] = { NT(392, 3), NT(523, 3), NDL(659, 330, 12) };

/* combattimento */
static const Nota S_COLPO[] = { NR(0x32, 3, 2), NT(262, 3), NT(196, 6) };
static const Nota S_CARTA_KO[] = { NR(0x32, 3, 2), NT(523, 3), NT(659, 3),
                                   NT(784, 3), NDL(1047, 523, 16) };
static const Nota S_DANNO[] = { NR(0x32, 4, 2), NT(247, 4), NT(196, 4), NL(147, 14) };
static const Nota S_FINE_TURNO[] = { ND(784, 392, 5), ND(659, 330, 5), NDL(523, 262, 16) };

/* jingle */
static const Nota S_AVVIO[] = { NT(523, 4), NT(659, 4), NT(784, 4), NT(1047, 4),
                                NDL(1319, 784, 16) };
static const Nota S_VITTORIA[] = {
	ND(523, 262, 6), ND(523, 262, 6), ND(523, 262, 6), ND(659, 330, 18),
	ND(587, 294, 6), ND(587, 294, 6), ND(587, 294, 6), ND(784, 392, 18),
	ND(659, 330, 8), ND(784, 392, 8), ND(1047, 523, 8), NDL(1047, 784, 36) };
static const Nota S_SCONFITTA[] = { ND(392, 196, 10), ND(330, 165, 10), ND(262, 131, 10),
                                    NDL(196, 98, 34) };

#endif
