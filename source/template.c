#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grafica.h"
#include "grafica_dadi.h"
#include "grafica_tavolo.h"
#include "ui.h"
#include "audio.h"
#include "transizione.h"

#define MAX_DADI 10
#define MAX_RILANCI 3
#define MAX_RITOCCHI 3
/* modalita' difficile: meno rilanci e meno ritocchi per turno */
#define MAX_RILANCI_DIFF 2
#define MAX_RITOCCHI_DIFF 2
#define MAX_DIFESA 4
#define MAX_CAMPO 10
#define SLOT_SPRITE MAX_CAMPO
#define MAX_CAS 3
#define MAX_AZ 6
#define PV_GIOCATORE 20
#define MAX_RISORSA 12

#define DADO_Y 107
/* griglia delle carte in campo: 5 colonne, due righe (sopra/sotto) cosi'
   si vedono tutte insieme invece di perderne alcune quando sono tante */
#define CARTA_COLONNE 5
#define CARTA_X(i) (20 + 42 * ((i) % CARTA_COLONNE))
#define CARTA_Y(i) (((i) / CARTA_COLONNE) == 0 ? 29 : 72)

/* ---------- generatore casuale (xorshift32) ---------- */
static unsigned int seme_rng = 1;

static void rng_seed(unsigned int s) {
	seme_rng = s ? s : 1;
}

static unsigned int rng32(void) {
	seme_rng ^= seme_rng << 13;
	seme_rng ^= seme_rng >> 17;
	seme_rng ^= seme_rng << 5;
	return seme_rng;
}

static int tira_dado(void) {
	return (int)(rng32() % 6) + 1;
}

/* ---------- CARTE ---------- */
typedef struct {
	char tipo;
	int val;
	int val2; /* usato solo da 'o': soddisfatta da val OPPURE val2 */
} Casella;

typedef enum {
	AZ_NULLA, AZ_DANNO_SP, AZ_CARICA_SP, AZ_DANNO_SX,
	AZ_DANNO_FISSO,   /* danno pari a az.val */
	AZ_LIMITA_DIFESA, /* il turno successivo si puo' mettere in difesa un solo dado */
	AZ_DANNO_POT,     /* danno pari al solo Potere */
	AZ_CARICA_POT,    /* Potere +1 (Sentenza invariata) */
	AZ_CARICA_SENTENZA, /* Sentenza +1 (Potere invariato) */
	AZ_DANNO_SENTENZA,  /* danno pari alla sola Sentenza */
	AZ_RIPARA_ADIACENTI, /* ripara az.val PV alle carte adiacenti sul campo */
	AZ_DANNO_SOGLIA_SENTENZA, /* danno 3, o 5 se la Sentenza supera 5 */
	AZ_RIPARA_SE,     /* la carta ripara az.val PV a se stessa (fino al massimo) */
	AZ_DANNO_CARTE,   /* danno pari al numero di carte in campo */
	AZ_BLOCCA_DADO,   /* il turno successivo si tira un dado in meno */
	AZ_RIMUOVI_DADO,  /* come BLOCCA_DADO, ma per sempre se la Sentenza supera 6 */
	AZ_AVANZA_ADIACENTI, /* le carte accanto eseguono un passo in piu' questo turno */
} TipoAzione;
typedef struct {
	TipoAzione tipo;
	int val;
} Azione;

/* effetto scatenato quando la carta viene sconfitta */
typedef enum { OM_NESSUNA, OM_DANNO_SX, OM_DANNO_TUTTI, OM_RIDUCI_PS,
               OM_CARICA_POT, /* Potere +om_val */
               OM_RIDUCI_POT /* Potere -om_val */ } OnMorte;

typedef struct {
	const char *nome;
	int pv;
	int n_cas;
	Casella cas[MAX_CAS];
	int n_az;
	Azione az[MAX_AZ];
	OnMorte on_morte;
	int om_val;
} Carta;

static const Carta POOL[] = {
	{"Misery Extractor", 6, 2, {{'=', 2}, {'=', 3}}, 4,
	 {{AZ_NULLA}, {AZ_DANNO_SX}, {AZ_DANNO_SX}, {AZ_DANNO_SX}},
	 OM_DANNO_SX, 2},
	{"Infernal Power Complex", 3, 3, {{'=', 1}, {'=', 1}, {'=', 1}}, 4,
	 {{AZ_NULLA}, {AZ_CARICA_SP}, {AZ_NULLA}, {AZ_CARICA_SP}},
	 OM_DANNO_TUTTI, 1},
	{"Design Flaw", 4, 2, {{'=', 3}, {'=', 4}}, 5,
	 {{AZ_NULLA}, {AZ_DANNO_FISSO, 3}, {AZ_DANNO_FISSO, 3}, {AZ_NULLA}, {AZ_DANNO_FISSO, 3}},
	 OM_RIDUCI_PS, 2},
	{"Thought Siphon", 4, 2, {{'=', 6}, {'=', 6}}, 5,
	 {{AZ_NULLA}, {AZ_DANNO_FISSO, 3}, {AZ_DANNO_FISSO, 3}, {AZ_LIMITA_DIFESA}, {AZ_NULLA}}},
	{"Extinction Collider", 6, 1, {{'o', 4, 5}}, 5,
	 {{AZ_NULLA}, {AZ_DANNO_POT}, {AZ_CARICA_POT}, {AZ_DANNO_POT}, {AZ_CARICA_POT}}},
	{"Recycling Vats", 6, 1, {{'o', 6, 1}}, 5,
	 {{AZ_NULLA}, {AZ_DANNO_FISSO, 4}, {AZ_DANNO_FISSO, 4}, {AZ_RIPARA_ADIACENTI, 2}, {AZ_DANNO_FISSO, 4}}},
	{"Synaptic Disruptor", 4, 1, {{'o', 4, 5}}, 3,
	 {{AZ_NULLA}, {AZ_CARICA_SENTENZA}, {AZ_DANNO_SENTENZA}}},
	{"Cerebral Amplifier", 4, 2, {{'~', 0, 0}, {'~', 0, 0}}, 3,
	 {{AZ_NULLA}, {AZ_DANNO_SOGLIA_SENTENZA}, {AZ_DANNO_SOGLIA_SENTENZA}}},
	{"Pulse Pool", 3, 2, {{'=', 6}, {'=', 1}}, 4,
	 {{AZ_NULLA}, {AZ_CARICA_POT}, {AZ_CARICA_POT}, {AZ_DANNO_FISSO, 3}}},
	{"Logic Mainframe", 4, 2, {{'~', 0, 0}, {'~', 0, 0}}, 3,
	 {{AZ_NULLA}, {AZ_CARICA_SENTENZA}, {AZ_CARICA_SENTENZA}},
	 OM_CARICA_POT, 2},
	{"Populace Grinder", 3, 2, {{'=', 3}, {'=', 3}}, 3,
	 {{AZ_NULLA}, {AZ_DANNO_FISSO, 4}, {AZ_RIPARA_SE, 3}}},
	{"Impact Nexus", 4, 2, {{'~', 0, 0}, {'~', 0, 0}}, 2,
	 {{AZ_NULLA}, {AZ_DANNO_CARTE}},
	 OM_RIDUCI_POT, 1},
	/* '+': val = ruolo (0 e 1 = addendi, 2 = somma); il terzo dado deve
	   valere la somma dei primi due */
	{"Soul Harvester", 3, 3, {{'+', 0}, {'+', 1}, {'+', 2}}, 4,
	 {{AZ_NULLA}, {AZ_DANNO_FISSO, 5}, {AZ_BLOCCA_DADO}, {AZ_DANNO_FISSO, 5}}},
	{"Stasis Generator", 6, 2, {{'=', 3}, {'<', 3}}, 4,
	 {{AZ_NULLA}, {AZ_RIMUOVI_DADO}, {AZ_NULLA}, {AZ_RIMUOVI_DADO}}},
	{"Hard Cycle Storage", 5, 2, {{'=', 3}, {'>', 3}}, 4,
	 {{AZ_NULLA}, {AZ_AVANZA_ADIACENTI}, {AZ_DANNO_FISSO, 4}, {AZ_AVANZA_ADIACENTI}}},
};
#define N_POOL 15
static const Carta NUCLEO = {"Doom Core", 4, 3, {{'=', 6}, {'=', 6}, {'=', 6}}, 4,
                              {{AZ_NULLA}, {AZ_DANNO_SP}, {AZ_CARICA_SP}, {AZ_CARICA_SP}}};
#define N_MAZZO (N_POOL + 1)
_Static_assert(N_ARTI == N_MAZZO, "N_ARTI (grafica.h) deve essere N_POOL + 1");

static const Carta *prendi(int idx) {
	return idx < 0 ? &NUCLEO : &POOL[idx];
}

static int tile_carta(int idx) {
	int slot = (idx < 0 ? N_POOL : idx);
	return slot * 16 | (banco_carta(slot) << 12);
}

static int casella_ok(Casella c, int v) {
	if (c.tipo == '=') return v == c.val;
	if (c.tipo == 'o') return v == c.val || v == c.val2;
	if (c.tipo == '<') return v < c.val;
	if (c.tipo == '+') return 1; /* addendi e somma si controllano in assegna() */
	if (c.tipo == '~') return 1; /* "due dadi uguali": qualunque valore va bene qui,
	                                 il vincolo (uguale all'altro '~') si controlla in assegna() */
	return v > c.val;
}

static const char LETTERE[] = "ABCDEF";

