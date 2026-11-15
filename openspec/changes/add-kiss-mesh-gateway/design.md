## Context

Proyecto nuevo: el repositorio sólo contiene el andamiaje de OpenSpec. El objetivo y el alcance están en `proposal.md`; el comportamiento exigido, en `specs/`.

Restricciones que moldean el diseño:

- **Hardware**: LilyGO T-Beam v1.1, ESP32-D0WD (dos núcleos, 520 KB de SRAM interna), SX1276 en 868 MHz, GPS no utilizado. Algunas revisiones incluyen PMU AXP192.
- **Bluetooth**: APRSdroid sólo habla SPP (Bluetooth Classic), y el firmware oficial de Meshtastic no implementa SPP. Eso descarta tanto "usar Meshtastic tal cual" como cualquier placa sin BT clásico (S3/C3).
- **Meshtastic**: no existe un portnum de APRS; los portnums privados van de 256 a 511 y son invisibles para la app oficial. El paquete máximo útil ronda los 200-240 bytes.
- **Radio compartida**: el ESP32 reparte por tiempo la radio de 2.4 GHz entre Bluetooth Classic y WiFi. En este diseño el WiFi existe sólo como punto de acceso de configuración.
- **Sin internet**: el equipo no incorpora cliente APRS-IS ni se asocia a redes externas. Todo el tráfico vive entre el cliente APRS, la malla y sus vecinos.
- **Antecedente**: los gateways tipo `aprstastic` resuelven el puente semántico en una Raspberry Pi con conectividad y registro previo de callsigns. Este proyecto difiere en dos ejes: no hay computadora intermediaria y no hay dependencia de internet.

## Goals / Non-Goals

**Goals:**

- Un único equipo flasheable que se comporte como TNC KISS por SPP y TCP, sin software en el cliente.
- Interoperar con nodos Meshtastic estándar usando sólo texto de canal, sin portnums propietarios ni app propia.
- Entregar el aire con disciplina: el equipo nunca debe ser la causa de saturación de un canal compartido.
- Operar de forma autónoma, sin internet, sin cuentas y sin servicios externos.
- Ser restaurable: el mismo T-Beam debe poder volver al firmware oficial de Meshtastic sin cirugía.

**Non-Goals:**

- Fidelidad completa al protocolo Meshtastic: no hay ACKs, administración remota, DMs PKI ni reemisión de tráfico ajeno.
- Puente con APRS-IS o con cualquier red de internet.
- Representación cartográfica: sin `POSITION_APP`, sin mapa.
- Autonomía de posición: el beacon lo genera el cliente APRS, no el GPS del equipo.
- Actualizaciones OTA en v1.

## Decisions

### D1 · Firmware propio con un subconjunto de Meshtastic, en lugar de forkear el firmware oficial

Un fork daría administración desde la app y disciplina de aire ya resuelta, pero obligaría a habilitar Bluedroid Classic junto a NimBLE en un firmware que hoy es BLE-only, a pelear con `Router`/`Modules` para inyectar tráfico y a seguir un upstream que se mueve rápido. El subconjunto necesario es chico: `MeshPacket`, `Data` y `User`, hash de canal, AES-CTR y los parámetros de radio.

**Alternativa descartada**: fork del firmware oficial. Se reconsideraría sólo si en el futuro se necesita configuración desde la app.

### D2 · Texto TNC2 con prefijo como portador, en lugar de un portnum privado binario

El requisito es que las tramas "lleguen como texto puro". Un portnum privado sería más eficiente y fiel, pero invisible para cualquier nodo que no corra este firmware. El texto tiene tres ventajas: lo lee un humano en la app estándar, lo parsea una máquina, y no requiere tocar el firmware de nadie más.

**Alternativas descartadas**: `PRIVATE_APP` binario (invisible), y protobuf propio sobre portnum privado (mismo problema, más código).

### D3 · El path viaja en el texto; el recorte es una opción de aire

El texto que circula por la malla es la trama TNC2 completa, incluido su path de digipeaters. El path no significa nada para la malla, pero sí para el extremo APRS-IS: si se descarta acá, el gateway remoto ya no puede reconstruirlo. El recorte queda como una opción de configuración, apagada por defecto, para quien prefiera ahorrar aire a costa de la fidelidad.

