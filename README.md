# R307 Fingerprint

An ESP32 reads an R307 optical fingerprint sensor. You can enroll new fingerprints from a PC over serial, and the firmware keeps scanning in the background. When a finger is placed on the sensor, it reports whether the print matches a stored one. Each enrollment is also recorded in a small JSON database on the ESP32's flash (LittleFS).

## Features

- Enroll a fingerprint under an ID from 1 to 200 with a serial command
- Enrollment takes two scans of the same finger and stores the combined template on the sensor
- Continuous scanning: place a finger at any time and the firmware searches the sensor's stored templates
- On a match, it prints the ID, the sensor's confidence score and when that ID was enrolled
- A JSON record per enrolled ID, saved to `/fingerprints.json` on LittleFS so it survives a reboot
- An "ESP is living!" message over serial every 15 s
- The onboard LED blinks as a heartbeat to show the firmware is running

## Hardware

| Part | Purpose |
|---|---|
| ESP32 dev board (`esp32dev`) | Microcontroller |
| R307 optical fingerprint sensor | Captures, stores and matches fingerprints |

## Wiring

The sensor talks over UART at 57600 baud. It uses the ESP32's second UART (`Serial2`) so it stays off GPIO 1/3, which are shared with USB serial.

| R307 pin | ESP32 |
|---|---|
| VCC | 5 V (VIN) |
| GND | GND |
| TX | GPIO 16 (RX2) |
| RX | GPIO 17 (TX2) |
| T-OUT | not connected |
| T3V3 | not connected |

The heartbeat LED is the onboard LED on GPIO 2.

The R307's TX and RX are crossed over: sensor TX goes to ESP32 RX, and sensor RX goes to ESP32 TX. Wire colours differ between R307 modules, so check the pinout for yours. T-OUT and T3V3 are only needed for touch-to-wake, which this firmware doesn't use.

## Building and flashing

This is a [PlatformIO](https://platformio.org/) project. The libraries are listed in `platformio.ini`, and PlatformIO downloads them automatically:

- `adafruit/Adafruit Fingerprint Sensor Library`
- `bblanchon/ArduinoJson` (v7)

```sh
pio run -t upload      # build and flash
pio device monitor     # serial monitor at 115200 baud
```

On the first boot, LittleFS formats itself if the filesystem can't be mounted.

## Usage

Open the serial monitor at 115200 baud and send one of these commands, each followed by a newline:

| Command | What it does |
|---|---|
| `e <id>` | Enroll a new fingerprint under `<id>` (1–200) |
| `?` | Print the list of commands |

To enroll, send for example `e 1`, then follow the prompts: place your finger, remove it, and place the same finger again.

To scan, just place a finger on the sensor. No command is needed.

## Serial output

On startup:

```
ESP Alive!
Fingerprint sensor found!
Send 'e <id>' to enroll a new fingerprint (id 1-200).
Otherwise, just place a finger on the sensor to scan.
```

Enrolling:

```
Place finger on sensor...
Image taken
Remove finger
Place same finger again...
Image taken
Enrolled fingerprint as ID #1
```

Scanning:

```
Scanning fingerprint...
Match! ID #1  confidence: 142  enrolled at: 48213 ms uptime
Scan complete
```

If the sensor isn't detected at startup, you get `Fingerprint sensor NOT found - check wiring` instead.

## Database format

`/fingerprints.json` maps each enrolled ID to its record:

```json
{
  "1": { "timestamp": 48213 },
  "2": { "timestamp": 91577 }
}
```

The fingerprint templates themselves live on the R307, not in this file. The JSON only holds extra information about each ID.

## Known quirks

- The enrollment "timestamp" is `millis()`, the ESP32's uptime in milliseconds when the finger was enrolled. It's not a real date or time, and it restarts from 0 after every reboot.
- Enrolling an ID that already exists overwrites both the template on the sensor and its JSON record, without a warning.
- There's no command to delete a fingerprint or clear the database.
- While waiting for a finger during enrollment, the firmware blocks and doesn't time out. Until a finger is placed, the heartbeat LED stops blinking and the "ESP is living!" message pauses.
- If a template exists on the sensor but not in the JSON (for example after the flash was erased), a match still works and prints `(no JSON record found)`.
- The R307 can hold more than 200 templates. The firmware limits IDs to 1–200.