static const char *azione_desc(Azione a) {
	static char buf[24];
	switch (a.tipo) {
	case AZ_NULLA: return "Nulla";
	case AZ_DANNO_SP: return "Danno = Pot+Sen";
	case AZ_CARICA_SP: return "Carica Pot+1 Sen+1";
	case AZ_DANNO_SX: return "Danno = PV carta a sx";
	case AZ_DANNO_FISSO: sprintf(buf, "Danno %d", a.val); return buf;
	case AZ_LIMITA_DIFESA: return "Limita difesa a 1 (prox turno)";
	case AZ_DANNO_POT: return "Danno = Potere";
	case AZ_CARICA_POT: return "Carica Potere +1";
	case AZ_CARICA_SENTENZA: return "Carica Sentenza +1";
	case AZ_DANNO_SENTENZA: return "Danno = Sentenza";
	case AZ_RIPARA_ADIACENTI: sprintf(buf, "Ripara %d PV adiacenti", a.val); return buf;
	case AZ_DANNO_SOGLIA_SENTENZA: return "Danno 3 (5 se Sentenza > 5)";
	case AZ_RIPARA_SE: sprintf(buf, "Auto-ripara %d PV", a.val); return buf;
	case AZ_DANNO_CARTE: return "Danno = carte in campo";
	case AZ_BLOCCA_DADO: return "Blocca 1 dado (prox turno)";
	case AZ_RIMUOVI_DADO: return "Rimuove dado (perm. Sen>6)";
	case AZ_AVANZA_ADIACENTI: return "Carte accanto: 2 step";
	}
	return "";
}

static int mazzo[N_MAZZO];
static int prossima = 0;
static int campo[MAX_CAMPO];
static int pv_campo[MAX_CAMPO];
static int eta_campo[MAX_CAMPO];
/* passi in piu' che ogni carta esegue nel turno nemico corrente */
static int passi_extra[MAX_CAMPO];
static int n_campo = 0;

static void prepara_mazzo(void) {
	for (int i = 0; i < N_POOL; i++) mazzo[i] = i;
	for (int i = N_POOL - 1; i > 0; i--) {
		int j = (int)(rng32() % (unsigned int)(i + 1));
		int t = mazzo[i];
		mazzo[i] = mazzo[j];
		mazzo[j] = t;
	}
	mazzo[N_POOL] = -1;
	prossima = 0;
	n_campo = 0;
}

static void entra_carta(void) {
	if (n_campo >= MAX_CAMPO || prossima >= N_MAZZO) return;
	for (int i = n_campo; i > 0; i--) {
		campo[i] = campo[i - 1];
		pv_campo[i] = pv_campo[i - 1];
		eta_campo[i] = eta_campo[i - 1];
	}
	campo[0] = mazzo[prossima++];
	pv_campo[0] = prendi(campo[0])->pv;
	eta_campo[0] = 0;
	n_campo++;
}

/* ---------- DADI ---------- */
static int n_dadi = 5;
static int n_attivi = 5;
static int dadi[MAX_DADI];
static int rilanciato[MAX_DADI];
static int ritoccato[MAX_DADI];
static int stato_dado[MAX_DADI];
static int selezionato = 0;
static int rilanci_usati = 0;
static int ritocchi_usati = 0;
static int tirato = 0;

static int difficile = 0; /* 1 = modalita' difficile (scelta nel menu del titolo) */

static int max_rilanci(void) { return difficile ? MAX_RILANCI_DIFF : MAX_RILANCI; }
static int max_ritocchi(void) { return difficile ? MAX_RITOCCHI_DIFF : MAX_RITOCCHI; }

enum { AZX_RILANCIA, AZX_PIU, AZX_MENO, AZX_DIFESA, AZX_TOGLI, AZX_ATTACCA };
static const char *const NOME_VOCE[6] = {
	"Rilancia", "Aumenta +1", "Diminuisci -1", "Metti in difesa",
	"Togli dalla difesa", "Attacca...",
};
static int voci[6];
static int n_voci = 0;
static int voce_sel = 0;
static int menu_aperto = 0;
static int da_menu = 0;

/* ---------- MENU DELLA CARTA ---------- */
enum { CZ_ATTACCA, CZ_VEDI };
static const char *const NOME_VOCE_CARTA[2] = {"Attacca...", "Vedi ciclo"};
static int voci_carta[2];
static int n_voci_carta = 0;
static int voce_carta = 0;
static int carta_menu = 0;
static int vedi_ciclo = 0;

/* ---------- STATO ---------- */
static int iniziata = 0;
static int titolo_voce = 0; /* 0 Nuova Partita, 1 Difficolta', 2 Codex, 3 Guida, 4 Opzioni */
#define N_VOCI_TITOLO 5
#define N_VOCI_PAUSA 5
static int codex = 0;       /* 1 = si sta sfogliando il Codex delle carte */
static int codex_cursore = 0;
static int opzioni = 0;  /* 1 = schermata Opzioni (dal titolo o dalla pausa) */
static int opz_voce = 0; /* 0 = Musica, 1 = Effetti, 2 = Indietro */
static int guida = 0;    /* 1 = schermata Guida (dal titolo o dalla pausa) */
static int guida_pag = 0;
static int vinto = 0;
static int perso = 0;
static int pausa = 0;
static int voce_pausa = 0;
static int conferma = 0;
static int vista = 0;
static int turno = 1;
static int cursore = 0;

static int pv_giocatore = PV_GIOCATORE;
static int potere = 0;
static int sentenza = 0;
static int ultimo_danno = 0;
static int ultima_difesa = 0;

static int attacco = 0;
static int bersaglio = 0;
static int riempita[MAX_CAS];
static int dado_in[MAX_CAS];

/* limite di dadi in difesa per il turno corrente (normalmente MAX_DIFESA,
   abbassato a 1 dal debuff di "Thought Siphon") */
static int limite_difesa = MAX_DIFESA;
static int prossimo_limite_1 = 0;

/* dadi bloccati: il turno dopo "Soul Harvester" si tira un dado in meno.
   prossimo_blocco si accumula durante il turno nemico e diventa
   dadi_bloccati quando inizia il turno successivo. */
static int dadi_bloccati = 0;
static int prossimo_blocco = 0;

static int dadi_disponibili(void) {
	int n = n_dadi - dadi_bloccati;
	return n < 1 ? 1 : n;
}

static int dado_x(int i, int q) {
	return (SCHERMO_W - (q * 22 - 6)) / 2 + 22 * i;
}

static int difesa_conta(void) {
	int n = 0;
	if (!tirato) return 0;
	for (int i = 0; i < n_attivi; i++) {
		if (stato_dado[i] == 3) n++;
	}
	return n;
}

static int difesa_somma(void) {
	int s = 0;
	if (!tirato) return 0;
	for (int i = 0; i < n_attivi; i++) {
		if (stato_dado[i] == 3) s += dadi[i];
	}
	return s;
}

static int banco_dado(int i) {
	if (stato_dado[i] == 2) return 3;
	if (stato_dado[i] == 1) return 4;
	if (stato_dado[i] == 3) return 2;
	if (rilanciato[i]) return 5;
	if (ritoccato[i]) return 6;
	return 1;
}

static char lettera_stato(int i, int *col) {
	if (stato_dado[i] == 2) { *col = K_GRIGIO; return 'X'; }
	if (stato_dado[i] == 1) { *col = K_GIALLO; return '*'; }
	if (stato_dado[i] == 3) { *col = K_BLU; return 'D'; }
	if (rilanciato[i]) { *col = K_VERDE; return 'R'; }
	if (ritoccato[i]) { *col = K_VIOLA; return '+'; }
	*col = K_TESTO;
	return ' ';
}

static void ritocca(int delta) {
	if (ritocchi_usati >= max_ritocchi()) return;
	if (stato_dado[selezionato] != 0) return;
	if (rilanciato[selezionato] || ritoccato[selezionato]) return;
	int nuovo = dadi[selezionato] + delta;
	if (nuovo < 1 || nuovo > 6) return;
	dadi[selezionato] = nuovo;
	ritoccato[selezionato] = 1;
	ritocchi_usati++;
}

static void rilancia(void) {
	if (rilanci_usati >= max_rilanci()) return;
	if (stato_dado[selezionato] != 0) return;
	if (rilanciato[selezionato] || ritoccato[selezionato]) return;
	dadi[selezionato] = tira_dado();
	rilanciato[selezionato] = 1;
	rilanci_usati++;
}

static void metti_difesa(void) {
	if (stato_dado[selezionato] != 0) return;
	if (difesa_conta() >= limite_difesa) return;
	stato_dado[selezionato] = 3;
}

static void togli_difesa(void) {
	if (stato_dado[selezionato] == 3) stato_dado[selezionato] = 0;
}

static void tira(void) {
	n_attivi = dadi_disponibili();
	for (int i = 0; i < n_attivi; i++) {
		dadi[i] = tira_dado();
		rilanciato[i] = 0;
		ritoccato[i] = 0;
		stato_dado[i] = 0;
	}
	rilanci_usati = 0;
	ritocchi_usati = 0;
	selezionato = 0;
	voce_sel = 0;
	menu_aperto = 0;
	tirato = 1;
}

/* rimuove la carta in posizione i, applicando prima il suo effetto alla
   morte (se presente) e gestendo le eventuali morti a catena che ne
   derivano */
