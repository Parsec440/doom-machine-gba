#!/usr/bin/env python3
"""Converte un brano (MOD, MP3, WAV...) in data/musica.bin: PCM 8 bit con segno,
mono, 16384 Hz, normalizzato. In coda si copiano i primi MUS_HEAD campioni
dell'inizio, cosi' il loop riparte senza buchi. Scrive anche source/musica.h.

Uso:  python3 tools/converti_musica.py brano.mod data/musica.bin source/musica.h
Richiede ffmpeg (con supporto libopenmpt per i MOD)."""
import subprocess, sys
import numpy as np

RATE = 16384
HEAD = 8192  # campioni copiati in coda (0,5 s)

def main(src, dst_bin, dst_h):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", src, "-ac", "1", "-ar", str(RATE), "-f", "f32le", "-"],
                         check=True, capture_output=True).stdout
    d = np.frombuffer(raw, dtype="<f4").astype(np.float64)
    d = d[: len(d) // 4 * 4]
    d = d / np.abs(d).max() * 0.95
    b = np.round(d * 127).astype(np.int8)
    n = len(b)
    open(dst_bin, "wb").write(np.concatenate([b, b[:HEAD]]).tobytes())
    open(dst_h, "w").write(
        "#ifndef MUSICA_H\n#define MUSICA_H\n\n"
        "/* GENERATO da tools/converti_musica.py - non modificare a mano.\n"
        "   data/musica.bin = MUS_LEN campioni + MUS_HEAD campioni di coda (copia dell'inizio). */\n"
        "#define MUS_LEN %d\n#define MUS_HEAD %d\n\n#endif\n" % (n, HEAD))
    print("musica: %d campioni (%.1f s), file %d byte" % (n, n / RATE, n + HEAD))

main(*sys.argv[1:4])
