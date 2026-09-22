# Projektstand Stamina (Übergabe)

Stand: 22.09.2026 · Version 0.6 (Beta) · Repo: https://github.com/n0va-SIDEffects/Pebble-Stamina

Diese Datei fasst zusammen, was gebaut wurde, warum es so gebaut wurde und was
noch offen ist. Damit lässt sich in einem neuen Chat weiterarbeiten, ohne den
bisherigen Verlauf zu kennen. Funktionsübersicht und Bauanleitung stehen in
[README.md](README.md), die Store-Schritte in [store/VEROEFFENTLICHEN.md](store/VEROEFFENTLICHEN.md).

## 1. Was die App ist

Watchapp für die Pebble Time 2 (und Pebble 2 Duo, Pebble Time, Pebble 2), die
Masturbation und Sex wie ein Workout trackt: Bewegungen zählen, Puls,
Kalorienschätzung, Höhepunkt markieren, danach Schlaf und Start in den Tag
auswerten. Partner-Modus mit angelernten Stellungen und Partner-Kürzeln.
Alle Daten bleiben auf der Uhr; nur optionale Timeline-Pins verlassen sie.

Name bewusst unauffällig („Stamina“), Texte auf der Uhr neutral formuliert.
Der Projektordner heißt noch `Wanker`, das ist nur der lokale Ordnername.

## 2. Umgebung

- SDK in der **WSL2-Distro `Pebble`** (Ubuntu 24.04, pebble-tool 5.0.40, SDK 4.33.1),
  installiert am 19.09.2026. Die ältere Distro `Ubuntu` ist WSL1 und hat kein SDK.
- Bauen aus dem Projektordner:
  `wsl -d Pebble -u root bash build.sh [pebble-argumente]`
- Demo-Build mit Beispieldaten für Screenshots: `STAMINA_DEMO=1 bash build.sh`
- Jeder Build legt zusätzlich `build/stamina.pbw` ab. **`build/Wanker.pbw` nicht
  löschen**, `pebble install` sucht genau diesen Namen (nach dem Projektordner).
- Python mit Pillow liegt unter Windows (nicht in WSL): `python tools/make_store_assets.py`.

### Emulator-Fallen (teuer erkauft)

- `emu-accel custom` ignoriert Dateien mit mehr als ~250 Zeilen (= 10 s bei 25 Hz).
  Längere Bewegung: dieselbe Datei mehrmals mit ~9,9 s Pause einspielen.
- `emu-set-time` wird beim App-Start zurückgesetzt. Zeitabhängiges (Schlaf, Morgen,
  Auto-Start-Fenster) testet man, indem man die Wartezeiten im Code kurz herabsetzt.
- Touch, Schlafphasen, Schritte und Timeline-Token lassen sich nicht simulieren.
- Puls kommt mit Verzögerung; `emu-heart-rate` während der Aufnahme mehrfach senden.
- Menüpositionen verschieben sich, wenn Testskripte Tasten zählen. Deshalb in
  `tools/emulator/screenshots.sh` vor jedem Bild die App neu starten.

## 3. Architektur

```
src/c/         Uhr-App              src/shared/   von App und Worker genutzt
  main.c       Hauptmenü, Start       shared.h    Typen, Persistenz-Schlüssel, Profil
  tracker.c    Session-Bildschirm     detector.c  Bewegungserkennung
  detector.c   Parameter aus Profil
  calibrate.c  Anlernen             worker_src/c/worker.c  Hintergrund-Erkennung (Auto-Start)
  positions.c  Stellungen           src/pkjs/     Einstellungsseite (Clay 1.0.4 in vendor/)
  partners.c   Partner-Kürzel       i18n/         Texte der Uhr (en.json ist die Vorlage)
  badges.c     59 Erfolge           i18n/phone/   Texte der Einstellungsseite
  charts.c     Diagramme            tools/        Generatoren (Sprachen, Grafiken, Release)
  sleep.c      Schlaf danach        store/        Store-Texte, Screenshots, Icons, Banner
  morning.c    Start in den Tag, Stimmungsfrage (Wakeup)
  history.c    Verlauf, Detail, Statistik
  profile.c    Einstellungen auf der Uhr
  settings.c   Abgleich mit dem Handy, Timeline-Pins
  storage.c    Persistenz           demo.c        Beispieldaten (nur Demo-Build)
  i18n.c       lädt die aktive Sprache
```