static void sconfitta(int i) {
	const Carta *c = prendi(campo[i]);
	if (campo[i] == -1) vinto = 1;

	if (c->on_morte == OM_DANNO_SX) {
		if (i > 0) pv_campo[i - 1] -= c->om_val;
	} else if (c->on_morte == OM_DANNO_TUTTI) {
		for (int j = 0; j < n_campo; j++) {
			if (j != i) pv_campo[j] -= c->om_val;
		}
	} else if (c->on_morte == OM_RIDUCI_PS) {
		potere -= c->om_val;
		if (potere < 0) potere = 0;
		sentenza -= c->om_val;
		if (sentenza < 0) sentenza = 0;
	} else if (c->on_morte == OM_CARICA_POT) {
		potere += c->om_val;
	} else if (c->on_morte == OM_RIDUCI_POT) {
		potere -= c->om_val;
		if (potere < 0) potere = 0;
	}

	for (int j = i; j < n_campo - 1; j++) {
		campo[j] = campo[j + 1];
		pv_campo[j] = pv_campo[j + 1];
		eta_campo[j] = eta_campo[j + 1];
	}
	n_campo--;
	if (n_dadi < MAX_DADI) n_dadi++;
	if (cursore >= n_campo) cursore = n_campo > 0 ? n_campo - 1 : 0;

	for (int j = 0; j < n_campo; j++) {
		if (pv_campo[j] <= 0) {
			sconfitta(j);
			j = -1;
		}
	}
}

static void annulla_attacco(void) {
	const Carta *c = prendi(campo[bersaglio]);
	for (int k = 0; k < c->n_cas; k++) {
		if (riempita[k]) stato_dado[dado_in[k]] = 0;
	}
	attacco = 0;
}

static int assegna(void) {
	int d = selezionato;
	if (stato_dado[d] != 0) return 0;
	const Carta *c = prendi(campo[bersaglio]);
	int trovata = 0;

	for (int passo = 0; passo < 2 && !trovata; passo++) {
		for (int k = 0; k < c->n_cas && !trovata; k++) {
			int e_uguale = (c->cas[k].tipo == '=');
			if (riempita[k]) continue;
			if ((passo == 0) != e_uguale) continue;
			if (casella_ok(c->cas[k], dadi[d])) {
				if (c->cas[k].tipo == '~') {
					/* deve combaciare con l'eventuale altra casella '~'
					   gia' riempita sulla stessa carta */
					int combacia = 1;
					for (int m = 0; m < c->n_cas; m++) {
						if (m != k && c->cas[m].tipo == '~' && riempita[m] &&
						    dadi[dado_in[m]] != dadi[d]) {
							combacia = 0;
						}
					}
					if (!combacia) continue;
				}
				if (c->cas[k].tipo == '+') {
					/* i primi due dadi sono gli addendi, il terzo deve
					   valere la loro somma (e restare un dado valido) */
					int ruolo = c->cas[k].val;
					int a = -1, b = -1;
					for (int m = 0; m < c->n_cas; m++) {
						if (c->cas[m].tipo != '+' || !riempita[m]) continue;
						if (c->cas[m].val == 0) a = dadi[dado_in[m]];
						if (c->cas[m].val == 1) b = dadi[dado_in[m]];
					}
					int v = dadi[d];
					int ok;
					if (ruolo == 2) ok = (a >= 0 && b >= 0 && v == a + b);
					else if (ruolo == 0) ok = (v <= 5 && (b < 0 || v + b <= 6));
					else ok = (v <= 5 && (a < 0 || v + a <= 6));
					if (!ok) continue;
				}
				riempita[k] = 1;
				dado_in[k] = d;
				stato_dado[d] = 1;
				trovata = 1;
			}
		}
	}
	if (!trovata) return 0;

	for (int k = 0; k < c->n_cas; k++) {
		if (!riempita[k]) return 1;
	}
	for (int k = 0; k < c->n_cas; k++) stato_dado[dado_in[k]] = 2;
	attacco = 0;
	pv_campo[bersaglio]--;
	if (pv_campo[bersaglio] <= 0) {
		sconfitta(bersaglio);
		return 3;
	}
	return 2;
}

static void esito_assegna(int r) {
	if (vinto) SUONA(S_VITTORIA);
	else if (r == 3) SUONA(S_CARTA_KO);
	else if (r == 2) SUONA(S_COLPO);
	else if (r == 1) CAMPIONE(CAMP_SELEZIONE);
	else CAMPIONE(CAMP_ERRORE);
}

static void costruisci_menu(void) {
	n_voci = 0;
	if (!tirato || attacco || vista != 0) return;
	int s = stato_dado[selezionato];
	if (s == 3) {
		voci[n_voci++] = AZX_TOGLI;
	} else if (s == 0) {
		int intatto = !rilanciato[selezionato] && !ritoccato[selezionato];
		if (intatto && rilanci_usati < max_rilanci()) voci[n_voci++] = AZX_RILANCIA;
		if (intatto && ritocchi_usati < max_ritocchi()) {
			if (dadi[selezionato] < 6) voci[n_voci++] = AZX_PIU;
			if (dadi[selezionato] > 1) voci[n_voci++] = AZX_MENO;
		}
		if (difesa_conta() < limite_difesa) voci[n_voci++] = AZX_DIFESA;
		if (n_campo > 0) voci[n_voci++] = AZX_ATTACCA;
	}
	if (voce_sel >= n_voci) voce_sel = n_voci > 0 ? n_voci - 1 : 0;
}

static void esegui_voce(void) {
	costruisci_menu();
	menu_aperto = 0;
	if (n_voci == 0) {
		CAMPIONE(CAMP_ERRORE);
		return;
	}
	switch (voci[voce_sel]) {
	case AZX_RILANCIA: rilancia(); CAMPIONE(CAMP_DADI); break;
	case AZX_PIU: ritocca(+1); SUONA(S_RITOCCO); break;
	case AZX_MENO: ritocca(-1); SUONA(S_RITOCCO); break;
	case AZX_DIFESA: metti_difesa(); SUONA(S_DIFESA); break;
	case AZX_TOGLI: togli_difesa(); CAMPIONE(CAMP_DESELEZIONE); break;
	case AZX_ATTACCA:
		da_menu = 1;
		vista = 1;
		CAMPIONE(CAMP_SELEZIONE);
		break;
	}
}

/* costruisce il menu "Attacca... / Vedi ciclo" per la carta selezionata */
static void costruisci_menu_carta(void) {
	n_voci_carta = 0;
	if (n_campo == 0) return;
	if (tirato) voci_carta[n_voci_carta++] = CZ_ATTACCA;
	voci_carta[n_voci_carta++] = CZ_VEDI;
	if (voce_carta >= n_voci_carta) voce_carta = n_voci_carta > 0 ? n_voci_carta - 1 : 0;
}

/* esegue il passo 'pos' del ciclo della carta in posizione i */
static void esegui_azione(int i, int pos, int *danno) {
	const Carta *c = prendi(campo[i]);
	switch (c->az[pos].tipo) {
	case AZ_NULLA: break;
	case AZ_DANNO_SP: *danno += potere + sentenza; break;
	case AZ_CARICA_SP: potere++; sentenza++; break;
	case AZ_DANNO_SX: if (i > 0) *danno += pv_campo[i - 1]; break;
	case AZ_DANNO_FISSO: *danno += c->az[pos].val; break;
	case AZ_LIMITA_DIFESA: prossimo_limite_1 = 1; break;
	case AZ_DANNO_POT: *danno += potere; break;
	case AZ_CARICA_POT: potere++; break;
	case AZ_CARICA_SENTENZA: sentenza++; break;
	case AZ_DANNO_SENTENZA: *danno += sentenza; break;
	case AZ_RIPARA_ADIACENTI: {
		int v = c->az[pos].val;
		if (i > 0) {
			pv_campo[i - 1] += v;
			int max = prendi(campo[i - 1])->pv;
			if (pv_campo[i - 1] > max) pv_campo[i - 1] = max;
		}
		if (i + 1 < n_campo) {
			pv_campo[i + 1] += v;
			int max = prendi(campo[i + 1])->pv;
			if (pv_campo[i + 1] > max) pv_campo[i + 1] = max;
		}
		break;
	}
	case AZ_DANNO_SOGLIA_SENTENZA: *danno += (sentenza > 5) ? 5 : 3; break;
	case AZ_DANNO_CARTE: *danno += n_campo; break;
	case AZ_BLOCCA_DADO: prossimo_blocco++; break;
	case AZ_RIMUOVI_DADO:
		if (sentenza > 6) {
			if (n_dadi > 1) n_dadi--; /* per sempre */
		} else {
			prossimo_blocco++;
		}
		break;
	case AZ_AVANZA_ADIACENTI: break; /* gia' applicato in turno_nemici() */
	case AZ_RIPARA_SE:
		pv_campo[i] += c->az[pos].val;
		if (pv_campo[i] > c->pv) pv_campo[i] = c->pv;
		break;
	}
}

static void turno_nemici(void) {
	int danno = 0;
	/* "Hard Cycle Storage": le carte accanto eseguono un passo in piu'
	   (due step invece di uno) in questo turno */
	for (int i = 0; i < n_campo; i++) passi_extra[i] = 0;
	for (int i = 0; i < n_campo; i++) {
		const Carta *c = prendi(campo[i]);
		if (c->az[eta_campo[i] % c->n_az].tipo != AZ_AVANZA_ADIACENTI) continue;
		if (i > 0) passi_extra[i - 1]++;
		if (i + 1 < n_campo) passi_extra[i + 1]++;
	}
	for (int i = 0; i < n_campo; i++) {
		const Carta *c = prendi(campo[i]);
		for (int e = 0; e <= passi_extra[i]; e++) {
			esegui_azione(i, (eta_campo[i] + e) % c->n_az, &danno);
		}
	}
	int dif = difesa_somma();
	int netto = danno - dif;
	if (netto < 0) netto = 0;
	pv_giocatore -= netto;
	ultimo_danno = netto;
	ultima_difesa = dif;
	if (pv_giocatore <= 0) perso = 1;
}

