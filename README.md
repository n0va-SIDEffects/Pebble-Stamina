# Stamina – Pebble Time 2

Diskret benannte Watchapp, die Masturbation und Sex wie ein Workout trackt.
Alle Daten bleiben **nur auf der Uhr**. Es gibt keinen Upload, weder aufs Handy noch in eine Cloud.

☕ [Buy me a coffee](https://buymeacoffee.com/SIDEffects)

## Funktionen

| | |
|---|---|
| **Bewegungserkennung** | 25 Hz Beschleunigung, Schwerkraft-Filter, dominante Achse, adaptiver Schmitt-Trigger. Zählt Bewegungszyklen und den Rhythmus pro Minute. |
| **Solo-Stile** | **Streichen** (Penis: größere, lineare Unterarmbewegung, 1–4 Hz) · **Reiben** (Klitoris: kleine, schnelle, oft kreisende Bewegung, bis ~8 Hz) · **Toy** (Vibrator: nur aktive Zeit). „Automatisch“ wählt Reiben bei weiblich, sonst Streichen. |
| **Anlernen** | 15 s typische Bewegung aufnehmen. Rhythmus und Stärke werden gemessen und die Erkennung je Stil darauf eingestellt. Zur Kontrolle läuft die neue Erkennung noch einmal über dieselbe Aufnahme. |
| **Auto-Start** | Optional (aus / nach 1, 2 oder 3 min). Ein Hintergrund-Worker wertet einmal pro Sekunde die Bewegung mit derselben Erkennung aus. Bei anhaltender rhythmischer Bewegung (≥ 70 % der Sekunden, nicht beim Gehen oder Laufen) vibriert die Uhr und fragt „Aktivität erkannt, Session starten?“ (Solo / Partner / Nicht jetzt). Die Session wird auf den Beginn der Bewegung rückdatiert, die bis dahin gezählten Bewegungen werden übernommen. Mit erhöhtem Puls (≥ 10 bpm über dem Üblichen zu dieser Tageszeit) wird schon nach der halben Zeit gefragt. Ohne Antwort schließt sich die Frage nach 60 s. Danach 30 min Pause, nach einer Session 10 min. |
| **Stellungen** (Partner) | 8 Plätze mit Standardnamen, in der Pebble-App umbenennbar. Anlernen unter Anlernen > Stellungen: 5 s in Position gehen, 20 s Aufnahme. Daraus entsteht ein Fingerabdruck aus Handgelenk-Lage (Schwerkraft), Bewegungsrichtung, Stärke und Rhythmus. Mehrere Aufnahmen werden gemittelt. Ist eine Stellung kaum von einer anderen zu unterscheiden, gibt die Uhr einen Hinweis. Live-Test zum Prüfen. In der Session wird alle 5 s erkannt und oben angezeigt; gewechselt wird erst, wenn dieselbe Stellung zweimal hintereinander erkannt wird. Detail: Zeitanteil je Stellung; Statistik: Lieblingsstellung. |
| **Display-Licht** | Während der Session: normal / immer an / Puls. „Puls“ lässt den Bildschirm bei jedem Zug rot aufblitzen (Pebble 2 Duo: weiß) und kurz vibrieren. |
| **Puls** | Pebble-HRM, während der Session im 1-s-Intervall; Ø / min / max |
| **Kalorien** | Maximum aus bewegungsbasierter MET-Schätzung und pulsbasierter Formel (Keytel 2005), gedeckelt auf 3,5 MET (solo) bzw. 5,6–6 MET (Partner, nach Frappier 2013). Erregung hebt den Puls, ohne dass entsprechend Energie verbraucht wird. Deshalb der Deckel. |
| **Höhepunkt** | Taste UNTEN (oder hochwischen) markiert den Zeitpunkt |
| **Schlaf danach** | Aus Pebble Health: Einschlafzeit, Schlafdauer, Tiefschlaf, Abweichung vom persönlichen Durchschnitt |
| **Start in den Tag** | Nach Morgen-Sessions (Start zwischen 4 und 12 Uhr): Schritte, aktive Minuten und Puls der 4 Stunden danach, verglichen mit deinem Üblichen für dasselbe Zeitfenster (Werktag/Wochenende getrennt). Dazu 3 h danach eine kurze Stimmungsfrage (1–5), abschaltbar. |
| **Statistik** | Durchschnittswerte, Wochenzähler, Schlaf- und Morgen-Auswirkung über alle Einträge |
| **Touchscreen** (Time 2) | Menüs: wischen scrollt, tippen wählt. Zahleneingabe: ziehen ändert, tippen bestätigt. Session: seitlich wischen = Pause, hochwischen = Höhepunkt. Einfache Taps lösen nichts aus, damit Hautkontakt keine Aktion startet. Die Gesten sind abschaltbar. |
| **Sprachen** | Uhr und Einstellungsseite in 19 Sprachen: en, de, fr, es, it, pt, nl, pl, cs, sv, da, nb, fi, ru, uk, tr, ja, zh, ko. Automatisch nach Systemsprache oder fest einstellbar. Russisch, Ukrainisch, Japanisch, Chinesisch und Koreanisch brauchen das passende Sprachpaket auf der Uhr, sonst erscheint Englisch. |
| **Einstellungen am Handy** | Zahnrad in der Pebble-App. Uhr und Handy halten die Einstellungen gegenseitig aktuell. |

Bedienung in der Session: **Mitte** = Pause · **Mitte halten** / **Zurück** = beenden und speichern (ab 20 s) · **Unten** = Höhepunkt.
In der Detailansicht: **Mitte 2× halten** = Eintrag löschen. In „Anlernen“: **Mitte halten** = Anlernen zurücksetzen.

Es werden bis zu 30 Sessions gespeichert (Pebble-Limit: 4 KB Speicher pro App).

## Projektaufbau

```
src/c/            Watch-App (C)
  main.c          Hauptmenü, Start
  tracker.c       Session-Bildschirm, Puls, Kalorien
  detector.c      Bewegungserkennung (auch vom Anlernen genutzt)
  calibrate.c     Anlernen
  sleep.c         Schlaf danach
  morning.c       Start in den Tag, Stimmungsfrage (Wakeup)
  history.c       Verlauf, Details, Statistik
  profile.c       Einstellungen auf der Uhr
  settings.c      Abgleich mit der Einstellungsseite am Handy
  storage.c       Persistenz
  i18n.c          lädt die Texte der aktiven Sprache
src/c/autostart.c Nachfrage beim automatischen Start, Worker ein/aus
src/c/positions.c Stellungen: Merkmale, Anlernen, Live-Test, Erkennung
src/shared/       Bewegungserkennung + gemeinsame Typen/Schlüssel (App und Worker)
worker_src/c/     Hintergrund-Worker für den Auto-Start
src/pkjs/         Einstellungsseite (Clay 1.0.4 in vendor/, weil das npm-Paket flint nicht kennt)
i18n/<lang>.json  Texte der Uhr, en.json ist die Vorlage
i18n/phone/       neuere Texte der Einstellungsseite (in src/pkjs/i18n.js eingepflegt)
tools/gen_strings.py  erzeugt beim Build die Sprachressourcen und prüft alle Übersetzungen
```

Neue Texte: Schlüssel in `i18n/en.json` ergänzen, dann in allen anderen Sprachen.
Der Build bricht ab, wenn in einer Sprache ein Schlüssel fehlt oder die `printf`-Platzhalter nicht zur englischen Vorlage passen.

## Bauen

Das Pebble SDK läuft unter Linux/macOS, auf Windows in WSL2. Einmalig:

```sh
curl -LsSf https://astral.sh/uv/install.sh | sh
uv tool install pebble-tool
pebble sdk install latest
```

Hier liegt das SDK in der WSL2-Distro **`Pebble`** (Ubuntu 24.04, pebble-tool 5.0, SDK 4.33.1).
`build.sh` reicht alle Argumente an `pebble` durch:

```powershell
# im Projektordner (PowerShell)
wsl -d Pebble -u root bash build.sh                              # bauen -> build/*.pbw
wsl -d Pebble -u root bash build.sh install --emulator emery     # Pebble-Time-2-Emulator
wsl -d Pebble -u root bash build.sh install --phone <IP-Handy>   # echte Uhr (Dev-Verbindung in der Pebble-App an)
wsl -d Pebble -u root bash build.sh logs --emulator emery        # APP_LOG-Ausgaben
```

Zielplattformen: `emery` (Pebble Time 2, 200×228, Touch), `flint` (Pebble 2 Duo), `basalt`, `diorite`.

### Testen im Emulator

- `emu-accel custom <datei>`: Beschleunigungsdaten (Zeilen `x, y, z` in mg, 25 Hz, **max. ~250 Zeilen pro Datei**)
- `emu-heart-rate 110`: Puls einspielen
- `emu-button click select` / `--duration 1200` für langes Drücken
- `send-app-message --int 10000=1 ...`: Einstellungen wie vom Handy senden (IDs in `build/js/message_keys.json`)
- Auto-Start: Auto-Start einschalten, App schließen, dann `stroke_a.txt` (250 Zeilen) mehrmals hintereinander einspielen
- Nicht simulierbar: Touch, Schlafphasen, Schritte im Zeitfenster. `emu-set-time` wird beim App-Start zurückgesetzt, Wartezeiten zum Testen also kurz in `morning.c` herabsetzen.

## Hintergrund zur Stellungserkennung

Zu jeder Session wird gespeichert:
- mittlerer Schwerkraftvektor (`grav[3]`): wie das Handgelenk im Raum liegt
- Orientierungsverteilung (`orient_pct[6]`): Anteil der Zeit pro Achse und Richtung
- Rhythmus, Puls und Modus

Eine Uhr am Handgelenk sieht nur Arm-Orientierung und Rhythmus, nicht die Körperlage.
Ein realistischer Weg:
1. **Label-Funktion**: Während einer Partner-Session per Tastendruck die aktuelle Stellung markieren und so Trainingsdaten sammeln.
2. **Features pro 5-s-Fenster**: Schwerkraftvektor, dominante Achse, Frequenz, Amplitude, Puls.
3. **Einfacher Klassifikator auf der Uhr** (z. B. kleiner Entscheidungsbaum oder k-NN auf den Features), am PC mit den gelabelten Daten trainiert.
4. Für den Export braucht es PebbleKit JS (`src/pkjs`) oder einen Datenexport aufs Handy.

Umgesetzt (siehe „Stellungen“). Grenze: Die Uhr sieht nur das eigene Handgelenk. Stellungen mit gleicher Armhaltung lassen sich nicht sicher unterscheiden.
