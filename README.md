# subway-display

A desk gadget that shows minutes until the next trains at two NYC subway stops
on a 4.2" e-paper panel.

```
client/     ESP32 firmware: WiFi, HTTP poll, e-paper rendering
server/     Node service: reads MTA GTFS-realtime, serves tiny JSON + a config UI
```

The ESP32 does no transit logic. It polls one endpoint every 30s and draws
whatever rows come back, so changing stations never needs a reflash.

## Getting set up

Everything runs out of the Nix shell:

```sh
nix-shell          # arduino-cli, node, python3
setup              # one time per machine: ESP32 core, Arduino libs, npm install
```

`setup` is separate because Nix supplies the toolchains but not these
dependencies: the ESP32 core is ~7.8GB of prebuilt Espressif binaries fetched by
arduino-cli (nixpkgs has no xtensa-esp32 compiler), and the server's packages
come from npm.

## Client

```sh
cp client/secrets.h.example client/secrets.h   # then fill in your WiFi
build              # compile
flash              # upload over USB
mon                # serial monitor at 115200
```

Set `ARRIVALS_URL` in `client/config.h` to wherever the server runs. It is
compiled in, so give that host a DHCP reservation or you will be reflashing
whenever its address moves.

`client/secrets.h` is gitignored and must stay that way.

## Server

```sh
cd server
npm ci
npm run build      # tsc -> dist/
npm start          # or: serve
```

Listens on `ARRIVALS_PORT` (default 8080). Not `PORT` -- the shell exports that
for the ESP32's serial device.

| route | |
|---|---|
| `GET /` | config UI: pick stations, preview the panel |
| `GET /api/arrivals` | what the device polls |
| `GET`/`PUT /api/config` | current rows / replace them |
| `GET /api/stops?q=&line=` | search the station catalog |
| `GET /healthz` | liveness |

No authentication: this is meant for a LAN. Anyone who can reach the port can
change what the display shows.

`stops.json` is a 24KB catalog of all 496 stations and the routes serving each,
generated from the static GTFS feed. It is committed so the 36MB
`stop_times.txt` never has to be.

### Notes on the data

- Subway GTFS-realtime feeds need no API key. (Bus Time still does.)
- A feed carries a whole line group, so rows filter on `route_id` too -- the
  `ace` feed has A trains calling at the same platform as the C.
- Stop IDs carry an `N`/`S` platform suffix. Those are railroad directions, not
  compass ones.
- Several stations share a name: three different stops are called "7 Av".
  `PUT /api/config` rejects a line/station pair that does not actually exist.

## Hardware

ELEGOO ESP32-WROOM DevKit, Waveshare 4.2" e-Paper V2 Rev 2.2 (400x300, B/W,
SSD1683). Always on USB power.

| Panel | GPIO | | Panel | GPIO |
|---|---|---|---|---|
| DIN | 23 | | DC | 21 |
| CLK | 18 | | RST | 22 |
| CS | 5 | | BUSY | 4 |

### Things that cost time to learn

- **GxEPD2 does not work with this panel batch.** Every driver class tried gave
  `Busy Timeout!`. `client/epd4in2_V2.*` is a trimmed version of Waveshare's own
  demo driver, which does work.
- **DC/RST must not be on GPIO 16/17** (GxEPD2's defaults) -- reset is
  unreliable there on this DevKit.
- **Waveshare's `IfInit()` calls `SPI.beginTransaction()` on every `Init()`**,
  which deadlocks on the second call. Hence the `spiStarted` flag in `epdif.cpp`.
- **Partial refresh needs the controller left alone between updates.** `Init()`
  resets it and `Sleep()` deep-sleeps it; both lose the RAM plane that a partial
  refresh diffs against, and the display composites onto leftovers. Full refresh
  re-establishes it, which is why that path alone calls `Init()`.
- A black or inverted screen means a refresh was interrupted, not a polarity bug.