static void fine_turno(void) {
	if (attacco) annulla_attacco();
	turno_nemici();
	limite_difesa = prossimo_limite_1 ? 1 : MAX_DIFESA;
	prossimo_limite_1 = 0;
	dadi_bloccati = prossimo_blocco;
	prossimo_blocco = 0;
	tirato = 0;
	turno++;
	for (int i = 0; i < n_campo; i++) eta_campo[i] += 1 + passi_extra[i];
	entra_carta();
	cursore = 0;
	voce_sel = 0;
	menu_aperto = 0;
	da_menu = 0;
	carta_menu = 0;
	vedi_ciclo = 0;
	voce_carta = 0;
	conferma = 0;
	rilanci_usati = 0;
	ritocchi_usati = 0;
	if (perso) SUONA(S_SCONFITTA);
	else if (ultimo_danno > 0) SUONA(S_DANNO);
	else SUONA(S_FINE_TURNO);
}

/* ---------- DISEGNO ---------- */

/* Gli sprite delle carte hanno contorni scuri e trasparenza: sopra i pannelli
   blu notte si perdevano. Dietro a ogni carta si disegna quindi un fondo
   grigio-azzurro piu' chiaro, su cui contorni e colori risaltano. */
static void fondo_carta(int x, int y, int cornice) {
	ui_rett(x - 2, y - 2, 36, 36, K_GRIGIO);
	ui_cornice(x - 2, y - 2, 36, 36, cornice);
}

/* posizione verticale dello sprite nella vista "ciclo" */
static int ciclo_sprite_y(void) {
	int h = 24 + prendi(campo[cursore])->n_az * 10;
	int py = (160 - h) / 2;
	if (py < 8) py = 8;
	int sy = py + h / 2 - 16;
	if (sy < 2) sy = 2;
	if (sy > 128) sy = 128;
	return sy;
}

static void aggiorna_sprite(void) {
	for (int s = 0; s < SLOT_SPRITE + MAX_DADI; s++) sprite_nascondi(s);

	if (opzioni || guida) return;
	if (!iniziata) {
		if (codex) {
			int idx = (codex_cursore < N_POOL) ? codex_cursore : -1;
			sprite_set(0, 191, 44, tile_carta(idx));
		}
		return;
	}
	if (vinto || perso) return;
	if (pausa || conferma) return;

	if (vista == 1) {
		if (vedi_ciclo) {
			/* nella vista "ciclo" mostra solo lo sprite della carta in esame,
			   spostato di lato rispetto al pannello */
			sprite_set(0, 198, ciclo_sprite_y(), tile_carta(campo[cursore]));
		} else {
			for (int i = 0; i < n_campo && i < SLOT_SPRITE; i++) {
				int y = (i == cursore) ? CARTA_Y(i) - 3 : CARTA_Y(i);
				sprite_set(i, CARTA_X(i), y, tile_carta(campo[i]));
			}
		}
	} else {
		if (attacco) sprite_set(0, 28, 40, tile_carta(campo[bersaglio]));
		int q = tirato ? n_attivi : dadi_disponibili();
		for (int i = 0; i < q; i++) {
			int x = dado_x(i, q);
			int y = DADO_Y;
			if (tirato) {
				if (i == selezionato) y -= 3;
				sprite_dado(SLOT_SPRITE + i, x, y, dadi[i], banco_dado(i));
			} else {
				sprite_dado(SLOT_SPRITE + i, x, y, 0, 1);
			}
		}
	}
}

#define CASELLA_W 24
#define CASELLA_P 28 /* passo tra una casella e la successiva */

static void ui_casella(int x, int y, Casella c, int piena) {
	char t[8];
	if (c.tipo == '=') sprintf(t, "%d", c.val);
	else if (c.tipo == 'o') sprintf(t, "%d/%d", c.val, c.val2);
	else if (c.tipo == '~') sprintf(t, "##");
	else if (c.tipo == '<') sprintf(t, "<%d", c.val);
	else if (c.tipo == '+') strcpy(t, c.val == 2 ? "a+b" : (c.val == 1 ? "b" : "a"));
	else sprintf(t, ">%d", c.val);
	ui_rett(x, y, CASELLA_W, 11, piena ? K_VERDE : K_BARRA);
	ui_cornice(x, y, CASELLA_W, 11, K_ORO);
	ui_testo_c(x + CASELLA_W / 2, y + 2, t, piena ? K_PANNELLO : K_TESTO, 1);
}

/* stampa un testo andando a capo (una volta sola) se non entra in maxw,
   cosi' resta sempre dentro al riquadro; con centrato!=0 ogni riga viene
   centrata su x, altrimenti x e' il margine sinistro. Ritorna la y della
   riga successiva libera. */
static int ui_testo_wrap(int x, int y, int maxw, const char *s, int c, int scala, int centrato) {
	if (ui_larghezza(s, scala) <= maxw) {
		if (centrato) ui_testo_c(x, y, s, c, scala); else ui_testo(x, y, s, c, scala);
		return y + 8 * scala;
	}
	int n = (int)strlen(s);
	int taglio = -1;
	for (int i = 1; i < n; i++) {
		if (s[i] != ' ') continue;
		char riga[48];
		int len = i < 47 ? i : 47;
		memcpy(riga, s, len);
		riga[len] = 0;
		if (ui_larghezza(riga, scala) <= maxw) taglio = i;
		else break;
	}
	if (taglio < 0) {
		if (centrato) ui_testo_c(x, y, s, c, scala); else ui_testo(x, y, s, c, scala);
		return y + 8 * scala;
	}
	char riga[48];
	int len = taglio < 47 ? taglio : 47;
	memcpy(riga, s, len);
	riga[len] = 0;
	if (centrato) {
		ui_testo_c(x, y, riga, c, scala);
		ui_testo_c(x, y + 8 * scala, s + taglio + 1, c, scala);
	} else {
		ui_testo(x, y, riga, c, scala);
		ui_testo(x, y + 8 * scala, s + taglio + 1, c, scala);
	}
	return y + 16 * scala;
}

/* barra dei comandi su una sola riga. spec = "TASTO|azione;TASTO|azione;..."
   Il tasto sta in una piccola etichetta evidenziata, l'azione e' testo
   normale; tutta la riga e' centrata e sta sempre dentro il riquadro. */
static void ui_aiuto(const char *spec) {
	char tasto[6][12], azione[6][16];
	int n = 0;
	const char *p = spec;
	while (*p && n < 6) {
		int a = 0, b = 0;
		while (*p && *p != '|' && a < 11) tasto[n][a++] = *p++;
		tasto[n][a] = 0;
		if (*p == '|') p++;
		while (*p && *p != ';' && b < 15) azione[n][b++] = *p++;
		azione[n][b] = 0;
		while (*p && *p != ';') p++;
		if (*p == ';') p++;
		n++;
	}
	int tot = 0;
	for (int i = 0; i < n; i++) {
		tot += ui_larghezza(tasto[i], 1) + 6 + 3 + ui_larghezza(azione[i], 1);
		if (i + 1 < n) tot += 8;
	}
	ui_pannello(2, 146, 236, 13);
	int x = 120 - tot / 2;
	for (int i = 0; i < n; i++) {
		int cw = ui_larghezza(tasto[i], 1) + 6;
		ui_rett(x, 148, cw, 9, K_SEL);
		ui_testo(x + 3, 149, tasto[i], K_GIALLO, 1);
		x += cw + 3;
		x = ui_testo(x, 149, azione[i], K_TESTO, 1) + 7;
	}
}

static void ui_riga_sel(int x, int y, int w) {
	ui_rett(x, y, w, 11, K_SEL);
	ui_rett(x + 2, y + 3, 1, 5, K_GIALLO);
	ui_rett(x + 3, y + 4, 1, 3, K_GIALLO);
	ui_px(x + 4, y + 5, K_GIALLO);
}

/* potere e sentenza: barre laterali piu' piccole */
static void lato_barra(int x, const char *nome, int val, int col) {
	char b[8];
	int cx = x + 3;
	ui_pannello(cx - 8, 28, 16, 11);
	ui_testo_c(cx, 30, nome, col, 1);
	ui_barra_v(x, 41, 6, 49, val, MAX_RISORSA, col);
	ui_pannello(cx - 8, 91, 16, 11);
	sprintf(b, "%d", val);
	ui_testo_c(cx, 93, b, K_TESTO, 1);
}

static void disegna_barre_laterali(void) {
	lato_barra(5, "P", potere, K_GIALLO);
	lato_barra(229, "S", sentenza, K_TURCHESE);
}

static void ui_hud(void) {
	char b[32];
	ui_pannello(2, 2, 236, 23);

	sprintf(b, "TURNO %d", turno);
	ui_testo(8, 5, b, K_GIALLO, 1);
	ui_testo(62, 5, "PV", K_TESTO, 1);
	ui_barra(76, 5, 56, 7, pv_giocatore, PV_GIOCATORE, K_ROSSO);
	sprintf(b, "%d/%d", pv_giocatore < 0 ? 0 : pv_giocatore, PV_GIOCATORE);
	ui_testo(138, 5, b, K_TESTO, 1);
	if (dadi_bloccati > 0) sprintf(b, "DADI %d -%d", n_dadi, dadi_bloccati);
	else sprintf(b, "DADI %d", n_dadi);
	ui_testo_d(232, 5, b, K_TESTO, 1);

	sprintf(b, "DIFESA %d", difesa_somma());
	ui_testo(8, 15, b, K_BLU, 1);
	sprintf(b, "MAZZO %d", N_MAZZO - prossima);
	ui_testo_c(120, 15, b, K_GRIGIO, 1);
	ui_testo_d(232, 15, "START pausa", K_GRIGIO, 1);
}

