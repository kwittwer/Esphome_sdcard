# ESPHome SD Card Logger

Diese Repository stellt eine ESPHome External Component bereit, um eine SD-Karte mit Dateisystem einzubinden und Messwerte/Ereignisse aus `lambda` heraus zu loggen.

## Features

- SD-Karte über konfigurierbaren CS-Pin einbinden
- Messwerte mit `;` getrennt loggen
- Jede Messwert-Zeile startet mit Datum und Uhrzeit (`YYYY-MM-DD HH:MM:SS`)
- Ereignisse in Tagesdateien (`YYYY-MM-DD.log`) schreiben

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

## Beispielkonfigurationen

- KinCony B16M: `/home/runner/work/Esphome_sdcard/Esphome_sdcard/examples/kincony_b16m_sd_logger.yaml`
