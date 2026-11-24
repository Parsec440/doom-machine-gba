#ifndef AUDIO_H
#define AUDIO_H

typedef struct {
	unsigned short hz;
	unsigned char fr;
	unsigned char rum;
} Nota;

#define RG16(a) (*(volatile unsigned short *)(a))
#define NT(hz, fr) {hz, fr, 0}
#define NR(p, fr) {p, fr, 1}
#define SUONA(s) au_suona(s, (int)(sizeof(s) / sizeof((s)[0])))

static const Nota *au_seq = 0;
static int au_n = 0;
static int au_i = 0;
static int au_t = 0;

static void au_init(void) {
	RG16(0x04000084) = 0x80;
	RG16(0x04000080) = 0xAA22; /* volume basso (2/7), canali 2 e 4 attivi su L e R */
	RG16(0x04000082) = 2;      /* mixing: canali diretti al 50% */
}

static void au_nota(const Nota *n) {
	if (n->rum) {
		/* rumore morbido: decadimento lento, volume contenuto, onda larga (bit3=0) */
		RG16(0x04000078) = (3 << 12) | (5 << 8);
		RG16(0x0400007C) = (n->hz & 0xFF) | (1 << 15);
	} else if (n->hz) {
		int f = 2048 - 131072 / n->hz;
		/* onda quadra al 50% (piu' piena e calda del 25%), decadimento lento */
		RG16(0x04000068) = (5 << 12) | (4 << 8) | (2 << 6);
		RG16(0x0400006C) = (f & 0x7FF) | (1 << 15);
	} else {
		RG16(0x04000068) = 0;
		RG16(0x0400006C) = 1 << 15;
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
	au_seq = s;
	au_n = n;
	au_i = 0;
	au_prossima();
}

static void au_aggiorna(void) {
	if (!au_seq) return;
	if (--au_t <= 0) au_prossima();
}

/* note gravi e morbide, intervalli piccoli, nessun suono acuto o metallico */
static const Nota S_SU_GIU[] = { NT(294, 4) };
static const Nota S_SX_DX[] = { NT(330, 4) };
static const Nota S_OK[] = { NT(247, 4), NT(330, 8) };
static const Nota S_INDIETRO[] = { NT(220, 4), NT(165, 8) };
static const Nota S_L[] = { NT(262, 4), NT(220, 7) };
static const Nota S_R[] = { NT(220, 4), NT(262, 7) };
static const Nota S_PAUSA[] = { NT(165, 5), NT(196, 5), NT(247, 11) };
static const Nota S_RIPRENDI[] = { NT(247, 5), NT(196, 5), NT(165, 11) };
static const Nota S_SELECT[] = { NT(196, 5), NT(294, 8) };
static const Nota S_FINE_TURNO[] = { NT(330, 5), NT(247, 5), NT(196, 12) };
static const Nota S_ERRORE[] = { NT(130, 7), NT(110, 11) };
static const Nota S_DADI[] = { NR(0x62, 4), NR(0x61, 4), NR(0x62, 4),
                               NR(0x71, 4), NR(0x70, 8) };
static const Nota S_DADO[] = { NR(0x71, 4), NR(0x70, 8) };
static const Nota S_RITOCCO[] = { NT(294, 4), NT(370, 7) };
static const Nota S_DIFESA[] = { NT(165, 4), NT(196, 4), NT(220, 8) };
static const Nota S_COLPO[] = { NR(0x72, 5), NT(110, 9) };
static const Nota S_CARTA_KO[] = { NT(165, 4), NT(196, 4), NT(220, 4), NT(294, 12) };
static const Nota S_DANNO[] = { NR(0x73, 6), NT(70, 12) };
static const Nota S_VITTORIA[] = { NT(165, 6), NT(165, 6), NT(165, 6),
                                   NT(196, 9), NT(247, 9), NT(330, 24) };
static const Nota S_SCONFITTA[] = { NT(147, 11), NT(139, 11), NT(131, 11), NT(123, 28) };
static const Nota S_AVVIO[] = { NT(165, 5), NT(196, 5), NT(247, 5), NT(330, 13) };

#endif