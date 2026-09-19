"""Erzeugt aus i18n/<sprache>.json die Sprachressourcen der Uhr.

- resources/lang/<sprache>.bin: alle Texte in fester Reihenfolge, je mit \\0 abgeschlossen
- src/c/i18n.auto.h:            Enum der Text-IDs, Sprachliste, Ressourcen-IDs

Wird von wscript bei jedem Build aufgerufen. Bricht ab, wenn einer Sprache Texte
fehlen oder die printf-Platzhalter nicht zur englischen Vorlage passen.
"""
import io
import json
import os
import re

# Reihenfolge = Wert der Einstellung "Sprache" (1-basiert, 0 = automatisch). Nicht umsortieren!
LANGS = ['en', 'de', 'fr', 'es', 'it', 'pt', 'nl', 'pl', 'cs', 'sv', 'da', 'nb', 'fi',
         'ru', 'uk', 'tr', 'ja', 'zh', 'ko']

PLACEHOLDER = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?(?:l|h)?[diouxXscp%]')


def _write_if_changed(path, data):
    if os.path.exists(path):
        with open(path, 'rb') as f:
            if f.read() == data:
                return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)


def generate(root):
    src = os.path.join(root, 'i18n')

    def load(lang):
        with io.open(os.path.join(src, lang + '.json'), encoding='utf-8') as f:
            return json.load(f)

    reference = load('en')
    keys = list(reference.keys())
    errors = []

    for lang in LANGS:
        data = load(lang)
        for key in keys:
            if key not in data:
                errors.append('{}: fehlt {}'.format(lang, key))
                continue
            if PLACEHOLDER.findall(data[key]) != PLACEHOLDER.findall(reference[key]):
                errors.append('{}: Platzhalter in {} passen nicht zu en'.format(lang, key))
        for key in data:
            if key not in reference:
                errors.append('{}: unbekannter Schlüssel {}'.format(lang, key))
        if errors:
            continue
        blob = b''.join(data[k].encode('utf-8') + b'\0' for k in keys)
        _write_if_changed(os.path.join(root, 'resources', 'lang', lang + '.bin'), blob)

    if errors:
        raise Exception('Übersetzungen fehlerhaft:\n  ' + '\n  '.join(errors))

    lines = [
        '// AUTOMATISCH ERZEUGT von tools/gen_strings.py - nicht bearbeiten',
        '#pragma once',
        '',
        'typedef enum {',
    ]
    lines += ['  S_{},'.format(k) for k in keys]
    lines += [
        '  STR_COUNT',
        '} StrId;',
        '',
        '#define LANG_COUNT {}'.format(len(LANGS)),
        '#define LANG_CODES {{{}}}'.format(', '.join('"{}"'.format(l) for l in LANGS)),
        '#define LANG_RESOURCE_IDS {{{}}}'.format(
            ', '.join('RESOURCE_ID_LANG_{}'.format(l.upper()) for l in LANGS)),
        '',
    ]
    _write_if_changed(os.path.join(root, 'src', 'c', 'i18n.auto.h'),
                      '\n'.join(lines).encode('utf-8'))


if __name__ == '__main__':
    generate(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    print('ok')
