#ifndef GRAFICA_H
#define GRAFICA_H

#define N_ARTI 16
#include "doom_cards.h"
#define COL5(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

/* Gli sprite delle carte sono disegnati a mano in doom_cards.h (32x32, 4bpp).
   L'ordine qui sotto e' quello di POOL in template.c; l'ultimo e' il Nucleo
   (Doom Core). Banco palette OBJ: 1-6 dadi, 7 Doom Core, 8 Synaptic
   Disruptor, 9 tutte le altre carte (hanno la stessa palette). */
#define ARTE_SYNAPTIC 6        /* Synaptic Disruptor */
#define ARTE_CORE (N_ARTI - 1) /* Doom Core */
#define BANCO_CORE 7
#define BANCO_SYNAPTIC 8
#define BANCO_COMUNE 9

static const unsigned int *const ARTE_TILE[N_ARTI] = {
	misery_extractor_tiles,
	infernal_power_complex_tiles,
	design_flaw_tiles,
	thought_siphone_tiles,
	extinction_collider_tiles,
	recycling_vats_tiles,
	synaptic_disruptor_tiles,
	cerebral_amplifier_tiles,
	pulse_pools_tiles,
	logic_mainframe_tiles,
	populace_grinder_tiles,
	impact_nexus_tiles,
	soul_harvester_tiles,
	stasis_generator_tiles,
	hard_cycle_storage_tiles,
	doom_core_tiles,
};

static int banco_carta(int slot) {
	if (slot == ARTE_CORE) return BANCO_CORE;
	if (slot == ARTE_SYNAPTIC) return BANCO_SYNAPTIC;
	return BANCO_COMUNE;
}

static void carica_palette(int banco, const unsigned short *pal16) {
	volatile unsigned short *pal = (volatile unsigned short *)0x05000200;
	for (int i = 0; i < 16; i++) pal[banco * 16 + i] = pal16[i];
}

/* copia nella VRAM i tile e le palette di tutte le carte */
static void carica_grafica(void) {
	volatile unsigned int *vram = (volatile unsigned int *)0x06010000;

	carica_palette(BANCO_CORE, doom_core_pal);
	carica_palette(BANCO_SYNAPTIC, synaptic_disruptor_pal);
	carica_palette(BANCO_COMUNE, extinction_collider_pal);

	for (int c = 0; c < N_ARTI; c++) {
		for (int i = 0; i < CARD_TILE_COUNT * 8; i++) vram[c * 16 * 8 + i] = ARTE_TILE[c][i];
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
