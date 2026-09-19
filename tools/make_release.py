#!/usr/bin/env python3
"""Schnürt den Store-Release-Ordner (store/release/) und optional ein ZIP.

    python tools/make_release.py [--zip]

Vorher: Normaler Build (nicht der Demo-Build!) und tools/make_store_assets.py.
Ablauf und Regeln nach dem pebble-publish-Skill: Version Major.Minor,
Store-Listung nur Englisch, max. 5 Screenshots pro Plattform, 80er-Icon ohne Alpha.
"""
import glob
import json
import os
import shutil
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
LIMIT = 1600

pk = json.load(open('package.json', encoding='utf-8'))
name = pk['pebble']['displayName'].replace(' ', '')
ver = pk['version']
if ver.count('.') != 1:
    sys.exit(f"version '{ver}' muss Major.Minor sein (sonst Server error 400 im Store)")

pbw = 'build/stamina.pbw'
if not os.path.exists(pbw):
    sys.exit('build/stamina.pbw fehlt, erst bauen')
with open(pbw, 'rb') as f:
    if b'Classic' + bytes(1) in f.read():  # Beispieldaten aus demo.c
        sys.exit('Demo-Build erkannt, normal bauen')

rel = 'store/release'
shutil.rmtree(rel, ignore_errors=True)
os.makedirs(rel)
shutil.copy(pbw, f'{rel}/{name}-{ver}.pbw')

for src, dst in (('store/icon_80.png', 'icon_80.png'), ('store/icon_large.png', 'icon_144.png'),
                 ('store/icon_small.png', 'icon_48.png'), ('store/banner.png', 'banner_720x320.png')):
    shutil.copy(src, f'{rel}/{dst}')

for d in sorted(glob.glob('store/screenshots/*/')):
    platform = os.path.basename(os.path.dirname(d))
    shots = sorted(glob.glob(os.path.join(d, '*.png')))[:5]  # Store: max. 5 pro Plattform
    out = f'{rel}/screenshots_{platform}'
    os.makedirs(out)
    for s in shots:
        shutil.copy(s, out)

desc = 'store/description_en.txt'
text = open(desc, encoding='utf-8').read()
body = text.split('\n', 1)[1].strip()
if len(body) > LIMIT:
    sys.exit(f'Beschreibung zu lang: {len(body)}/{LIMIT}')
shutil.copy(desc, f'{rel}/description_en.txt')
shutil.copy('store/RELEASE_NOTES.md', f'{rel}/RELEASE_NOTES.md')
print(f'Beschreibung {len(body)}/{LIMIT} Zeichen')
print('Release-Ordner:', sorted(os.listdir(rel)))

if '--zip' in sys.argv:
    z = f'store/{name}_Store_Paket.zip'
    with zipfile.ZipFile(z, 'w', zipfile.ZIP_DEFLATED) as zf:
        for root, _, files in os.walk(rel):
            for f in files:
                full = os.path.join(root, f)
                zf.write(full, os.path.relpath(full, 'store'))
        zf.write('store/VEROEFFENTLICHEN.md', 'VEROEFFENTLICHEN.md')
    print('ZIP:', z)
