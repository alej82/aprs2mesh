## Why

Un tracker APRS (un teléfono con APRSdroid, o un PC con Direwolf) necesita un TNC y una radio para transmitir. En una zona sin repetidoras APRS —y sin querer depender de internet— ese tráfico no tiene por dónde salir, aunque una malla Meshtastic pueda estar activa y cubriendo la zona. Los gateways existentes resuelven el puente semántico pero exigen una computadora y conectividad, justo lo que un tracker embarcado no tiene.

Hace falta un equipo único, barato y flasheable que se presente ante el cliente APRS como un TNC KISS estándar y use la malla Meshtastic como enlace local, sin internet ni software intermediario.

## What Changes

- Nuevo firmware para LilyGO T-Beam v1.1 (ESP32 clásico + SX1276) construido con PlatformIO y el core Arduino-ESP32.
- **Interfaz KISS TNC** disponible simultáneamente por Bluetooth Classic SPP y por TCP (`:8001`), con framing FEND/FESC y decodificación de tramas AX.25.
- **Subida**: cada trama de datos KISS se convierte a texto TNC2, se le quita el path de digipeaters para ahorrar aire y se publica en un canal Meshtastic preconfigurado con un prefijo configurable (por defecto `A#`).
- **Recepción**: los textos del canal que llevan el prefijo se convierten de vuelta a tramas AX.25 y se entregan a los clientes KISS conectados, con deduplicación por `(from, id)` y sin reemitir tráfico ajeno.
- **Portal web de configuración** en modo punto de acceso (callsign, canal, PSK, región, preset, prefijo, límites de transmisión) con persistencia en NVS y árbol de arranque RUN/PORTAL.
- **Presentación en la malla** como nodo `CLIENT_MUTE` con `long_name` igual al callsign, `hop_limit` 3 y NodeInfo periódico, sin reemisión de paquetes de otros.
- **Sin conexión a internet**: el equipo no abre sesiones con APRS-IS, no se asocia como cliente a ninguna red WiFi y no depende de servicios externos para operar.

Fuera de alcance en v1: conexión a internet y gateway APRS-IS en cualquier dirección, mensajes directos PKI, `POSITION_APP` y visualización en mapa, gestión del nodo desde la app oficial de Meshtastic, actualización OTA, beacon autónomo con GPS, reemisión de tráfico ajeno y cualquier texto del canal que no lleve el prefijo.

## Capabilities

### New Capabilities

- `kiss-transport`: interfaz KISS TNC sobre Bluetooth SPP y TCP, framing, códec AX.25 ↔ TNC2, concurrencia de clientes y serialización de transmisiones.
- `mesh-link`: enlace compatible con Meshtastic (región, preset, canal y PSK), publicación y recepción de textos, identidad del nodo, deduplicación y disciplina de airtime.
- `device-configuration`: portal web en modo punto de acceso, almacenamiento en NVS, árbol de arranque y parámetros del equipo.

### Modified Capabilities

Ninguna: el repositorio no tiene specs previas.

## Impact

- Proyecto nuevo desde cero: `platformio.ini`, `src/`, `lib/`, `docs/` y `data/` para el portal. No hay código existente que modificar.
- Dependencias: RadioLib (SX1276), BluetoothSerial (Bluedroid SPP), WiFi en modo AP + WebServer, mbedTLS (AES-CTR), Preferences/NVS y un codificador/decodificador protobuf mínimo para `MeshPacket`, `Data` y `User`.
- Hardware objetivo: LilyGO T-Beam v1.1 (ESP32-D0WD, SX1276, GPS no utilizado en v1), con posible PMU AXP192 según revisión de placa.
- Presupuesto de recursos: Bluedroid SPP + lwIP + LoRa conviven en 520 KB de SRAM interna; la coexistencia de radio 2.4 GHz entre BT clásico y el punto de acceso WiFi solo ocurre durante la configuración y debe validarse en banco.
- Sistemas externos: únicamente la malla Meshtastic compartida con otros operadores, cuyo airtime y normas de convivencia deben respetarse. No hay integración con APRS-IS ni con ningún servicio de internet.
- Riesgo regulatorio: el enlace LoRa opera en banda SRD (869 MHz en EU), no en banda de radioaficionados; el proyecto debe advertirlo explícitamente.
