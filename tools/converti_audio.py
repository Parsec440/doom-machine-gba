#!/usr/bin/env python3
"""Converte i WAV degli effetti in source/audio_campioni.h (PCM 8 bit con
segno, mono, 16384 Hz: il timer 0 del GBA conta 1024 cicli per campione).

Uso:  python3 tools/converti_audio.py cartella_wav source/audio_campioni.h

Per cambiare un suono basta sostituire il WAV (stesso nome) e rilanciare."""
import sys, wave
import numpy as np

RATE = 16384
FRAME_HZ = 16777216 / 280896   # frame video al secondo
SUONI = [  # (costante, file)
    ("CAMP_DADI", "01_lancio_dadi.wav"),
    ("CAMP_SPOSTA", "05_selezioni_spostamento_schede.wav"),
    ("CAMP_SELEZIONE", "06_selezione.wav"),
    ("CAMP_DESELEZIONE", "07_deselezione.wav"),
    ("CAMP_SCHEDA", "08_cambio_scheda.wav"),
    ("CAMP_ERRORE", "99_selezione_non_corretta.wav"),
]

def carica(path):
    w = wave.open(path)
    assert w.getsampwidth() == 2, "serve PCM a 16 bit"
    d = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64) / 32768
    if w.getnchannels() > 1:
        d = d.reshape(-1, w.getnchannels()).mean(axis=1)
    return d, w.getframerate()

def ricampiona(d, sr):
    # filtro passa-basso (sinc con finestra) sotto Nyquist del nuovo rate, poi interpolazione
    fc = (RATE / 2 * 0.9) / sr
    n = np.arange(-48, 49)
    h = np.sinc(2 * fc * n) * 2 * fc * np.hamming(len(n))
    d = np.convolve(d, h / h.sum(), mode="same")
    t = np.arange(0, len(d) / sr, 1 / RATE)
    return np.interp(t * sr, np.arange(len(d)), d)

def main(cartella, dst):
    o = ["#ifndef AUDIO_CAMPIONI_H", "#define AUDIO_CAMPIONI_H", "",
         "/* GENERATO da tools/converti_audio.py - non modificare a mano.",
         "   PCM 8 bit con segno, mono, 16384 Hz. */", ""]
    nomi, dati, frames = [], [], []
    for i, (nome, f) in enumerate(SUONI):
        d, sr = carica(f"{cartella}/{f}")
        d = ricampiona(d, sr)
        pico = np.abs(d).max() or 1
        d = d / pico * 0.9
        fade = int(RATE * 0.004)
        d[-fade:] *= np.linspace(1, 0, fade)
        b = np.round(d * 127).astype(np.int8)
        n = len(b)
        b = np.concatenate([b, np.zeros(1024 + (-(n + 1024)) % 4, dtype=np.int8)])  # silenzio di coda
        nomi.append(nome); dati.append(b)
        frames.append(int(np.ceil(n * FRAME_HZ / RATE)) + 1)
        o.append(f"#define {nome} {i}")
    o += ["", f"#define N_CAMPIONI {len(SUONI)}", ""]
    for i, b in enumerate(dati):
        o.append(f"static const signed char CAMP_{i}[{len(b)}] __attribute__((aligned(4))) = {{")
        for k in range(0, len(b), 24):
            o.append("\t" + ",".join(str(int(x)) for x in b[k:k + 24]) + ",")
        o.append("};")
    o += ["", "static const signed char *const CAMP_DATI[N_CAMPIONI] = {"]
    o += [f"\tCAMP_{i}," for i in range(len(dati))] + ["};", ""]
    o += ["/* durata in frame video, per fermare la riproduzione */",
          "static const unsigned short CAMP_FRAME[N_CAMPIONI] = {" + ", ".join(map(str, frames)) + "};", "", "#endif", ""]
    open(dst, "w").write("\n".join(o))
    print("campioni:", [(n, len(b)) for n, b in zip(nomi, dati)], "totale byte", sum(len(b) for b in dati))

main(sys.argv[1], sys.argv[2])