static void disegna_dadi(void) {
	char b[64];
	int q = tirato ? n_attivi : dadi_disponibili();

	if (!attacco && !menu_aperto && turno > 1) {
		if (ultimo_danno > 0) {
			sprintf(b, "Turno nemico: -%d PV (difesa %d)", ultimo_danno, ultima_difesa);
		} else {
			sprintf(b, "Turno nemico: nessun danno");
		}
		const char *nota = (limite_difesa < MAX_DIFESA) ? "Difesa massima: 1 dado!" : 0;
		int w = ui_larghezza(b, 1);
		if (nota && ui_larghezza(nota, 1) > w) w = ui_larghezza(nota, 1);
		w += 16;
		ui_pannello(120 - w / 2, 26, w, nota ? 21 : 11);
		ui_testo_c(120, 28, b, K_TESTO, 1);
		if (nota) ui_testo_c(120, 38, nota, K_ROSSO, 1);
	}

	if (!tirato) {
		ui_pannello(60, 58, 120, dadi_bloccati > 0 ? 36 : 26);
		ui_testo_c(120, 64, "Tira i dadi", K_GIALLO, 1);
		ui_testo_c(120, 74, "premi A", K_TESTO, 1);
		if (dadi_bloccati > 0) {
			sprintf(b, dadi_bloccati == 1 ? "%d dado bloccato" : "%d dadi bloccati",
			        dadi_bloccati);
			ui_testo_c(120, 84, b, K_ROSSO, 1);
		}
	}

	if (attacco) {
		const Carta *c = prendi(campo[bersaglio]);
		ui_pannello(20, 30, 200, 56);
		fondo_carta(28, 40, K_BORDO);
		ui_testo(70, 36, c->nome, K_GIALLO, 1);
		ui_barra(70, 47, 90, 5, pv_campo[bersaglio], c->pv, K_ROSSO);
		sprintf(b, "PV %d/%d", pv_campo[bersaglio], c->pv);
		ui_testo(168, 45, b, K_TESTO, 1);
		ui_testo(70, 57, "Servono", K_GRIGIO, 1);
		for (int k = 0; k < c->n_cas; k++) {
			ui_casella(70 + CASELLA_P * k, 68, c->cas[k], riempita[k]);
		}
	}

	if (tirato && !attacco && menu_aperto) {
		costruisci_menu();
		int n = n_voci > 0 ? n_voci : 1;
		int w = 124;
		int h = n * 11 + 6;
		int cx = dado_x(selezionato, q) + 8;
		int mx = cx - w / 2;
		/* lascia libere le barre laterali (riquadri larghi 16 px per lato) */
		if (mx > 222 - w) mx = 222 - w;
		if (mx < 18) mx = 18;
		int my = 97 - h;
		ui_pannello(mx, my, w, h);
		if (n_voci == 0) {
			ui_testo_c(mx + w / 2, my + 8, "Nessuna azione", K_GRIGIO, 1);
		} else {
			for (int k = 0; k < n_voci; k++) {
				int ry = my + 3 + k * 11;
				char cnt[12];
				cnt[0] = 0;
				if (k == voce_sel) ui_riga_sel(mx + 3, ry, w - 6);
				ui_testo(mx + 12, ry + 2, NOME_VOCE[voci[k]],
				         k == voce_sel ? K_GIALLO : K_TESTO, 1);
				switch (voci[k]) {
				case AZX_RILANCIA: sprintf(cnt, "x%d", max_rilanci() - rilanci_usati); break;
				case AZX_PIU:
				case AZX_MENO: sprintf(cnt, "x%d", max_ritocchi() - ritocchi_usati); break;
				case AZX_DIFESA: sprintf(cnt, "%d/%d", difesa_conta(), limite_difesa); break;
				default: break;
				}
				if (cnt[0]) ui_testo_d(mx + w - 8, ry + 2, cnt, K_GRIGIO, 1);
			}
		}
		ui_freccia_giu(cx, 97, K_ORO);
	}

	if (tirato) {
		for (int i = 0; i < q; i++) {
			int col;
			char l = lettera_stato(i, &col);
			int cx = dado_x(i, q) + 8;
			if (i == selezionato) ui_freccia_su(cx, 121, K_ORO);
			if (l != ' ') {
				char s[2] = {l, 0};
				ui_rett(cx - 6, 127, 13, 11, K_PANNELLO);
				ui_cornice(cx - 6, 127, 13, 11, K_BORDO);
				ui_testo_c(cx, 129, s, col, 1);
			}
		}
	}

	if (attacco) {
		ui_aiuto("A|metti dado;B|annulla;Sx/Dx|dado");
	} else if (!tirato) {
		ui_aiuto("A|tira;R|carte;SELECT|fine turno");
	} else if (menu_aperto) {
		ui_aiuto("Su/Giu|voce;A|conferma;B|chiudi");
	} else {
		ui_aiuto("Sx/Dx|dado;A|menu;R|carte;SELECT|fine");
	}
}

/* disegna il menu "Attacca.../Vedi ciclo" sotto la carta selezionata */
static void disegna_menu_carta(void) {
	int n = n_voci_carta > 0 ? n_voci_carta : 1;
	int w = 94;
	int h = n * 11 + 6;
	/* dentro al riquadro dei dettagli, a destra: cosi' non copre le carte
	   (gli sprite starebbero sopra) ne' taglia il testo a sinistra */
	int x = 138;
	int y = 113;
	ui_pannello(x, y, w, h);
	for (int k = 0; k < n_voci_carta; k++) {
		int ry = y + 3 + k * 11;
		if (k == voce_carta) ui_riga_sel(x + 3, ry, w - 6);
		ui_testo(x + 12, ry + 2, NOME_VOCE_CARTA[voci_carta[k]],
		         k == voce_carta ? K_GIALLO : K_TESTO, 1);
	}
}

/* mostra l'intero ciclo della carta, passo per passo, con descrizione;
   il pannello e' piu' stretto per lasciare spazio allo sprite a lato */
static void disegna_vedi_ciclo(void) {
	const Carta *c = prendi(campo[cursore]);
	int n = c->n_az;
	int h = 24 + n * 10;
	int w = 184;
	int x = 4;
	int y = (160 - h) / 2;
	if (y < 8) y = 8;
	int pos = eta_campo[cursore] % n;
	char b[16];

	ui_pannello(x, y, w, h);
	fondo_carta(198, ciclo_sprite_y(), K_BORDO);
	ui_testo(x + 8, y + 5, c->nome, K_GIALLO, 1);
	sprintf(b, "PV %d/%d", pv_campo[cursore], c->pv);
	ui_testo_d(x + w - 8, y + 5, b, K_TESTO, 1);

	for (int i = 0; i < n; i++) {
		int ry = y + 17 + i * 10;
		int attivo = (i == pos);
		if (attivo) ui_rett(x + 6, ry - 1, w - 12, 10, K_SEL);
		char s[2] = {LETTERE[i], 0};
		ui_testo(x + 10, ry, s, attivo ? K_GIALLO : K_TESTO, 1);
		ui_testo(x + 22, ry, azione_desc(c->az[i]), attivo ? K_GIALLO : K_GRIGIO, 1);
	}
}

/* Tutte le carte in campo (fino a 10, cinque sopra e cinque sotto) mostrano
   solo icona e vita: i dettagli del ciclo si vedono soltanto per la carta
   selezionata, nel pannello sotto. Cosi' non se ne perde piu' nessuna. */
static void disegna_carte(void) {
	char b[32];

	for (int i = 0; i < n_campo; i++) {
		const Carta *c = prendi(campo[i]);
		int x = CARTA_X(i);
		int y = CARTA_Y(i);

		/* fondo chiaro dietro ogni icona (vedi fondo_carta); la carta
		   selezionata e' alzata di 3 px, quindi il fondo parte da y - 1 */
		fondo_carta(x, y - 1, i == cursore ? K_ORO : K_BARRA);

		ui_barra(x, y + 34, 32, 4, pv_campo[i], c->pv, K_ROSSO);
	}

	if (n_campo > 0) {
		const Carta *c = prendi(campo[cursore]);
		int len = c->n_az;
		int pos = eta_campo[cursore] % len;
		ui_pannello(4, 111, 232, 34);
		ui_testo(10, 113, c->nome, K_GIALLO, 1);
		ui_testo(10, 125, "Serve", K_GRIGIO, 1);
		for (int k = 0; k < c->n_cas; k++) {
			ui_casella(42 + CASELLA_P * k, 123, c->cas[k], 0);
		}
		/* PV, step e azione di turno stanno sotto al menu quando e' aperto */
		if (!carta_menu) {
			sprintf(b, "PV %d/%d", pv_campo[cursore], c->pv);
			ui_testo_d(230, 113, b, K_TESTO, 1);
			sprintf(b, "Step %d/%d", pos + 1, len);
			ui_testo_d(230, 125, b, K_GRIGIO, 1);
			sprintf(b, "%c: %s", LETTERE[pos], azione_desc(c->az[pos]));
			ui_testo(10, 136, b, K_GIALLO, 1);
		}
	}

	if (carta_menu) disegna_menu_carta();

	if (carta_menu) {
		ui_aiuto("Su/Giu|voce;A|conferma;B|chiudi");
	} else {
		ui_aiuto("Sx/Dx|carta;A|menu;L|dadi;SELECT|fine");
	}
}

/* Codex: scheda consultabile di tutte le carte del gioco (pool + Nucleo),
   con artwork a lato (disegnato come sprite da aggiorna_sprite) */
