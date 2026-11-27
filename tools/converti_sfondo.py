#!/usr/bin/env python3
"""Converte lo sfondo 240x160 (header con cyberpunk_table_Pal / _Bitmap,
modo 4) in source/grafica_tavolo.h: tile 8bpp deduplicati + mappa 30x20.

Uso:  python3 tools/converti_sfondo.py cyberpunk_table_bg.h source/grafica_tavolo.h

Vincoli: max 256 tile unici (un char block) e max 240 colori; la palette
parte dall'indice 16 perche' 0-15 sono della UI (BG0)."""
import re, sys

def array(testo, nome):
    m = re.search(nome + r'\[\d+\][^=]*=\s*\{(.*?)\};', testo, re.S)
    return [int(x, 0) for x in re.findall(r'0x[0-9A-Fa-f]+|\d+', m.group(1))]

def main(src, dst):
    s = open(src).read()
    pal = array(s, 'cyberpunk_table_Pal')
    px = bytearray()
    for w in array(s, 'cyberpunk_table_Bitmap'):
        px += bytes([w & 255, w >> 8])
    usati = sorted(set(px))
    assert len(usati) <= 240, "troppi colori"
    nuovo = {c: 16 + i for i, c in enumerate(usati)}
    tiles, indice, mappa = [], {}, []
    for ty in range(20):
        for tx in range(30):
            t = bytes(nuovo[px[(ty * 8 + y) * 240 + tx * 8 + x]] for y in range(8) for x in range(8))
            if t not in indice:
                indice[t] = len(tiles); tiles.append(t)
            mappa.append(indice[t])
    assert len(tiles) <= 256, "troppi tile unici: %d" % len(tiles)
    o = ["#ifndef GRAFICA_TAVOLO_H", "#define GRAFICA_TAVOLO_H", "",
         "/* GENERATO da tools/converti_sfondo.py - non modificare a mano.",
         "   Sfondo 240x160: %d tile 8bpp unici, %d colori (indici BG da 16). */" % (len(tiles), len(usati)), "",
         "#define TAV_N_COLORI %d" % len(usati), "#define TAV_N_TILE %d" % len(tiles), "",
         "static const unsigned short TAV_PAL[TAV_N_COLORI] = {"]
    o.append("\t" + ", ".join("0x%04X" % pal[c] for c in usati) + ",")
    o += ["};", "", "static const unsigned int TAV_TILE[TAV_N_TILE * 16] = {"]
    for t in tiles:
        ws = [sum(t[r * 8 + k * 4 + j] << (8 * j) for j in range(4)) for r in range(8) for k in range(2)]
        o.append("\t" + ", ".join("0x%08X" % w for w in ws) + ",")
    o += ["};", "", "static const unsigned char TAV_MAPPA[600] = {"]
    for r in range(20):
        o.append("\t" + ", ".join(str(v) for v in mappa[r * 30:(r + 1) * 30]) + ",")
    o += ["};", "",
          "static void carica_tavolo(void) {",
          "\tvolatile unsigned short *pal = (volatile unsigned short *)0x05000000;",
          "\tvolatile unsigned int *vram = (volatile unsigned int *)0x06008000;",
          "\tvolatile unsigned short *map = (volatile unsigned short *)0x0600E000;",
          "",
          "\tfor (int i = 0; i < TAV_N_COLORI; i++) pal[16 + i] = TAV_PAL[i];",
          "\tfor (int i = 0; i < TAV_N_TILE * 16; i++) vram[i] = TAV_TILE[i];",
          "\tfor (int ty = 0; ty < 32; ty++) {",
          "\t\tfor (int tx = 0; tx < 32; tx++) {",
          "\t\t\tmap[ty * 32 + tx] = (tx < 30 && ty < 20) ? TAV_MAPPA[ty * 30 + tx] : 0;",
          "\t\t}",
          "\t}",
          "\t/* BG1: priorita' 2, char block 2, 256 colori, mappa nel blocco 28 */",
          "\t*(volatile unsigned short *)0x0400000A = 2 | (2 << 2) | (1 << 7) | (28 << 8);",
          "}", "", "#endif", ""]
    open(dst, "w").write("\n".join(o))
    print("tile %d, colori %d" % (len(tiles), len(usati)))

main(sys.argv[1], sys.argv[2])
