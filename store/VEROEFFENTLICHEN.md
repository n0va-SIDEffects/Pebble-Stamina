# Stamina 0.6 (Beta) veröffentlichen

Portal: https://developer.repebble.com/dashboard (Konto der Pebble-Handy-App).
**Am PC im Desktop-Browser einreichen**, im Handy-Browser antwortet das Portal mit „Server error 400“.

## Dateien in `store/release/`

| Datei | Verwendung |
|---|---|
| `Stamina-0.6.pbw` | Release (Version und Plattformen liest das Portal daraus) |
| `icon_80.png` | Icon im Portal (RGB ohne Alpha, Pflichtfeld) |
| `icon_144.png`, `icon_48.png` | Large/Small Icon, falls abgefragt |
| `banner_720x320.png` | Marketing Banner, in jeder Asset Collection |
| `screenshots_emery/` | Pebble Time 2, 5 Bilder, 200 × 228 |
| `screenshots_flint/` | Pebble 2 Duo, 5 Bilder, 144 × 168 |
| `screenshots_basalt/` | Pebble Time, 5 Bilder, 144 × 168 |
| `screenshots_diorite/` | Pebble 2, 5 Bilder, 144 × 168 |
| `description_en.txt` | 1. Zeile Kurzbeschreibung, danach Beschreibung (≤ 1600 Zeichen) |
| `RELEASE_NOTES.md` | Text für „Release Notes“ |

## Portal-Schritte

1. **Add Watchapp**, `Stamina-0.6.pbw` hochladen.
2. Grunddaten:
   - Title: `Stamina`
   - Category: `Health & Fitness`
   - Source code URL: `https://github.com/n0va-SIDEffects/Pebble-Stamina`
   - Website: `https://github.com/n0va-SIDEffects/Pebble-Stamina`
   - Support email: deine Support-Adresse
   - Icon: `icon_80.png`
3. **Create**.
4. **Add a release**: `Stamina-0.6.pbw`, Release Notes aus `RELEASE_NOTES.md`. Seite neu laden, **Publish** am Release.
5. **Manage Asset Collections → Create**, je einmal für `emery`, `flint`, `basalt`, `diorite`:
   - Description: Text aus `description_en.txt` ab Zeile 2
   - Screenshots: der passende Ordner, Reihenfolge 01–05
   - Marketing Banner: `banner_720x320.png`
6. Zum Prüfen zuerst **Publish Privately** (nur per Link), danach **Publish**.

Die App erscheint nach einigen Minuten in der Suche der Pebble-Handy-App.

## Updates per Kommandozeile (WSL)

```bash
pebble login
pebble build
pebble publish --release-notes "…"
```

`version` in `package.json` erhöhen (Format `Major.Minor`, z. B. `0.7`). Die UUID `ca26453c-432f-4f97-94a6-b2afac7fb0c9` nie ändern.

## Checkliste

- [ ] Normaler Build, nicht der Demo-Build (`make_release.py` prüft das)
- [ ] `version` im Format `Major.Minor`
- [ ] Beschreibung ≤ 1600 Zeichen (`make_release.py` prüft das)
- [ ] Keine expliziten Begriffe in Titel, Icon, Beschreibung, Screenshots (Pebble Program Policies)
- [ ] Datenschutz-Link erreichbar: `store/privacy.md` im öffentlichen Repo
- [ ] Auf der echten Uhr getestet: Erkennung, Touch, Diktat, Timeline-Pins
