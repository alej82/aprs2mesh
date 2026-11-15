# aprs2mesh

KISS TNC bridge between an APRS client and a Meshtastic channel, running on a
LilyGO T-Beam v1.1 (ESP32 classic + SX1276). No internet, no companion
computer: the client connects over Bluetooth SPP or TCP, and the frames travel
the mesh as plain channel text.

The plan lives in `openspec/changes/add-kiss-mesh-gateway/`.

## Status

| Area | State |
|---|---|
| KISS codec (FEND/FESC, resync, size limits, frame classes) | implemented, host-tested |
| AX.25 ↔ TNC2 (addresses, SSID, path, UI/PID) | implemented, host-tested |
| KISS hub (per-client decoders, broadcast, failed-client eviction) | implemented, host-tested |
| TCP KISS server (`:8001`) | implemented, compiles for T-Beam, not run on hardware |
| Bluetooth SPP transport | implemented, compiles for T-Beam, not run on hardware |
| Mesh wire format: protobuf `MeshPacket`/`Data`/`User`, 16-byte header, channel hash, PSK expansion | implemented, host-tested against NIST/firmware vectors |
| AES-CTR payload crypto (AES-128/256, nonce from packet id + node) | implemented, host-tested against NIST vectors |
| Mesh radio (RadioLib, presets, region) | pending |
| Dedupe, TX queue, airtime limits | pending |
| KISS ↔ mesh pipelines | pending |
| Configuration portal + NVS | pending |

Nothing is claimed as working on the board until it has been run on the board.

## Layout

```
lib/kiss/       KISS codec, AX.25 codec, KISS hub (portable, no Arduino)
src/            firmware: main loop and transports (Arduino)
src/transport/  TCP and Bluetooth SPP adapters
test/           native unit tests (Unity)
```

## Build and test

Host tests need no board and no ESP32 toolchain:

```
pio test -e native
```

Firmware build:

```
pio run -e tbeam
pio run -e tbeam -t upload
pio device monitor -e tbeam
```

The image is about 1.6 MB, which does not fit the default 1.31 MB app slot
once Bluedroid and WiFi are both linked. The build uses `huge_app.csv`
(single 3 MB app partition, no OTA). Enable OTA later only with a custom
partition table that accounts for the size.

## Bench check without radio

1. Flash the board and open the serial monitor.
2. The AP `aprs2mesh` (password `aprs2mesh`) comes up with the IP printed at boot.
3. Point Direwolf (or `kissutil`) at `TCP:8001` and send an APRS frame.
4. The serial log prints `kiss rx: <TNC2>`, for example:

```
kiss rx: EA1ABC-9>APRS,WIDE1-1,WIDE2-1:!4332.10N/00752.30W-Test
```

That exercises framing, the AX.25 parser and the TNC2 formatter end to end.

## Notes

- `kiss.hpp` limits: 512-byte KISS frames, 8 digipeaters, 256-byte info field.
- The AP credentials are compile-time defaults until task 5.3 (portal).
- Radio and Mesh parameters are not wired yet; nothing is transmitted on LoRa.
