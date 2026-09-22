#!/usr/bin/env python3
"""Erzeugt Beschleunigungsdaten für den Emulator (pebble emu-accel custom <datei>).

    python tools/emulator/make_testdata.py [zielordner]

Wichtig: Der Emulator ignoriert Dateien mit mehr als ~250 Zeilen, deshalb sind
alle Dateien 250 Zeilen = 10 s bei 25 Hz. Für längere Bewegung mehrmals
hintereinander einspielen (sleep 9.9 dazwischen).

Dateien:
  stroke_a.txt  Streichen: linear auf x, 2 Hz, 450 mg  (Solo/Partner)
  stroke_b.txt  Fortsetzung von stroke_a (Sekunde 10-16), fürs Anlernen
  rub.txt       Reiben: kreisend x/y, 4 Hz, 120 mg
  posA.txt      Stellung A: Uhr flach (Schwerkraft -z), Bewegung auf x, 2 Hz
  posB.txt      Stellung B: Uhr hochkant (Schwerkraft +y), Bewegung auf z, 1,5 Hz
"""
import math
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
HZ = 25


def write(name, fn, n=250, start=0):
    path = os.path.join(OUT, name)
    with open(path, 'w', newline='\n') as f:
        for i in range(start, start + n):
            f.write('%d, %d, %d\n' % fn(i / HZ))
    print(path)


write('stroke_a.txt', lambda t: (int(450 * math.sin(2 * math.pi * 2 * t)), -150, -980))
write('stroke_b.txt', lambda t: (int(450 * math.sin(2 * math.pi * 2 * t)), -150, -980), n=150, start=250)
write('rub.txt', lambda t: (int(120 * math.sin(2 * math.pi * 4 * t)),
                            int(-300 + 120 * math.cos(2 * math.pi * 4 * t)), -950))
write('posA.txt', lambda t: (int(400 * math.sin(2 * math.pi * 2 * t)), -150, -980))
write('posB.txt', lambda t: (80, 950, int(-150 + 300 * math.sin(2 * math.pi * 1.5 * t))))
