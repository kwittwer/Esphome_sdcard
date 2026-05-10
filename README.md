# ESPHome SD Card Logger

Dieses Repository stellt eine ESPHome External Component bereit, um eine SD-Karte mit Dateisystem einzubinden und Messwerte/Ereignisse aus `lambda` heraus zu loggen.

## Features

- SD-Karte über konfigurierbaren CS-Pin einbinden
- Messwerte mit `;` getrennt loggen
- Jede Messwert-Zeile startet mit Datum und Uhrzeit (`YYYY-MM-DD HH:MM:SS`)
- Ereignisse in Tagesdateien (`YYYY-MM-DD.log`) schreiben
- **Web-Dateimanager** (automatisch aktiv wenn `web_server:` konfiguriert ist): Dateien im Browser auflisten, herunterladen und löschen

## Verwendung

```yaml
external_components:
  - source: github://kwittwer/Esphome_sdcard

time:
  - platform: sntp
    id: sntp_time

sdcard_logger:
  id: sd_logger
  cs_pin: GPIO5
  time_id: sntp_time
  base_dir: /logs
```

## Lambda-Aufrufe

### Messwerte loggen

```yaml
interval:
  - interval: 60s
    then:
      - lambda: |-
          id(sd_logger).log_measurements(
            "messwerte.csv",
            id(temp_sensor).state,
            id(humidity_sensor).state,
            "OK"
          );
```

Ausgabeformat pro Zeile:

```text
2026-05-09 20:30:00;23.4;54.8;OK
```

### Ereignis loggen

```yaml
on_boot:
  then:
    - lambda: |-
        id(sd_logger).log_event("Systemstart abgeschlossen");
```

Diese Zeile wird in eine Datei mit aktuellem Datum geschrieben, z. B. `/logs/2026-05-09.log`.

## Web-Dateimanager

Wenn `web_server:` im Projekt konfiguriert ist, aktiviert sich der Web-Dateimanager **automatisch** – keine zusätzliche Konfiguration nötig.

```yaml
web_server:
  port: 80

sdcard_logger:
  id: sd_logger
  cs_pin: GPIO5
  time_id: sntp_time
  base_dir: /logs
```

Verfügbare Endpunkte:

| URL | Beschreibung |
|-----|--------------|
| `http://<ip>/sd` | Dateiliste der SD-Karte anzeigen |
| `http://<ip>/sd/download?path=<dateiname>` | Datei herunterladen |
| `http://<ip>/sd/delete?path=<dateiname>` | Datei löschen (Weiterleitung zu `/sd`) |

Die Dateiliste wird als übersichtliche HTML-Seite mit Download- und Lösch-Schaltflächen dargestellt. Ist `USE_WEBSERVER_AUTH` konfiguriert, gelten die Zugangsdaten des Web-Servers auch für den Dateimanager.

> **Hinweis:** Der Web-Dateimanager listet nur Dateien direkt in `base_dir` auf – keine Unterverzeichnisse. Gleichzeitiger SD-Zugriff aus dem Hauptprogramm und dem Web-Handler sollte vermieden werden.

## Beispielkonfigurationen

- KinCony B16M: `examples/kincony_b16m_sd_logger.yaml`