static void disegna_codex(void) {
	char b[48];
	int idx = (codex_cursore < N_POOL) ? codex_cursore : -1;
	const Carta *c = prendi(idx);
	const int px = 4, pw = 168;
	const int cx = px + pw / 2;
	const int maxw = pw - 16;

	ui_pannello(px, 10, pw, 130);

	ui_testo_c(cx, 14, c->nome, K_GIALLO, 1);

	sprintf(b, "PV %d", c->pv);
	ui_testo_c(cx, 25, b, K_TESTO, 1);

	ui_testo_c(cx, 36, "Servono", K_GRIGIO, 1);
	{
		int larg = c->n_cas * CASELLA_P - (CASELLA_P - CASELLA_W);
		int sx = cx - larg / 2;
		for (int k = 0; k < c->n_cas; k++) {
			ui_casella(sx + CASELLA_P * k, 45, c->cas[k], 0);
		}
	}

	ui_testo_c(cx, 61, "Ciclo", K_GRIGIO, 1);
	int ry = 72;
	for (int i = 0; i < c->n_az; i++) {
		sprintf(b, "%c: %s", LETTERE[i], azione_desc(c->az[i]));
		ry = ui_testo_wrap(cx, ry, maxw, b, K_GIALLO, 1, 1);
	}

	if (c->on_morte != OM_NESSUNA) {
		if (c->on_morte == OM_DANNO_SX) {
			sprintf(b, "Alla morte: %d danni a sx", c->om_val);
		} else if (c->on_morte == OM_DANNO_TUTTI) {
			sprintf(b, "Alla morte: %d danni a tutti", c->om_val);
		} else if (c->on_morte == OM_CARICA_POT) {
			sprintf(b, "Alla morte: Potere +%d", c->om_val);
		} else if (c->on_morte == OM_RIDUCI_POT) {
			sprintf(b, "Alla morte: Potere -%d", c->om_val);
		} else {
			sprintf(b, "Alla morte: -%d Potere/Sentenza", c->om_val);
		}
		ui_testo_wrap(cx, ry + 2, maxw, b, K_ROSSO, 1, 1);
	}

	ui_pannello(178, 10, 58, 100);
	fondo_carta(191, 44, K_BORDO);
	sprintf(b, "%d/%d", codex_cursore + 1, N_MAZZO);
	ui_testo_c(207, 15, b, K_GRIGIO, 1);

	ui_aiuto("Sx/Dx|carta;B|indietro");
}

static void disegna_pausa(void) {
	static const char *const V[N_VOCI_PAUSA] = {"Riprendi", "Opzioni", "Guida", "Riavvia partita", "Torna al titolo"};
	ui_pannello(50, 20, 140, 104);
	ui_testo_c(120, 26, "PAUSA", K_GIALLO, 2);
	ui_testo_c(120, 43, difficile ? "Modalita difficile" : "Modalita normale",
	           difficile ? K_ROSSO : K_GRIGIO, 1);
	for (int k = 0; k < N_VOCI_PAUSA; k++) {
		int ry = 54 + k * 12;
		if (k == voce_pausa) ui_riga_sel(55, ry, 130);
		ui_testo(70, ry + 2, V[k], k == voce_pausa ? K_GIALLO : K_TESTO, 1);
	}
	ui_aiuto("Su/Giu|voce;A|conferma;B|riprendi");
}

/* ---------- OPZIONI ---------- */
static const char *const PERCENTO[VOL_MAX + 1] = {"OFF", "12%", "25%", "50%", "100%"};

static void disegna_opzioni(void) {
	static const char *const NOMI[2] = {"Musica", "Effetti sonori"};
	ui_pannello(20, 14, 200, 120);
	ui_testo_c(120, 20, "OPZIONI", K_GIALLO, 2);
	for (int k = 0; k < 2; k++) {
		int v = (k == 0) ? au_vol_mus : au_vol_eff;
		int ry = 44 + k * 32;
		int sel = (opz_voce == k);
		if (sel) ui_cornice(26, ry - 3, 188, 29, K_ORO);
		ui_testo(34, ry, NOMI[k], sel ? K_GIALLO : K_TESTO, 1);
		ui_testo_d(206, ry, PERCENTO[v], v == 0 ? K_ROSSO : K_TESTO, 1);
		for (int t = 0; t < VOL_MAX; t++) {
			ui_rett(34 + t * 43, ry + 11, 39, 7, t < v ? K_VERDE : K_BARRA);
		}
	}
	{
		int ry = 110;
		if (opz_voce == 2) ui_riga_sel(76, ry, 88);
		ui_testo_c(120, ry + 2, "Indietro", opz_voce == 2 ? K_GIALLO : K_TESTO, 1);
	}
	ui_aiuto("Su/Giu|voce;Sx/Dx|volume;B|indietro");
}

/* ---------- GUIDA ---------- */
typedef struct {
	const char *titolo;
	const char *r[10]; /* righe di testo (0 = fine) */
} PaginaGuida;

/* pagine 3 e 4 mostrano le caselle disegnate: il testo sta nella funzione */
#define PAG_CASELLE 3
#define PAG_SOMME 4
#define N_PAG_GUIDA 8
static const PaginaGuida GUIDA[N_PAG_GUIDA] = {
	{"OBIETTIVO", {
		"Davanti a te c'e' una fila di carte",
		"nemiche. Sconfiggile tutte, fino al",
		"Doom Core: l'ultima carta del mazzo.",
		"",
		"Hai 20 PV: se arrivano a 0 perdi.",
		"A ogni turno tiri dei dadi e li usi per",
		"colpire le carte o per difenderti.",
		"Ogni carta sconfitta ti da' un dado",
		"in piu' (fino a 10).", 0}},
	{"IL TURNO", {
		"1  Premi A per tirare i dadi.",
		"2  Per ogni dado scegli: rilancia,",
		"    ritocca +1/-1, difesa o attacca.",
		"3  SELECT chiude il turno.",
		"4  Ogni carta in campo esegue il",
		"    passo del suo ciclo. Il danno totale",
		"    meno la tua difesa ti toglie PV.",
		"5  Arriva una nuova carta e si",
		"    ricomincia.", 0}},
	{"RILANCI, RITOCCHI, DIFESA", {
		"Rilancia: tira di nuovo quel dado",
		"(3 volte per turno).",
		"Aumenta / Diminuisci: +1 o -1 al",
		"dado (3 volte per turno). Un dado gia'",
		"rilanciato o ritoccato resta com'e'.",
		"Difesa: fino a 4 dadi; la loro somma",
		"viene tolta al danno del turno.",
		"Modalita difficile: solo 2 rilanci e",
		"2 ritocchi per turno.", 0}},
	{"CASELLE: I NUMERI", {0}},
	{"CASELLE: LE SOMME", {0}},
	{"COME SI ATTACCA", {
		"Dal menu di un dado (A) scegli Attacca",
		"e poi la carta; oppure dalla schermata",
		"carte (R) apri il menu della carta e",
		"scegli Attacca. Poi assegna i dadi:",
		"ognuno va nella prima casella adatta.",
		"Quando tutte le caselle sono piene la",
		"carta perde 1 PV e quei dadi sono",
		"spesi per il turno. B annulla.", 0}},
	{"CICLO, POTERE, SENTENZA", {
		"Ogni carta ha un ciclo: A, B, C...",
		"A ogni turno nemico esegue il passo",
		"successivo, poi ricomincia. Nel menu",
		"della carta, Vedi ciclo li mostra.",
		"Potere (P) e Sentenza (S) sono le due",
		"barre ai lati: alcune carte le fanno",
		"crescere e ne usano il valore come",
		"danno. Alla morte alcune carte hanno",
		"un effetto (vedi il Codex).", 0}},
	{"DADI E COMANDI", {
		"Sotto ai dadi: R = rilanciato,",
		"+ = ritoccato, D = in difesa,",
		"* = assegnato a un attacco,",
		"X = gia' usato in questo turno.",
		"",
		"A tira / menu     B indietro",
		"R carte     L dadi     SELECT fine turno",
		"START pausa", 0}},
};

static void guida_casella(int y, Casella c, const char *testo) {
	ui_casella(16, y, c, 0);
	ui_testo_wrap(48, y + 2, 176, testo, K_TESTO, 1, 0);
}

static void disegna_guida(void) {
	char b[8];
	const PaginaGuida *g = &GUIDA[guida_pag];
	ui_pannello(4, 6, 232, 136);
	ui_testo_c(120, 12, g->titolo, K_GIALLO, 1);
	sprintf(b, "%d/%d", guida_pag + 1, N_PAG_GUIDA);
	ui_testo_d(228, 12, b, K_GRIGIO, 1);
	if (guida_pag == PAG_CASELLE) {
		static const Casella C4 = {'=', 4, 0}, C45 = {'o', 4, 5}, CM = {'<', 3, 0},
		                     CP = {'>', 3, 0}, CU = {'~', 0, 0};
		guida_casella(28, C4, "Un dado che vale esattamente 4");
		guida_casella(44, C45, "Un dado che vale 4 oppure 5");
		guida_casella(60, CM, "Un dado minore di 3");
		guida_casella(76, CP, "Un dado maggiore di 3");
		guida_casella(92, CU, "Due dadi uguali tra loro (valore libero)");
		ui_testo_wrap(120, 120, 212, "Ogni carta mostra le caselle che servono.", K_GRIGIO, 1, 1);
	} else if (guida_pag == PAG_SOMME) {
		static const Casella A = {'+', 0, 0}, B = {'+', 1, 0}, S = {'+', 2, 0};
		ui_casella(16, 28, A, 0);
		ui_casella(44, 28, B, 0);
		ui_casella(72, 28, S, 0);
		const char *const R[] = {
			"a e b sono due dadi qualsiasi; il",
			"terzo dado deve valere a+b.",
			"Esempio: 2 e 3, poi un 5.",
			"a e b valgono al massimo 5 e la loro",
			"somma al massimo 6.",
			"I dadi si assegnano da soli alla prima",
			"casella adatta, quindi conviene",
			"metterli nell'ordine giusto.", 0};
		for (int k = 0; R[k]; k++) ui_testo(14, 46 + k * 10, R[k], K_TESTO, 1);
	} else {
		for (int k = 0; g->r[k]; k++) ui_testo(14, 26 + k * 10, g->r[k], K_TESTO, 1);
	}
	ui_aiuto("Sx/Dx|pagina;B|indietro");
}

