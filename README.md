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
| Text contract, receive filter and packet classification | implemented, host-tested |
| Dedupe windows, bounded TX queue, airtime limits | implemented, host-tested |
| KISS ↔ mesh pipelines (uplink wired, downlink ready for RX) | implemented, host-tested |
| Config model, validation and atomic NVS store | implemented, host-tested (store compiles for T-Beam) |
| Configuration portal (AP + web form + status) | implemented, compiles for T-Beam, not run on hardware |

Nothing is claimed as working on the board until it has been run on the board.

## Layout

```
lib/kiss/       KISS codec, AX.25 codec, KISS hub (portable, no Arduino)
lib/mesh/       Meshtastic wire format, text contract, dedupe, TX queue
lib/bridge/     KISS <-> mesh pipeline (uplink, downlink, echo, counters)
lib/config/     configuration model and validation
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

## Rollback

The board is not locked: flash the official Meshtastic firmware again with the
web flasher or `esptool.py`. The configuration blob stays in NVS but Meshtastic
ignores it; run `esptool.py erase-flash` as well if you want a clean slate
before handing the board over.

## Bench check without radio

1. Flash the board and open the serial monitor. With no stored configuration
   the firmware boots in **portal mode**: it raises the AP `aprs2mesh`
   (password `aprs2mesh`) and does not start KISS or the mesh link.
2. Connect to the AP and open `http://192.168.4.1/`. Fill in a callsign (for
   example `EA1ABC-9`), the channel and its base64 PSK (`AQ==` is the
   Meshtastic default), region and preset, then save. The board reboots into
   run mode.
3. The AP stays up for the status page, which shows mode, connected KISS
   clients, bridge counters, airtime queue, free heap and the last error.
   Hold the BOOT button for three seconds while running, then release it, to
   reboot into portal mode again.
4. Point Direwolf (or `kissutil`) at `TCP:8001` and send an APRS frame.
5. The serial log prints `kiss rx: <TNC2>`, for example:

```
kiss rx: EA1ABC-9>APRS,WIDE1-1,WIDE2-1:!4332.10N/00752.30W-Test
```

That exercises framing, the AX.25 parser and the TNC2 formatter end to end.

The same frame then goes through the mesh pipeline, which is visible on the
serial console even before the radio exists:

```
mesh tx: A#EA1ABC-9>APRS,WIDE1-1,WIDE2-1:!4332.10N/00752.30W-Test
```

`mesh tx` is the seam where RadioLib will transmit (tasks 3.3-3.6). Every 30 s
the firmware prints the bridge counters: uplink accepted/duplicate/rejected,
downlink delivered/ignored/rejected, queue drains and airtime drops.

## Legal and privacy

- The LoRa link runs on an SRD band (869 MHz in the EU), **not** on an amateur
  radio band. Holding a ham license does not raise the power or duty-cycle
  limits that apply there; check the rules of your country before increasing
  them.
- The channel is shared: every member reads every text. A public channel is
  readable by anyone in range; a private PSK only restricts who can decrypt.
- APRS traffic carries callsigns and often positions. Publishing it on a mesh,
  and later through an APRS-IS gateway, makes it public. The operator decides
  what to send.
- Beacons and messages are the responsibility of the operator of the client
  that generates them. This firmware does not invent positions.
- No warranty: this is experimental firmware on hardware that also controls a
  radio transmitter.

## Notes

- `kiss.hpp` limits: 512-byte KISS frames, 8 digipeaters, 256-byte info field.
- The AP SSID and password are compile-time defaults (`aprs2mesh` /
  `aprs2mesh`); change them in `src/main.cpp`.
- The channel PSK is typed as base64, the same format the Meshtastic app shows.
- The mesh prefix is the constant `A#` in `lib/mesh/src/mesh_text.h`; it is not
  configurable so the future APRS-IS gateway cannot drift apart from the tracker.
- Dedupe windows: 30 s for client retries `(src, payload)`, 60 s for mesh floods
  `(from, id)`.
- Radio and Mesh parameters are not wired yet; nothing is transmitted on LoRa.