### Bewegungserkennung (Kern)

25 Hz Beschleunigung. Pro Achse Schwerkraft per Tiefpass abziehen, Achse mit der
meisten Bewegungsenergie wählen, Signal mit Schmitt-Trigger (adaptive Schwelle)
in Zyklen zerlegen. Parameter je Stil (Streichen / Reiben / Toy / Partner),
skaliert mit der Empfindlichkeit und überschrieben vom Anlernen.
Derselbe Code läuft in der App und im Hintergrund-Worker.

### Persistenz (Limit 4 KB pro App, aktuell ~3 KB)

| Schlüssel | Inhalt |
|---|---|
| 1 | Version des Speicherformats |
| 2 | Profil (Geschlecht, Alter, Gewicht, alle Einstellungen) |
| 3 | Anzahl Sessions |
| 4 | Anlernen je Bewegungsart |
| 5 | letzte automatische Erkennung |
| 6, 7 | Stellungen: Fingerabdrücke, Namen |
| 8 | Partner-Kürzel |
| 9, 10 | Erfolge, Summen |
| 11 | Zähler für die Diagramme |
| 100+ | Sessions (max. 30, je 58 Byte) |

Summen (Erfolge) und Diagramm-Zähler laufen getrennt vom Verlauf, weil der nur
30 Einträge behält. Neue Felder immer **hinten** anhängen: `persist_read_data`
liest kürzere alte Daten ein, der Rest behält die Vorgabewerte.

### Speicher

Die App braucht ~47 KB von 64 KB (Pebble Time, Pebble 2, Pebble 2 Duo) und
~48 KB von 128 KB (Time 2). Vor größeren Erweiterungen prüfen, ob die alten
Uhren noch mitkommen, oder Funktionen per `#if` auf emery beschränken.

## 4. Sprachen

19 Sprachen: en, de, fr, es, it, pt, nl, pl, cs, sv, da, nb, fi, ru, uk, tr, ja, zh, ko.

- Quelle: `i18n/<sprache>.json`, Vorlage ist `en.json` (Reihenfolge = Reihenfolge
  der Text-IDs im Code!). `tools/gen_strings.py` erzeugt beim Build die
  Ressourcen und **bricht ab**, wenn ein Schlüssel fehlt oder die printf-Platzhalter
  nicht zur Vorlage passen.
- Neue Texte: erst `en.json` und `de.json`, dann die übrigen 17 (bisher von
  Hilfs-Agents übersetzt, jeweils mit dem Hinweis, Begriffe aus der Datei
  wiederzuverwenden).
- Die Uhr lädt nur die aktive Sprache (~4 KB).
- **Russisch, Ukrainisch, Japanisch, Chinesisch, Koreanisch** brauchen das
  passende Sprachpaket auf der Uhr. Ohne Paket zeigt die Systemschrift nur
  Kästchen, deshalb schaltet die App dann automatisch auf Englisch.
- Die Einstellungsseite am Handy hat eigene Texte: `i18n/phone/<sprache>.json`,
  eingepflegt in `src/pkjs/i18n.js` (Merge-Skript lag im Sitzungsordner; bei
  Bedarf neu schreiben: JSON laden, Schlüssel ergänzen, Datei als
  `module.exports = {...}` schreiben).

## 5. Entscheidungen und ihre Gründe

- **Kalorien gedeckelt** (3,5 MET solo, 5,6–6 MET Partner, nach Frappier 2013):
  Erregung hebt den Puls, ohne dass entsprechend Energie verbraucht wird. Ohne
  Deckel käme Joggen heraus.
- **Fensterwechsel nie im Klick-Handler.** Zerstört sich ein Fenster selbst,
  greift die Firmware danach auf freigegebenen Speicher zu (Absturz, reproduziert).
  Überall `app_timer_register(10, …)` dazwischen.
- **Touch nur mit Wischgesten in der Session**, keine Taps: Hautkontakt soll
  nichts auslösen. Abschaltbar.
- **Puls-Licht rot mit kurzer Vibration** (auf Wunsch), Textfarben während des
  Blitzes weiß, sonst unlesbar.
- **Stellungserkennung** nutzt nur Handgelenk-Lage, Bewegungsrichtung, Stärke,
  Rhythmus. Stellungen mit gleicher Armhaltung sind nicht unterscheidbar; die
  App warnt beim Anlernen, wenn zwei zu ähnlich sind.