static void disegna_conferma(void) {
	char b[40];
	ui_pannello(30, 44, 180, 48);
	ui_testo_c(120, 52, "Finire il turno?", K_GIALLO, 1);
	if (!tirato) {
		ui_testo_c(120, 66, "Non hai ancora tirato i dadi", K_TESTO, 1);
	} else {
		int liberi = 0;
		for (int i = 0; i < n_attivi; i++) {
			if (stato_dado[i] == 0) liberi++;
		}
		sprintf(b, "Dadi ancora liberi: %d", liberi);
		ui_testo_c(120, 66, b, K_TESTO, 1);
		sprintf(b, "Difesa attuale: %d", difesa_somma());
		ui_testo_c(120, 77, b, K_BLU, 1);
	}
	ui_aiuto("A|conferma;B|annulla");
}

static void disegna_pannelli(void) {
	ui_pulisci();

	if (opzioni) {
		disegna_opzioni();
	} else if (guida) {
		disegna_guida();
	} else if (!iniziata) {
		if (codex) {
			disegna_codex();
		} else {
			char voce_diff[32];
			sprintf(voce_diff, "Difficolta: < %s >", difficile ? "Difficile" : "Normale");
			const char *const VOCE_TITOLO[N_VOCI_TITOLO] = {"Nuova Partita", voce_diff, "Codex", "Guida", "Opzioni"};

			const int dy = 12; /* riquadro centrato in verticale */
			ui_pannello(14, 14 + dy, 212, 114);

			/* titolo in rilievo su due righe (ombra + luce + colore pieno) */
			ui_testo_c(122, 26 + dy, "DOOM", K_OMBRA, 3);
			ui_testo_c(118, 22 + dy, "DOOM", K_LUCE, 3);
			ui_testo_c(120, 24 + dy, "DOOM", K_GIALLO, 3);

			ui_testo_c(121, 53 + dy, "MACHINE", K_OMBRA, 2);
			ui_testo_c(119, 51 + dy, "MACHINE", K_LUCE, 2);
			ui_testo_c(120, 52 + dy, "MACHINE", K_ROSSO, 2);

			for (int k = 0; k < N_VOCI_TITOLO; k++) {
				int ry = 68 + dy + k * 11;
				/* la barra luminosa e' larga quanto la scritta (piu' un
				   margine), cosi' la ingloba sempre per intero */
				int mw = ui_larghezza(VOCE_TITOLO[k], 1) + 18;
				if (k == titolo_voce) ui_riga_sel(120 - mw / 2, ry, mw);
				ui_testo_c(120, ry + 2, VOCE_TITOLO[k],
				           k == titolo_voce ? K_GIALLO : (k == 1 && difficile ? K_ROSSO : K_TESTO), 1);
			}
		}
	} else if (vinto || perso) {
		ui_pannello(40, 48, 160, 64);
		ui_testo_c(120, 62, vinto ? "HAI VINTO!" : "HAI PERSO",
		           vinto ? K_VERDE : K_ROSSO, 2);
		if (difficile) ui_testo_c(120, 78, "Modalita difficile", K_ROSSO, 1);
		ui_testo_c(120, 90, "START: nuova partita", K_TESTO, 1);
	} else if (pausa) {
		ui_hud();
		disegna_pausa();
	} else if (conferma) {
		ui_hud();
		disegna_conferma();
	} else {
		ui_hud();
		if (!vedi_ciclo) disegna_barre_laterali();
		if (vista == 0) disegna_dadi();
		else if (vedi_ciclo) disegna_vedi_ciclo();
		else disegna_carte();
	}

}

_Static_assert(SLOT_SPRITE + MAX_DADI <= TR_SPRITE, "TR_SPRITE troppo piccolo");

/* schermata corrente (0 titolo, 1 codex, 2 dadi, 3 carte, 4 ciclo, 6 fine
   partita, 7 opzioni, 8 guida, 9 riquadri sovrapposti come pausa e conferma) */
static int schermata_id(void) {
	if (opzioni) return 7;
	if (guida) return 8;
	if (!iniziata) return codex ? 1 : 0;
	if (vinto || perso) return 6;
	if (pausa || conferma) return 9;
	if (vista == 0) return 2;
	return vedi_ciclo ? 4 : 3;
}

static int schermata_precedente = -1;

/* +1 = la nuova schermata entra da destra, -1 da sinistra, 0 = niente
   scorrimento (primo disegno, stessa schermata, riquadri sovrapposti) */
static int direzione_scorrimento(int da, int a) {
	static const int profondita[] = {0, 1, 1, 2, 3, 0, 4, 1, 1};
	if (da < 0 || da == a || da == 9 || a == 9) return 0;
	if (da == 6) return 1; /* da fine partita si va sempre avanti (nuova partita) */
	return profondita[a] >= profondita[da] ? 1 : -1;
}

static void disegna(void) {
	int id = schermata_id();
	int dir = direzione_scorrimento(schermata_precedente, id);
	schermata_precedente = id;

	if (dir) tr_leggi_oam(oam_a);
	aggiorna_sprite();
	if (dir) {
		tr_leggi_oam(oam_b);
		tr_scrivi_oam(oam_a); /* finche' la nuova schermata non entra restano gli sprite vecchi */
	}
	disegna_pannelli();
	if (dir) transizione(dir);
	else ui_carica();
}

static void nuova_partita(void) {
	prepara_mazzo();
	n_dadi = 5;
	n_attivi = 5;
	for (int k = 0; k < 3; k++) entra_carta();
	iniziata = 1;
	vinto = 0;
	perso = 0;
	pausa = 0;
	voce_pausa = 0;
	conferma = 0;
	turno = 1;
	tirato = 0;
	vista = 0;
	attacco = 0;
	menu_aperto = 0;
	da_menu = 0;
	carta_menu = 0;
	vedi_ciclo = 0;
	voce_carta = 0;
	cursore = 0;
	selezionato = 0;
	voce_sel = 0;
	rilanci_usati = 0;
	ritocchi_usati = 0;
	pv_giocatore = PV_GIOCATORE;
	potere = 0;
	sentenza = 0;
	ultimo_danno = 0;
	ultima_difesa = 0;
	limite_difesa = MAX_DIFESA;
	prossimo_limite_1 = 0;
	dadi_bloccati = 0;
	prossimo_blocco = 0;
}