**Trade-off**: con `WIDE1-1,WIDE2-1` el texto crece unos 14 bytes (~30% de una trama típica). Con un beacon cada pocos minutos y LONG_FAST es despreciable; con tráfico alto no lo es, y ahí se justifica activar el recorte sabiendo que el paquete llegará a APRS-IS sin digis de RF.

**Alternativa descartada**: recortar siempre y que el gateway invente un path. Inventar información en un paquete APRS es peor que perderla.

### D4 · Prefijo corto y fijo (`A#`)

Dos bytes de overhead sobre ~44 de trama. Un prefijo más largo desperdicia aire en cada paquete; detectar "forma TNC2" sin prefijo produce falsos positivos con el chat normal del canal y hace imposible distinguir intención.

**Trade-off aceptado**: el prefijo ensucia un poco la lectura humana del canal. Queda fijo en `lib/mesh` para que tracker y gateway no puedan desincronizarse: un prefijo distinto entre equipos produce silencio total sin ningún error visible. Cambiarlo exige recompilar ambos.

### D5 · KISS por SPP y TCP en simultáneo, con transmisión serializada

SPP cubre Android; TCP cubre Direwolf/Xastir y sirve de banco de pruebas sin teléfono. Permitir ambos a la vez es casi gratis si la cola de transmisión es única y cada trama se publica completa.

### D6 · Bluedroid SPP (`BluetoothSerial`) sobre el core Arduino-ESP32

Es la vía probada para SPP en ESP32 y la que hablan los clientes APRS existentes. BLE queda descartado por incompatibilidad con APRSdroid, no por preferencia técnica.

### D7 · Protobuf mínimo escrito a mano, sin generador

Sólo se necesitan tres mensajes y un puñado de campos. Un codificador/decodificador manual evita la cadena de generación de código y deja el binario chico, que es lo que importa con Bluedroid y lwIP en 520 KB.

### D8 · Nodo `CLIENT_MUTE` que no reemite

No reemitir elimina la necesidad de deduplicar el reenvío, de reescribir cabeceras de relay y de asumir responsabilidad sobre tráfico ajeno. El alcance multihop se conserva porque los relays de la malla reenvían los paquetes del equipo igual. Declararse `CLIENT_MUTE` es además honesto: la malla no espera que este nodo reenvíe.

### D9 · Deduplicación en dos ventanas

| Ventana | Clave | Motivo |
|---|---|---|
| Subida KISS → malla | `(src, payload)` 30 s | APRSdroid reintenta 5 veces sin ACK |
| Recepción de malla → KISS | `(from, id)` 60 s | El flood entrega el mismo paquete por varios relays |

### D10 · AP permanente, portal disponible en operación

El portal es la única interfaz de administración, así que tiene que funcionar sin PC ni app y tiene que poder mostrar estado mientras el equipo opera: el AP se levanta en los dos modos y el portal queda accesible en `192.168.4.1`. No hace falta STA: el equipo nunca se asocia como cliente WiFi. Guardar una configuración válida reinicia el equipo y aplica canal, clave, región y preset; el recorte de path y los límites se aplican al configurar el enlace.

**Trade-off**: el AP convive con SPP y LoRa todo el tiempo, no sólo durante la administración. La tarea 1.6 mide la latencia de SPP con el portal activo; si molesta, la siguiente iteración puede apagar el WiFi tras un tiempo sin clientes web.

El botón es GPIO0, que también es strapping pin: mantenerlo pulsado durante el reset entra en el descargador ROM, no en la aplicación. Por eso la entrada manual al portal es una pulsación larga de tres segundos en operación, con una bandera en RTC que sobrevive al reinicio por software.

### D11 · El equipo no implementa APRS-IS ni conexión saliente

Es una decisión de arquitectura, no una omisión: un tracker embarcado no debe depender de credenciales, DNS, NTP ni del estado de un servidor remoto. Quitarlo elimina el cliente TCP, la tabla de flujos, el manejo de passcodes y la superficie de error más grande del diseño.