- **Timeline-Pins standardmäßig aus:** Sie laufen über den Rebble-Server
  (Internet) und widersprechen sonst dem Versprechen „keine Cloud“. Session-Pins
  zeigen nur Dauer und kcal, weil die Timeline auch andere sehen.
- **Store-Wortwahl sachlich:** Die Pebble Program Policies verbieten
  „sexually explicit or erotic content, icons, titles, or descriptions“. Deshalb
  neutrale Beschreibung, neutrale Screenshots (Stellungsnamen „Top“, „Classic“,
  „Side“ im Demo-Build) und ein Icon ohne Anspielung. Altershinweis sachlich
  formuliert („Intended for adults“), nicht als „18+“-Warnschild.
- **Icon:** gekipptes pinkes Herz mit weißer Herzschlag-Linie; Launcher-Icon
  nur die Herz-Silhouette (25 px verträgt keine feinen Linien).

## 6. Store

- Alles fertig in `store/`: `description_en.txt` (1508 von 1600 Zeichen),
  `RELEASE_NOTES.md`, `privacy.md`, Icons (80 ohne Alpha, 144, 48), `banner.png`
  (mit SIDE effect's Logo unten links), Screenshots für alle vier Plattformen,
  `VEROEFFENTLICHEN.md`, Paket `store/release/` + ZIP.
- Neu bauen: `python tools/make_store_assets.py`, dann `python tools/make_release.py --zip`
  (prüft Version, Zeichenlimit und ob versehentlich der Demo-Build drin ist).
- **Regeln, die schon Einreichungen gekostet haben** (aus dem pebble-publish-Skill):
  Version im Format `Major.Minor`; Icon ohne Alpha; Quellcode-URL ist Pflicht;
  Einreichung am PC, nicht im Handy-Browser; höchstens 5 Screenshots je Plattform;
  Listung nur auf Englisch.
- Der **pebble-publish-Skill** des Nutzers liegt außerhalb dieses Projekts:
  `~/Downloads/pebble-publish/` und
  `~/Documents/Claude-Projekte/pebble banner/pebble-theremin/.claude/skills/pebble-publish/`.
  Vor Store-Arbeit dort `SKILL.md` und `references/*.md` lesen. Das Logo liegt in
  `pebble-theremin/store/icon/side_effects_logo.png` (Kopie hier in `store/assets/`).

## 7. Git

- Remote `origin` = https://github.com/n0va-SIDEffects/Pebble-Stamina.git, Branch `main`,
  alles gepusht.
- Commits laufen unter `n0va-SIDEffects <109081733+n0va-SIDEffects@users.noreply.github.com>`
  (repo-lokal gesetzt), damit die private E-Mail nicht öffentlich wird.
- Lizenz MIT; Clay (MIT) und Montserrat (SIL OFL) behalten ihre eigenen Lizenzen.

## 8. Offen / nächste Schritte

1. **Test auf der echten Uhr** (das Wichtigste): Bewegungserkennung und
   Schwellen, Touch-Gesten, Diktat für Stellungsnamen, Schlaf- und
   Morgen-Auswertung, Akkuverbrauch des Auto-Starts.
2. **Timeline-Pins** lassen sich erst nach der Veröffentlichung prüfen (Token).
3. **Übersetzungen** von Muttersprachlern gegenlesen lassen, besonders die
   Stellungsnamen (türkisch „Binme“, chinesisch 抚动/揉动) und die Begriffe für
   den Höhepunkt.
4. **Store-Einreichung** nach `store/VEROEFFENTLICHEN.md`, vorher Support-E-Mail
   festlegen.
5. Denkbar später: Stellungserkennung mit mehr Merkmalen, Datenexport aufs Handy,
   Speicher der alten Uhren im Blick behalten.

## 9. Emulator-Werkzeuge im Repo

- `tools/emulator/make_testdata.py` – Bewegungsdaten (Streichen, Reiben, zwei Stellungen)
- `tools/emulator/screenshots.sh` – Screenshot-Satz je Plattform (Demo-Build, Englisch)
- Kurztest von Hand:
  `wsl -d Pebble -u root bash build.sh install --emulator emery`, dann
  `emu-accel custom`, `emu-heart-rate`, `emu-button click select --duration 1200`.
