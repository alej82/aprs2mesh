# aprs2mesh

Two firmwares for the LilyGO T-Beam v1.1 (ESP32 classic + SX1276) that carry
APRS frames over a Meshtastic channel as plain text:

- **tracker** (`tbeam`): KISS TNC over Bluetooth SPP or TCP `:8001`. Frames
  travel the mesh with the fixed `A#` prefix; the device needs no internet.
- **gateway** (`server_tbeam`): joins the same channel, injects the prefixed
  texts into APRS-IS with `qAR`, admits only replies addressed to stations that
  originated a recent uplink, and exposes a read-only KISS monitor.

The plan lives in `openspec/changes/add-kiss-mesh-gateway/` (tracker) and
`openspec/changes/add-aprs-is-gateway/` (gateway).

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
| APRS-IS session (login, keepalive, backoff, verified/unverified) | implemented, host-tested, not run on hardware |
| Gateway pipeline (`qAR` injection, pinhole flows, `TCPIP*` rewrite, dedupe and rates) | implemented, host-tested |
| Read-only KISS monitor (`:8002`) | implemented, compiles for T-Beam, not run on hardware |
| Gateway config, AP+STA portal and hot gate switch | implemented, compiles for T-Beam, not run on hardware |

Nothing is claimed as working on the board until it has been run on the board.

## Layout

```
lib/aprs/       KISS framing, AX.25 codec, TNC2 lines (portable)
lib/kiss/       KISS client hub (tracker only)
lib/mesh/       Meshtastic wire format, text contract, dedupe, TX queue
lib/bridge/     KISS <-> mesh pipeline (uplink, downlink, echo, counters)
lib/gateway/    APRS-IS session, flows, admission, monitor bridge (portable)
lib/config/     configuration model and validation
src/            tracker firmware: main loop and transports (Arduino)
src/gateway/    gateway firmware: main, APRS-IS transport, monitor, portal
src/transport/  tracker TCP and Bluetooth SPP adapters
test/           native unit tests (Unity)
```

## Build and test

Host tests need no board and no ESP32 toolchain:

```
pio test -e native
```

Firmware builds:

```
pio run -e tbeam          # tracker
pio run -e tbeam -t upload
pio device monitor -e tbeam

pio run -e server_tbeam   # APRS-IS gateway
pio run -e server_tbeam -t upload
pio device monitor -e server_tbeam
```

The tracker image is about 1.66 MB, which does not fit the default 1.31 MB app
slot once Bluedroid and WiFi are both linked, so the `tbeam` env uses
`huge_app.csv` (single 3 MB app partition, no OTA). The gateway links no
Bluetooth and fits the default slot at about 960 KB. Enable OTA later only with
a custom partition table that accounts for the size.

## Rollback

The board is not locked: flash the official Meshtastic firmware again with the
web flasher or `esptool.py`. The configuration blob stays in NVS but Meshtastic
ignores it; run `esptool.py erase-flash` as well if you want a clean slate
before handing the board over.

## Tracker bench check without radio

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

## Gateway (`server_tbeam`)

Same board, no Bluetooth Classic. The gateway joins the same channel and PSK as
the tracker, reads the `A#` texts and injects them into APRS-IS. Each successful
injection opens a *pinhole* for that origin: messages addressed to it are
admitted during the flow TTL (60 minutes by default) and published back to the
channel as `A#` texts whose path is reduced to `TCPIP*`.

### Provisioning

1. Flash `server_tbeam` and open the serial monitor. Without a valid
   configuration it boots in portal mode with the AP `aprs2mesh-gw`
   (password `aprs2meshgw`) and opens no APRS-IS session.
2. Open `http://192.168.4.1/` and fill in:
   - gateway callsign with its own SSID (for example `EA1ABC-10`);
   - APRS-IS host (default `rotate.aprs2.net`), port 14580, passcode, and the
     receive filter as a single line (default `m/50`);
   - WiFi SSID and password, channel name and base64 PSK, region and preset;
   - flow TTL, maximum flows and the rate limits.
3. The gate starts **closed**. Tick "Gate abierto al guardar" or use the gate
   button after the first boot.
4. Save: the board reboots into run mode and raises the AP and the STA link at
   the same time, so the portal keeps answering while the igate is connected.
5. Point Direwolf or Xastir at `TCP:8002` for the read-only monitor.

### Gate

The portal's gate button applies immediately, without a reboot: the next `A#`
text is not injected and the event is counted. The monitor and the mesh
reception keep working. The state is persisted, so a reboot keeps it.

### When nothing arrives

The status page separates three numbers on purpose:

- **recibidas**: non-comment lines from APRS-IS (the filter works);
- **admitidas**: messages that matched an open flow (the pinhole works);
- **rechazadas**: everything else, with the last reason.

Zero received means the filter is too narrow or the session is down. Received
growing with admitted at zero means nobody has injected recently, or the replies
are not addressed to the stations that did.

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
- The gateway is open to the channel: anyone with the PSK can inject under any
  callsign. It records node and callsign per injection, rate-limits and has a
  kill switch, but it does not authenticate. Do not share the PSK with people
  you would not trust with your APRS identity.
- Injected packets use the `APZ2M` software id and the `qAR` construct; they
  were not heard on RF. APRS-IS is public: check what you are publishing.
- The `TCPIP*` mark on downlinks is a convention, not a lock. A misconfigured
  client can still re-inject a downlink; APRS-IS deduplicates it, but the effect
  should be understood before publishing.
- No warranty: this is experimental firmware on hardware that also controls a
  radio transmitter.

## Notes

- KISS limits: 512-byte frames, 8 digipeaters, 256-byte info field.
- The tracker AP SSID and password are compile-time defaults (`aprs2mesh` /
  `aprs2mesh`); change them in `src/main.cpp`. The gateway uses
  `aprs2mesh-gw` / `aprs2meshgw` in `src/gateway/main.cpp` and serves the
  monitor on `TCP:8002`.
- The channel PSK is typed as base64, the same format the Meshtastic app shows.
- The mesh prefix is the constant `A#` in `lib/mesh/src/mesh_text.h`; it is not
  configurable so the tracker and the gateway cannot drift apart.
- Dedupe windows: 30 s for client retries `(src, payload)`, 60 s for mesh floods
  `(from, id)`, 30 s for the gateway uplink buffer and the downlink
  `(src, msgid, text)` window.
- The APRS-IS connect call can block the main loop for up to 2 s during a retry
  (DNS plus TCP handshake). The monitor and the radio buffer absorb it; the
  field measurement is still pending.
- The gateway starts with the gate closed on purpose: provisioning has to open
  it explicitly.
- Radio and Mesh parameters are not wired yet; nothing is transmitted on LoRa in
  either firmware.