**Alternativas descartadas**: igate de subida (dependencia de internet y de política de APRS-IS), bajada con pinhole (requiere un origen remoto que ya no existe).

### D12 · Estructura de módulos

```
src/
  main.cpp              arranque, modos, lazo principal
  config_store.*        blob NVS atómico
  portal.*              formulario, validación y página de estado
  transport/            adaptadores Arduino: SPP y servidor KISS TCP
lib/kiss/               codec KISS, AX.25 ↔ TNC2, hub de clientes
lib/mesh/               protobuf, cabecera, AES-CTR, texto A#, dedupe, cola de aire
lib/bridge/             pipeline KISS ↔ malla, eco y contadores
lib/config/             modelo, validación y base64 de la PSK
```

El radio sigue pendiente: `mesh_radio.*` y `mesh_node.*` se agregan en las tareas 3.3-3.6.

### D13 · Modelo de datos de configuración

`Config`: callsign, nombre BT, canal, PSK (base64, como la muestra la app de Meshtastic), región, preset, recorte de path, tasa máxima de subida e intervalo mínimo entre transmisiones. Vive en NVS como un único blob validado antes de escribirse; sin callsign válido el equipo arranca en modo portal y no inicia el enlace.

### D14 · El contrato de texto entre firmwares

La interfaz con el gateway es una línea TNC2 completa precedida por el prefijo fijo `A#`, en ASCII imprimible y con un máximo de unos 200 bytes. El tracker no reescribe el path de la bajada: entrega lo que venga en el texto. Así cada extremo decide las reglas de su dominio —radio APRS en el tracker, internet en el gateway— sin compartir estado.

**Trade-off**: no hay handshake ni versión negociada. Un desajuste de canal, preset o PSK se manifiesta como silencio. El diagnóstico vive del lado del gateway, que sí ve la malla y puede contar lo que recibe.

## Risks / Trade-offs

- **Coexistencia BT clásico + WiFi permanente** → el AP queda arriba en operación para que el portal muestre estado; medir latencia de SPP con el portal activo (tarea 1.6) y, si molesta, apagar el WiFi tras un tiempo sin clientes web.
- **Presión de RAM** (Bluedroid + lwIP + LoRa en 520 KB) → buffers estáticos, colas acotadas, sin asignaciones dinámicas en el camino caliente; verificar si la unidad tiene PSRAM.
- **Sin internet no hay persistencia remota** → si en el momento de transmitir no hay ningún nodo de la malla en alcance, la trama se pierde. Es inherente a un tracker local; se mitiga con contadores visibles y con la disciplina de reintento del cliente APRS.
- **Legalidad del enlace** → la malla opera en banda SRD, no de radioaficionados. El proyecto debe advertirlo explícitamente y no asumir licencia del operador.
- **Visibilidad del canal** → todo lo que el equipo publica lo leen todos los miembros del canal. Se documenta; para tráfico sensible se recomienda canal privado.
- **Deriva del protocolo Meshtastic** → fijar versiones del esquema protobuf, guardar copia local de `portnums.proto`/`mesh.proto`, y probar contra nodos estándar en cada release.
- **Sin confirmación de transmisión** → el cliente APRS nunca sabrá si la trama llegó a la malla. Se asume y se compensa con deduplicación y contadores visibles en el portal.
- **Variantes de placa** (AXP192, PSRAM, versión de SX1276) → detectar en arranque y documentar la revisión soportada.

## Migration Plan

1. Banco: flashear por USB con PlatformIO y validar KISS con Direwolf antes de tocar radio.
2. Provisionar por portal (callsign, canal, PSK, región, preset) y verificar interop con un nodo Meshtastic estándar.
3. Prueba de campo con el cliente APRS real y ajuste de los límites de aire según el preset elegido.
4. Rollback: el T-Beam se restaura al firmware oficial de Meshtastic con el flasher web; no hay estado persistente incompatible.

## Open Questions

- Latencia real de SPP con el portal AP activo en esta placa.
- Presencia de PSRAM en la unidad concreta del operador.
