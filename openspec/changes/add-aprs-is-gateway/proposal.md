## Why

El tracker deja las tramas APRS dentro de la malla Meshtastic, pero ahí terminan: ningún nodo estándar las publica en APRS-IS ni trae las respuestas. Sin un segundo extremo, el sistema sirve para conversar con otros usuarios de Meshtastic, no para llegar a la red APRS.

La alternativa conocida es una computadora con Direwolf haciendo de igate, pero contradice el objetivo de un equipo autónomo sin PC. El segundo T-Beam puede hacer las dos cosas que faltan —hablar LoRa y hablar TCP— en la misma caja, siempre que el firmware implemente el cliente APRS-IS, la admisión de bajada y un puerto de monitoreo que haga visible lo que hoy es invisible.

## What Changes

- Nuevo target `server_tbeam` sobre el mismo T-Beam v1.1, **sin Bluetooth Classic**: la placa solo necesita LoRa y WiFi.
- **Cliente APRS-IS**: login con callsign, passcode e identificador experimental `APZ`, filtro escrito por el operador (una sola línea, saneada), keepalive, reconexión con espera creciente y detección de login no verificado.
- **Subida**: cada texto con prefijo `A#` que llega del canal se inyecta en APRS-IS con su path original y el construct `qAR` del gateway; deduplicación de 30 s y límite de tasa. Lo que llega desde APRS-IS no se reinyecta nunca.
- **Buffer corto sin internet**: hasta 30 s de subidas pendientes; lo que excede el TTL se descarta y se cuenta.
- **Bajada por pinhole**: solo se admiten mensajes APRS dirigidos a una estación que originó un intercambio reciente (TTL configurable, 60 min por defecto). Beacons, objetos, boletines y paquetes de terceros se descartan.
- **Reescritura de bajada**: los constructs `q` y el pseudo-digi `TCPIP*` se normalizan a un path `TCPIP*`, para marcar que el paquete viene de internet y evitar que un cliente lo reinyecte como si fuera RF.
- **Monitor KISS de solo lectura** en TCP, con las tramas de ambos sentidos en formato AX.25 para Direwolf/Xastir; lo que el cliente intente transmitir se descarta.
- **Portal propio** (`aprs2mesh-gw`) con credenciales APRS-IS, filtro, TTL, tasas, región, preset fijo o `CUSTOM` (frecuencia, ancho de banda, factor de dispersión y tasa de codificación), número de saltos, estado del gate y contadores que distinguen "no llegó nada" de "llegó y se rechazó".
- **Política abierta al canal**: al ser un canal privado con PSK propia, cualquier miembro puede inyectar; se compensa con registro por inyección, límites de tasa y un interruptor de cierre del gate.
- **Separación de código compartido**: AX.25/TNC2 sale de `lib/kiss` a `lib/aprs` para que el gateway no arrastre el hub ni los transportes del tracker; dedupe y cola de transmisión pasan a `lib/mesh`.

Fuera de alcance en v1: allowlist de nodos o callsigns (el modo abierto es el único), firmas o HMAC de las tramas, mensajes directos PKI, `POSITION_APP`, OTA, y cualquier cambio en el firmware del tracker (vive en `add-kiss-mesh-gateway`).

## Capabilities

### New Capabilities

- `aprs-is-gateway`: sesión APRS-IS, subida de textos del canal con `qAR`, buffer corto, tabla de flujos con TTL, admisión de bajada, reescritura de path, límites de tasa y política del gate.
- `kiss-monitor`: puerto KISS/TCP de solo lectura que refleja el tráfico de ambos sentidos para observación y clientes APRS de escritorio.
- `gateway-configuration`: portal del gateway, credenciales y filtro de APRS-IS, parámetros de política, persistencia en NVS y estado con contadores de diagnóstico.

### Modified Capabilities

Ninguna. El contrato de texto compartido (`mesh-link`) y el portal del tracker siguen en `add-kiss-mesh-gateway`, que permanece abierto; este cambio consume ese contrato sin redefinirlo.

## Impact

- Nuevo entorno `[env:server_tbeam]` en `platformio.ini`, sin `BluetoothSerial`; se espera un binario bastante menor que los 1.62 MB del tracker, a medir.
- Reorganización de librerías: `lib/aprs` (AX.25/TNC2) separada de `lib/kiss`, y `lib/mesh` ampliada con dedupe y cola de transmisión (tareas 3.8 y 3.9 del change del tracker).
- Dependencias nuevas: cliente TCP sin TLS (APRS-IS usa el puerto 14580 en claro) y resolución DNS por lwIP. No se requiere NTP: los TTL usan tiempo monótono.
- Hardware: segundo LilyGO T-Beam v1.1 con LoRa y WiFi; el GPS no se usa.
- Sistemas externos: servidores APRS-IS (`rotate.aprs2.net:14580`) y la malla compartida.
- Riesgo operativo: el gateway es el único punto de salida hacia internet. Si se cae, el tracker sigue funcionando pero el tráfico no llega a APRS; sin ACKs, el operador solo lo detecta por el monitor o el portal.
- Riesgo de política: un gateway abierto al canal puede inyectar bajo cualquier callsign. El registro, los límites y el interruptor son parte del diseño, y la documentación debe decirlo.
- Riesgo regulatorio: el enlace sigue operando en banda SRD, no en banda de radioaficionados.