int main(void) {
	irqInit();
	irqEnable(IRQ_VBLANK);
	au_init();
	au_musica();

	*(volatile unsigned short *)0x04000000 = 0x0080;
	sprite_nascondi_tutti();
	carica_grafica();
	carica_dadi();
	carica_tavolo();
	ui_init();
	disegna();
	*(volatile unsigned short *)0x04000000 = (1 << 6) | (1 << 8) | (1 << 9) | (1 << 12);

	unsigned int frame = 0;
	rng_seed(12345);

	while (1) {
		VBlankIntrWait();
		frame++;
		rng32();
		au_aggiorna();
		scanKeys();
		u16 premuti = keysDown();
		int cambiato = 0;

		if (opzioni) {
			if (premuti & KEY_UP) {
				opz_voce = (opz_voce + 2) % 3;
				CAMPIONE(CAMP_SPOSTA);
				cambiato = 1;
			}
			if (premuti & KEY_DOWN) {
				opz_voce = (opz_voce + 1) % 3;
				CAMPIONE(CAMP_SPOSTA);
				cambiato = 1;
			}
			int delta = ((premuti & KEY_RIGHT) ? 1 : 0) - ((premuti & KEY_LEFT) ? 1 : 0);
			if ((premuti & KEY_A) && opz_voce < 2) delta = 1; /* A fa girare i livelli */
			if (delta && opz_voce == 0) {
				int v = au_vol_mus + delta;
				if (v > VOL_MAX) v = (premuti & KEY_A) ? 0 : VOL_MAX;
				if (v < 0) v = 0;
				au_set_musica(v);
				cambiato = 1;
			} else if (delta && opz_voce == 1) {
				int v = au_vol_eff + delta;
				if (v > VOL_MAX) v = (premuti & KEY_A) ? 0 : VOL_MAX;
				if (v < 0) v = 0;
				au_set_effetti(v);
				CAMPIONE(CAMP_SELEZIONE); /* anteprima al nuovo volume */
				cambiato = 1;
			}
			if ((premuti & KEY_B) || ((premuti & KEY_A) && opz_voce == 2)) {
				opzioni = 0;
				CAMPIONE(CAMP_DESELEZIONE);
				cambiato = 1;
			}
			if (cambiato) disegna();
			continue;
		}
		if (guida) {
			if (premuti & (KEY_RIGHT | KEY_DOWN | KEY_R)) {
				guida_pag = (guida_pag + 1) % N_PAG_GUIDA;
				CAMPIONE(CAMP_SPOSTA);
				cambiato = 1;
			}
			if (premuti & (KEY_LEFT | KEY_UP | KEY_L)) {
				guida_pag = (guida_pag + N_PAG_GUIDA - 1) % N_PAG_GUIDA;
				CAMPIONE(CAMP_SPOSTA);
				cambiato = 1;
			}
			if (premuti & (KEY_B | KEY_SELECT)) {
				guida = 0;
				CAMPIONE(CAMP_DESELEZIONE);
				cambiato = 1;
			}
			if (cambiato) disegna();
			continue;
		}

		if (!iniziata) {
			if (codex) {
				if (premuti & (KEY_LEFT | KEY_UP)) {
					codex_cursore = (codex_cursore + N_MAZZO - 1) % N_MAZZO;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & (KEY_RIGHT | KEY_DOWN)) {
					codex_cursore = (codex_cursore + 1) % N_MAZZO;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & (KEY_B | KEY_SELECT)) {
					codex = 0;
					CAMPIONE(CAMP_DESELEZIONE);
					cambiato = 1;
				}
			} else {
				if (premuti & KEY_UP) {
					titolo_voce = (titolo_voce + N_VOCI_TITOLO - 1) % N_VOCI_TITOLO;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & KEY_DOWN) {
					titolo_voce = (titolo_voce + 1) % N_VOCI_TITOLO;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (titolo_voce == 1 && (premuti & (KEY_LEFT | KEY_RIGHT))) {
					difficile = !difficile;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & KEY_A) { /* sul titolo START non fa nulla: si sceglie solo con A */
					if (titolo_voce == 0) {
						rng_seed(frame ^ 0xA5A5A5A5u);
						nuova_partita();
						SUONA(S_AVVIO);
					} else if (titolo_voce == 1) {
						difficile = !difficile;
						CAMPIONE(CAMP_SPOSTA);
					} else if (titolo_voce == 2) {
						codex = 1;
						codex_cursore = 0;
						CAMPIONE(CAMP_SELEZIONE);
					} else if (titolo_voce == 3) {
						guida = 1;
						guida_pag = 0;
						CAMPIONE(CAMP_SELEZIONE);
					} else {
						opzioni = 1;
						opz_voce = 0;
						CAMPIONE(CAMP_SELEZIONE);
					}
					cambiato = 1;
				}
			}
			if (cambiato) disegna();
			continue;
		}
		if (vinto || perso) {
			if (premuti & KEY_START) {
				rng_seed(frame ^ 0x5A5A5A5Au);
				nuova_partita();
				SUONA(S_AVVIO);
				cambiato = 1;
			}
			if (cambiato) disegna();
			continue;
		}

		if (pausa) {
			if (premuti & KEY_UP) {
				voce_pausa = (voce_pausa + N_VOCI_PAUSA - 1) % N_VOCI_PAUSA;
				CAMPIONE(CAMP_SPOSTA);
				cambiato = 1;
			}
			if (premuti & KEY_DOWN) {
				voce_pausa = (voce_pausa + 1) % N_VOCI_PAUSA;
				CAMPIONE(CAMP_SPOSTA);
				cambiato = 1;
			}
			if (premuti & (KEY_B | KEY_START)) {
				pausa = 0;
				SUONA(S_RIPRENDI);
				cambiato = 1;
			} else if (premuti & KEY_A) {
				if (voce_pausa == 0) {
					pausa = 0;
					SUONA(S_RIPRENDI);
				} else if (voce_pausa == 1) {
					opzioni = 1;
					opz_voce = 0;
					CAMPIONE(CAMP_SELEZIONE);
				} else if (voce_pausa == 2) {
					guida = 1;
					guida_pag = 0;
					CAMPIONE(CAMP_SELEZIONE);
				} else if (voce_pausa == 3) {
					nuova_partita();
					SUONA(S_AVVIO);
				} else {
					iniziata = 0;
					pausa = 0;
					CAMPIONE(CAMP_DESELEZIONE);
				}
				cambiato = 1;
			}
			if (cambiato) disegna();
			continue;
		}

		if (conferma) {
			if (premuti & KEY_A) {
				fine_turno();
				cambiato = 1;
			} else if (premuti & (KEY_B | KEY_SELECT)) {
				conferma = 0;
				CAMPIONE(CAMP_DESELEZIONE);
				cambiato = 1;
			}
			if (cambiato) disegna();
			continue;
		}

		if (premuti & KEY_START) {
			pausa = 1;
			voce_pausa = 0;
			SUONA(S_PAUSA);
			disegna();
			continue;
		}
		if (premuti & KEY_SELECT) {
			if (attacco || menu_aperto || carta_menu || vedi_ciclo) {
				CAMPIONE(CAMP_ERRORE);
			} else {
				conferma = 1;
				CAMPIONE(CAMP_SELEZIONE);
				cambiato = 1;
			}
		} else if (premuti & (KEY_L | KEY_R)) {
			if (attacco || menu_aperto || carta_menu || vedi_ciclo) {
				CAMPIONE(CAMP_ERRORE);
			} else {
				vista = !vista;
				da_menu = 0;
				if (premuti & KEY_L) CAMPIONE(CAMP_SCHEDA); else CAMPIONE(CAMP_SCHEDA);
				cambiato = 1;
			}
		}
		if (cambiato) {
			disegna();
			continue;
		}

		if (vista == 0) {
			if (!tirato) {
				if (premuti & KEY_A) {
					tira();
					CAMPIONE(CAMP_DADI);
					cambiato = 1;
				}
			} else if (attacco) {
				if (premuti & KEY_LEFT) {
					selezionato = (selezionato + n_attivi - 1) % n_attivi;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & KEY_RIGHT) {
					selezionato = (selezionato + 1) % n_attivi;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & KEY_A) {
					esito_assegna(assegna());
					cambiato = 1;
				} else if (premuti & KEY_B) {
					annulla_attacco();
					CAMPIONE(CAMP_DESELEZIONE);
					cambiato = 1;
				}
			} else if (menu_aperto) {
				costruisci_menu();
				if ((premuti & KEY_UP) && n_voci > 0) {
					voce_sel = (voce_sel + n_voci - 1) % n_voci;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if ((premuti & KEY_DOWN) && n_voci > 0) {
					voce_sel = (voce_sel + 1) % n_voci;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & KEY_A) {
					esegui_voce();
					cambiato = 1;
				} else if (premuti & KEY_B) {
					menu_aperto = 0;
					CAMPIONE(CAMP_DESELEZIONE);
					cambiato = 1;
				}
			} else {
				if (premuti & KEY_LEFT) {
					selezionato = (selezionato + n_attivi - 1) % n_attivi;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & KEY_RIGHT) {
					selezionato = (selezionato + 1) % n_attivi;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & KEY_A) {
					costruisci_menu();
					if (n_voci > 0) {
						menu_aperto = 1;
						voce_sel = 0;
						CAMPIONE(CAMP_SELEZIONE);
					} else {
						CAMPIONE(CAMP_ERRORE);
					}
					cambiato = 1;
				}
			}
		} else {
			/* ----- scheda carte ----- */
			if (vedi_ciclo) {
				if (premuti & KEY_B) {
					vedi_ciclo = 0;
					CAMPIONE(CAMP_DESELEZIONE);
					cambiato = 1;
				}
			} else if (carta_menu) {
				costruisci_menu_carta();
				if ((premuti & KEY_UP) && n_voci_carta > 0) {
					voce_carta = (voce_carta + n_voci_carta - 1) % n_voci_carta;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if ((premuti & KEY_DOWN) && n_voci_carta > 0) {
					voce_carta = (voce_carta + 1) % n_voci_carta;
					CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & KEY_A) {
					if (n_voci_carta > 0) {
						if (voci_carta[voce_carta] == CZ_ATTACCA) {
							attacco = 1;
							bersaglio = cursore;
							for (int k = 0; k < MAX_CAS; k++) riempita[k] = 0;
							carta_menu = 0;
							vista = 0; /* si scelgono i dadi: subito sulla schermata dadi */
							CAMPIONE(CAMP_SELEZIONE);
						} else {
							vedi_ciclo = 1;
							carta_menu = 0;
							CAMPIONE(CAMP_SELEZIONE);
						}
					} else {
						CAMPIONE(CAMP_ERRORE);
					}
					cambiato = 1;
				} else if (premuti & KEY_B) {
					carta_menu = 0;
					CAMPIONE(CAMP_DESELEZIONE);
					cambiato = 1;
				}
			} else {
				if ((premuti & (KEY_LEFT | KEY_UP)) && n_campo > 0) {
					cursore = (cursore + n_campo - 1) % n_campo;
					if (premuti & KEY_UP) CAMPIONE(CAMP_SPOSTA); else CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if ((premuti & (KEY_RIGHT | KEY_DOWN)) && n_campo > 0) {
					cursore = (cursore + 1) % n_campo;
					if (premuti & KEY_DOWN) CAMPIONE(CAMP_SPOSTA); else CAMPIONE(CAMP_SPOSTA);
					cambiato = 1;
				}
				if (premuti & KEY_B) {
					da_menu = 0;
					vista = 0;
					CAMPIONE(CAMP_DESELEZIONE);
					cambiato = 1;
				} else if (premuti & KEY_A) {
					if (n_campo == 0) {
						CAMPIONE(CAMP_ERRORE);
					} else if (da_menu) {
						attacco = 1;
						bersaglio = cursore;
						for (int k = 0; k < MAX_CAS; k++) riempita[k] = 0;
						vista = 0;
						esito_assegna(assegna());
						da_menu = 0;
					} else {
						costruisci_menu_carta();
						if (n_voci_carta > 0) {
							carta_menu = 1;
							voce_carta = 0;
							CAMPIONE(CAMP_SELEZIONE);
						} else {
							CAMPIONE(CAMP_ERRORE);
						}
					}
					cambiato = 1;
				}
			}
		}

		if (cambiato) disegna();
	}
}