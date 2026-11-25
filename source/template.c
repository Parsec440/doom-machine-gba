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

#define MAX_DADI 10
#define MAX_RILANCI 3
#define MAX_RITOCCHI 3
#define MAX_DIFESA 4
#define MAX_CAMPO 10
#define SLOT_SPRITE 5
#define MAX_CAS 3
#define MAX_AZ 6
#define PV_GIOCATORE 20
#define MAX_RISORSA 12

#define DADO_Y 107
#define CARTA_Y 36
#define CARTA_X(i) (10 + 46 * (i))

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
} TipoAzione;
typedef struct {
	TipoAzione tipo;
	int val;
} Azione;

/* effetto scatenato quando la carta viene sconfitta */
typedef enum { OM_NESSUNA, OM_DANNO_SX, OM_DANNO_TUTTI, OM_RIDUCI_PS } OnMorte;

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
	{"Scrap Sentinel", 3, 2, {{'=', 6}, {'=', 6}}, 2,
	 {{AZ_NULLA}, {AZ_DANNO_SP}}},
	{"Frost Relay", 3, 2, {{'=', 2}, {'=', 3}}, 3,
	 {{AZ_NULLA}, {AZ_CARICA_SP}, {AZ_DANNO_SP}}},
	{"Gilt Capacitor", 4, 2, {{'=', 3}, {'>', 3}}, 3,
	 {{AZ_NULLA}, {AZ_CARICA_SP}, {AZ_CARICA_SP}}},
	{"Coolant Drip", 2, 1, {{'>', 4}}, 4,
	 {{AZ_NULLA}, {AZ_CARICA_SP}, {AZ_DANNO_SP}, {AZ_CARICA_SP}}},
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
};
#define N_POOL 9
static const Carta NUCLEO = {"Doom Core", 4, 3, {{'=', 6}, {'=', 6}, {'=', 6}}, 4,
                              {{AZ_NULLA}, {AZ_DANNO_SP}, {AZ_CARICA_SP}, {AZ_CARICA_SP}}};
#define N_MAZZO (N_POOL + 1)

static const Carta *prendi(int idx) {
	return idx < 0 ? &NUCLEO : &POOL[idx];
}

static int tile_carta(int idx) {
	return (idx < 0 ? N_POOL : idx) * 16;
}

static int casella_ok(Casella c, int v) {
	if (c.tipo == '=') return v == c.val;
	if (c.tipo == 'o') return v == c.val || v == c.val2;
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
	}
	return "";
}

static int mazzo[N_MAZZO];
static int prossima = 0;
static int campo[MAX_CAMPO];
static int pv_campo[MAX_CAMPO];
static int eta_campo[MAX_CAMPO];
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
	if (ritocchi_usati >= MAX_RITOCCHI) return;
	if (stato_dado[selezionato] != 0) return;
	if (rilanciato[selezionato] || ritoccato[selezionato]) return;
	int nuovo = dadi[selezionato] + delta;
	if (nuovo < 1 || nuovo > 6) return;
	dadi[selezionato] = nuovo;
	ritoccato[selezionato] = 1;
	ritocchi_usati++;
}

static void rilancia(void) {
	if (rilanci_usati >= MAX_RILANCI) return;
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
	n_attivi = n_dadi;
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
	else if (r == 1) SUONA(S_OK);
	else SUONA(S_ERRORE);
}

static void costruisci_menu(void) {
	n_voci = 0;
	if (!tirato || attacco || vista != 0) return;
	int s = stato_dado[selezionato];
	if (s == 3) {
		voci[n_voci++] = AZX_TOGLI;
	} else if (s == 0) {
		int intatto = !rilanciato[selezionato] && !ritoccato[selezionato];
		if (intatto && rilanci_usati < MAX_RILANCI) voci[n_voci++] = AZX_RILANCIA;
		if (intatto && ritocchi_usati < MAX_RITOCCHI) {
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
		SUONA(S_ERRORE);
		return;
	}
	switch (voci[voce_sel]) {
	case AZX_RILANCIA: rilancia(); SUONA(S_DADO); break;
	case AZX_PIU: ritocca(+1); SUONA(S_RITOCCO); break;
	case AZX_MENO: ritocca(-1); SUONA(S_RITOCCO); break;
	case AZX_DIFESA: metti_difesa(); SUONA(S_DIFESA); break;
	case AZX_TOGLI: togli_difesa(); SUONA(S_INDIETRO); break;
	case AZX_ATTACCA:
		da_menu = 1;
		vista = 1;
		SUONA(S_OK);
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

static void turno_nemici(void) {
	int danno = 0;
	for (int i = 0; i < n_campo; i++) {
		const Carta *c = prendi(campo[i]);
		int pos = eta_campo[i] % c->n_az;
		switch (c->az[pos].tipo) {
		case AZ_NULLA: break;
		case AZ_DANNO_SP: danno += potere + sentenza; break;
		case AZ_CARICA_SP: potere++; sentenza++; break;
		case AZ_DANNO_SX: if (i > 0) danno += pv_campo[i - 1]; break;
		case AZ_DANNO_FISSO: danno += c->az[pos].val; break;
		case AZ_LIMITA_DIFESA: prossimo_limite_1 = 1; break;
		case AZ_DANNO_POT: danno += potere; break;
		case AZ_CARICA_POT: potere++; break;
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
	tirato = 0;
	turno++;
	for (int i = 0; i < n_campo; i++) eta_campo[i]++;
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
static void aggiorna_sprite(void) {
	for (int s = 0; s < SLOT_SPRITE + MAX_DADI; s++) sprite_nascondi(s);

	if (!iniziata) {
		for (int i = 0; i < 4; i++) sprite_set(i, 38 + 44 * i, 108, tile_carta(i));
		return;
	}
	if (vinto || perso) return;
	if (pausa || conferma) return;

	if (vista == 1) {
		if (vedi_ciclo) {
			/* nella vista "ciclo" mostra solo lo sprite della carta in esame,
			   spostato di lato rispetto al pannello */
			const Carta *c = prendi(campo[cursore]);
			int n = c->n_az;
			int h = 24 + n * 10;
			int py = (160 - h) / 2;
			if (py < 8) py = 8;
			int sy = py + h / 2 - 16;
			if (sy < 2) sy = 2;
			if (sy > 128) sy = 128;
			sprite_set(0, 198, sy, tile_carta(campo[cursore]));
		} else {
			for (int i = 0; i < n_campo && i < SLOT_SPRITE; i++) {
				int y = (i == cursore) ? CARTA_Y - 3 : CARTA_Y;
				sprite_set(i, CARTA_X(i), y, tile_carta(campo[i]));
			}
		}
	} else {
		if (attacco) sprite_set(0, 28, 40, tile_carta(campo[bersaglio]));
		int q = tirato ? n_attivi : n_dadi;
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

static void ui_casella(int x, int y, Casella c, int piena) {
	char t[8];
	if (c.tipo == '=') sprintf(t, "%d", c.val);
	else if (c.tipo == 'o') sprintf(t, "%d/%d", c.val, c.val2);
	else sprintf(t, ">%d", c.val);
	ui_rett(x, y, 22, 11, piena ? K_VERDE : K_BARRA);
	ui_cornice(x, y, 22, 11, K_ORO);
	ui_testo_c(x + 11, y + 2, t, piena ? K_PANNELLO : K_TESTO, 1);
}

/* riquadro dei comandi, piu' piccolo e compatto */
static void ui_suggerimenti(const char *r1, const char *r2) {
	ui_pannello(2, 145, 236, 15);
	ui_testo_c(120, 147, r1, K_TESTO, 1);
	if (r2[0]) ui_testo_c(120, 154, r2, K_GRIGIO, 1);
}

static void ui_riga_sel(int x, int y, int w) {
	ui_rett(x, y, w, 11, K_SEL);
	ui_rett(x + 2, y + 3, 1, 5, K_GIALLO);
	ui_rett(x + 3, y + 4, 1, 3, K_GIALLO);
	ui_px(x + 4, y + 5, K_GIALLO);
}

/* potere e sentenza: barre laterali piu' piccole */
static void disegna_barre_laterali(void) {
	char b[8];
	int y = 40, h = 56, w = 6;

	ui_barra_v(3, y, w, h, potere, MAX_RISORSA, K_GIALLO);
	ui_testo_c(3 + w / 2, y - 8, "P", K_GIALLO, 1);
	sprintf(b, "%d", potere);
	ui_testo_c(3 + w / 2, y + h + 2, b, K_TESTO, 1);

	ui_barra_v(231, y, w, h, sentenza, MAX_RISORSA, K_TURCHESE);
	ui_testo_c(231 + w / 2, y - 8, "S", K_TURCHESE, 1);
	sprintf(b, "%d", sentenza);
	ui_testo_c(231 + w / 2, y + h + 2, b, K_TESTO, 1);
}

static void ui_hud(void) {
	char b[32];
	ui_pannello(2, 2, 236, 23);

	sprintf(b, "TURNO %d", turno);
	ui_testo(8, 5, b, K_GIALLO, 1);
	ui_testo(70, 5, "PV", K_TESTO, 1);
	ui_barra(84, 5, 60, 7, pv_giocatore, PV_GIOCATORE, K_ROSSO);
	sprintf(b, "%d/%d", pv_giocatore < 0 ? 0 : pv_giocatore, PV_GIOCATORE);
	ui_testo(150, 5, b, K_TESTO, 1);
	sprintf(b, "DADI %d", n_dadi);
	ui_testo_d(232, 5, b, K_TESTO, 1);

	sprintf(b, "DIFESA %d", difesa_somma());
	ui_testo(8, 15, b, K_BLU, 1);
}

static void disegna_dadi(void) {
	char b[64];
	int q = tirato ? n_attivi : n_dadi;

	if (!attacco && turno > 1) {
		if (ultimo_danno > 0) {
			sprintf(b, "Turno nemico: -%d PV (difesa %d)", ultimo_danno, ultima_difesa);
		} else {
			sprintf(b, "Turno nemico: nessun danno");
		}
		if (limite_difesa < MAX_DIFESA) strcat(b, " - difesa max 1 dado!");
		int w = ui_larghezza(b, 1) + 14;
		ui_pannello(120 - w / 2, 26, w, 10);
		ui_testo_c(120, 28, b, K_TESTO, 1);
	}

	if (!tirato) {
		ui_pannello(60, 58, 120, 26);
		ui_testo_c(120, 64, "Tira i dadi", K_GIALLO, 1);
		ui_testo_c(120, 74, "premi A", K_TESTO, 1);
	}

	if (attacco) {
		const Carta *c = prendi(campo[bersaglio]);
		ui_pannello(20, 30, 200, 56);
		ui_testo(70, 36, c->nome, K_GIALLO, 1);
		ui_barra(70, 47, 90, 5, pv_campo[bersaglio], c->pv, K_ROSSO);
		sprintf(b, "PV %d/%d", pv_campo[bersaglio], c->pv);
		ui_testo(168, 45, b, K_TESTO, 1);
		ui_testo(70, 57, "Servono", K_GRIGIO, 1);
		for (int k = 0; k < c->n_cas; k++) {
			ui_casella(70 + 26 * k, 68, c->cas[k], riempita[k]);
		}
	}

	if (tirato && !attacco && menu_aperto) {
		costruisci_menu();
		int n = n_voci > 0 ? n_voci : 1;
		int w = 104;
		int h = n * 11 + 6;
		int cx = dado_x(selezionato, q) + 8;
		int mx = cx - w / 2;
		if (mx < 2) mx = 2;
		if (mx > 238 - w) mx = 238 - w;
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
				case AZX_RILANCIA: sprintf(cnt, "x%d", MAX_RILANCI - rilanci_usati); break;
				case AZX_PIU:
				case AZX_MENO: sprintf(cnt, "x%d", MAX_RITOCCHI - ritocchi_usati); break;
				case AZX_DIFESA: sprintf(cnt, "%d/%d", difesa_conta(), limite_difesa); break;
				default: break;
				}
				if (cnt[0]) ui_testo_d(mx + w - 6, ry + 2, cnt, K_GRIGIO, 1);
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
				ui_testo_c(cx, 129, s, col, 1);
			}
		}
	}

	if (attacco) {
		ui_suggerimenti("A metti il dado  B annulla", "Sx/Dx dado");
	} else if (!tirato) {
		ui_suggerimenti("A tira  R carte  SELECT fine", "START pausa");
	} else if (menu_aperto) {
		ui_suggerimenti("Su/Giu voce  A conferma  B chiudi", "");
	} else {
		ui_suggerimenti("Sx/Dx dado  A azioni  R carte", "SELECT fine  START pausa");
	}
}

/* disegna il menu "Attacca.../Vedi ciclo" sotto la carta selezionata */
static void disegna_menu_carta(void) {
	int n = n_voci_carta > 0 ? n_voci_carta : 1;
	int w = 90;
	int h = n * 11 + 6;
	int x = CARTA_X(cursore) + 16 - w / 2;
	if (x < 2) x = 2;
	if (x > 238 - w) x = 238 - w;
	int y = CARTA_Y + 34;
	ui_pannello(x, y, w, h);
	for (int k = 0; k < n_voci_carta; k++) {
		int ry = y + 3 + k * 11;
		if (k == voce_carta) ui_riga_sel(x + 3, ry, w - 6);
		ui_testo(x + 10, ry + 2, NOME_VOCE_CARTA[voci_carta[k]],
		         k == voce_carta ? K_GIALLO : K_TESTO, 1);
	}
}

/* mostra l'intero ciclo della carta, passo per passo, con descrizione;
   il pannello e' piu' stretto per lasciare spazio allo sprite a lato */
static void disegna_vedi_ciclo(void) {
	const Carta *c = prendi(campo[cursore]);
	int n = c->n_az;
	int h = 24 + n * 10;
	int w = 176;
	int x = 4;
	int y = (160 - h) / 2;
	if (y < 8) y = 8;
	int pos = eta_campo[cursore] % n;
	char b[16];

	ui_pannello(x, y, w, h);
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

static void disegna_carte(void) {
	char b[32];

	for (int i = 0; i < n_campo && i < SLOT_SPRITE; i++) {
		const Carta *c = prendi(campo[i]);
		int x = CARTA_X(i);
		int cx = x + 16;
		int n = c->n_az;
		int pos = eta_campo[i] % n;
		int larg = n <= 5 ? 7 : 5;
		int sx = cx - (n * (larg + 1) - 1) / 2;

		if (i == cursore) ui_cornice(x - 2, CARTA_Y - 5, 36, 36, K_ORO);

		ui_barra(x, 71, 32, 4, pv_campo[i], c->pv, K_ROSSO);
		sprintf(b, "%d/%d", pv_campo[i], c->pv);
		ui_testo_c(cx, 78, b, K_TESTO, 1);

		for (int j = 0; j < n; j++) {
			char s[2] = {LETTERE[j], 0};
			int attivo = (j == pos);
			int bx = sx + (larg + 1) * j;
			ui_rett(bx, 89, larg, 9, attivo ? K_GIALLO : K_BARRA);
			ui_cornice(bx, 89, larg, 9, attivo ? K_ORO : K_TURCHESE);
			ui_testo(bx + (larg > 5 ? 1 : 0), 90, s, attivo ? K_PANNELLO : K_TESTO, 1);
		}
	}

	if (n_campo > 0) {
		const Carta *c = prendi(campo[cursore]);
		int len = c->n_az;
		int pos = eta_campo[cursore] % len;
		ui_pannello(4, 100, 232, 36);
		ui_testo(10, 104, c->nome, K_GIALLO, 1);
		sprintf(b, "Mazzo %d", N_MAZZO - prossima);
		ui_testo_d(230, 104, b, K_GRIGIO, 1);
		ui_testo(10, 115, "Servono", K_GRIGIO, 1);
		for (int k = 0; k < c->n_cas; k++) {
			ui_casella(60 + 26 * k, 113, c->cas[k], 0);
		}
		sprintf(b, "%c: %s", LETTERE[pos], azione_desc(c->az[pos]));
		ui_testo(10, 126, b, K_GIALLO, 1);
	}

	if (carta_menu) disegna_menu_carta();

	if (carta_menu) {
		ui_suggerimenti("Su/Giu voce  A conferma  B chiudi", "");
	} else {
		ui_suggerimenti("Sx/Dx carta  A menu  L dadi", "SELECT fine  START pausa");
	}
}

static void disegna_pausa(void) {
	static const char *const V[3] = {"Riprendi", "Riavvia partita", "Torna al titolo"};
	ui_pannello(50, 40, 140, 68);
	ui_testo_c(120, 46, "PAUSA", K_GIALLO, 2);
	for (int k = 0; k < 3; k++) {
		int ry = 66 + k * 12;
		if (k == voce_pausa) ui_riga_sel(55, ry, 130);
		ui_testo(70, ry + 2, V[k], k == voce_pausa ? K_GIALLO : K_TESTO, 1);
	}
	ui_suggerimenti("Su/Giu voce  A conferma", "B o START riprendi");
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
	ui_suggerimenti("A conferma", "B o SELECT annulla");
}

static void disegna(void) {
	aggiorna_sprite();
	ui_pulisci();

	if (!iniziata) {
		ui_pannello(20, 44, 200, 54);
		ui_testo_c(120, 54, "Doom Machine", K_GIALLO, 2);
		ui_testo_c(120, 80, "Premi START per iniziare", K_TESTO, 1);
		for (int i = 0; i < 4; i++) {
			ui_testo_c(38 + 44 * i + 16, 144, POOL[i].nome, K_TESTO, 1);
		}
	} else if (vinto || perso) {
		ui_pannello(40, 48, 160, 64);
		ui_testo_c(120, 62, vinto ? "HAI VINTO!" : "HAI PERSO",
		           vinto ? K_VERDE : K_ROSSO, 2);
		ui_testo_c(120, 90, "START: nuova partita", K_TESTO, 1);
	} else if (pausa) {
		ui_hud();
		disegna_pausa();
	} else if (conferma) {
		ui_hud();
		disegna_conferma();
	} else {
		ui_hud();
		disegna_barre_laterali();
		if (vista == 0) disegna_dadi();
		else if (vedi_ciclo) disegna_vedi_ciclo();
		else disegna_carte();
	}

	ui_carica();
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
}

int main(void) {
	irqInit();
	irqEnable(IRQ_VBLANK);
	au_init();

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

		if (!iniziata) {
			if (premuti & KEY_START) {
				rng_seed(frame ^ 0xA5A5A5A5u);
				nuova_partita();
				SUONA(S_AVVIO);
				cambiato = 1;
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
				voce_pausa = (voce_pausa + 2) % 3;
				SUONA(S_SU_GIU);
				cambiato = 1;
			}
			if (premuti & KEY_DOWN) {
				voce_pausa = (voce_pausa + 1) % 3;
				SUONA(S_SU_GIU);
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
					nuova_partita();
					SUONA(S_AVVIO);
				} else {
					iniziata = 0;
					pausa = 0;
					SUONA(S_INDIETRO);
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
				SUONA(S_INDIETRO);
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
				SUONA(S_ERRORE);
			} else {
				conferma = 1;
				SUONA(S_SELECT);
				cambiato = 1;
			}
		} else if (premuti & (KEY_L | KEY_R)) {
			if (attacco || menu_aperto || carta_menu || vedi_ciclo) {
				SUONA(S_ERRORE);
			} else {
				vista = !vista;
				da_menu = 0;
				SUONA((premuti & KEY_L) ? S_L : S_R);
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
					SUONA(S_DADI);
					cambiato = 1;
				}
			} else if (attacco) {
				if (premuti & KEY_LEFT) {
					selezionato = (selezionato + n_attivi - 1) % n_attivi;
					SUONA(S_SX_DX);
					cambiato = 1;
				}
				if (premuti & KEY_RIGHT) {
					selezionato = (selezionato + 1) % n_attivi;
					SUONA(S_SX_DX);
					cambiato = 1;
				}
				if (premuti & KEY_A) {
					esito_assegna(assegna());
					cambiato = 1;
				} else if (premuti & KEY_B) {
					annulla_attacco();
					SUONA(S_INDIETRO);
					cambiato = 1;
				}
			} else if (menu_aperto) {
				costruisci_menu();
				if ((premuti & KEY_UP) && n_voci > 0) {
					voce_sel = (voce_sel + n_voci - 1) % n_voci;
					SUONA(S_SU_GIU);
					cambiato = 1;
				}
				if ((premuti & KEY_DOWN) && n_voci > 0) {
					voce_sel = (voce_sel + 1) % n_voci;
					SUONA(S_SU_GIU);
					cambiato = 1;
				}
				if (premuti & KEY_A) {
					esegui_voce();
					cambiato = 1;
				} else if (premuti & KEY_B) {
					menu_aperto = 0;
					SUONA(S_INDIETRO);
					cambiato = 1;
				}
			} else {
				if (premuti & KEY_LEFT) {
					selezionato = (selezionato + n_attivi - 1) % n_attivi;
					SUONA(S_SX_DX);
					cambiato = 1;
				}
				if (premuti & KEY_RIGHT) {
					selezionato = (selezionato + 1) % n_attivi;
					SUONA(S_SX_DX);
					cambiato = 1;
				}
				if (premuti & KEY_A) {
					costruisci_menu();
					if (n_voci > 0) {
						menu_aperto = 1;
						voce_sel = 0;
						SUONA(S_OK);
					} else {
						SUONA(S_ERRORE);
					}
					cambiato = 1;
				}
			}
		} else {
			/* ----- scheda carte ----- */
			if (vedi_ciclo) {
				if (premuti & KEY_B) {
					vedi_ciclo = 0;
					SUONA(S_INDIETRO);
					cambiato = 1;
				}
			} else if (carta_menu) {
				costruisci_menu_carta();
				if ((premuti & KEY_UP) && n_voci_carta > 0) {
					voce_carta = (voce_carta + n_voci_carta - 1) % n_voci_carta;
					SUONA(S_SU_GIU);
					cambiato = 1;
				}
				if ((premuti & KEY_DOWN) && n_voci_carta > 0) {
					voce_carta = (voce_carta + 1) % n_voci_carta;
					SUONA(S_SU_GIU);
					cambiato = 1;
				}
				if (premuti & KEY_A) {
					if (n_voci_carta > 0) {
						if (voci_carta[voce_carta] == CZ_ATTACCA) {
							attacco = 1;
							bersaglio = cursore;
							for (int k = 0; k < MAX_CAS; k++) riempita[k] = 0;
							carta_menu = 0;
							SUONA(S_OK);
						} else {
							vedi_ciclo = 1;
							carta_menu = 0;
							SUONA(S_OK);
						}
					} else {
						SUONA(S_ERRORE);
					}
					cambiato = 1;
				} else if (premuti & KEY_B) {
					carta_menu = 0;
					SUONA(S_INDIETRO);
					cambiato = 1;
				}
			} else {
				if ((premuti & (KEY_LEFT | KEY_UP)) && n_campo > 0) {
					cursore = (cursore + n_campo - 1) % n_campo;
					SUONA((premuti & KEY_UP) ? S_SU_GIU : S_SX_DX);
					cambiato = 1;
				}
				if ((premuti & (KEY_RIGHT | KEY_DOWN)) && n_campo > 0) {
					cursore = (cursore + 1) % n_campo;
					SUONA((premuti & KEY_DOWN) ? S_SU_GIU : S_SX_DX);
					cambiato = 1;
				}
				if (premuti & KEY_B) {
					da_menu = 0;
					vista = 0;
					SUONA(S_INDIETRO);
					cambiato = 1;
				} else if (premuti & KEY_A) {
					if (n_campo == 0) {
						SUONA(S_ERRORE);
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
							SUONA(S_OK);
						} else {
							SUONA(S_ERRORE);
						}
					}
					cambiato = 1;
				}
			}
		}

		if (cambiato) disegna();
	}
}