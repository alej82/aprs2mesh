## Context

Segundo rol del sistema. El objetivo y el alcance están en `proposal.md`; el comportamiento exigido, en `specs/`. El contrato de texto que este equipo consume —prefijo fijo `A#`, línea TNC2 completa, sin reescritura por parte del tracker— está definido en el design de `add-kiss-mesh-gateway` (D14), que sigue abierto.

Restricciones que moldean el diseño:

- **Hardware**: segundo LilyGO T-Beam v1.1. No necesita Bluetooth Classic; solo LoRa, WiFi y USB para flashear.
- **Sin reloj**: no hay NTP ni RTC. APRS-IS en el puerto 14580 es TCP en claro y no requiere hora; los TTL y las tasas usan tiempo monótono. Las marcas de tiempo de los logs son relativas al arranque.
- **Memoria**: sin Bluedroid el presupuesto es mucho más holgado que en el tracker, pero lwIP y el servidor web siguen compartiendo los 520 KB con el búfer de radio.
- **Canal compartido**: el gateway publica en el mismo canal que usan las personas; el airtime se negocia con el mismo límite de tasa que el tracker.
- **OpenSpec**: `add-kiss-mesh-gateway` todavía no está archivado, así que este cambio introduce capacidades nuevas y no modifica las suyas. El contrato compartido se corrigió allí, no acá.

## Goals / Non-Goals

**Goals:**

- Cerrar el circuito APRS ↔ malla sin computadora intermediaria.
- Hacer observable un enlace que por diseño no tiene confirmaciones: el monitor KISS y los contadores son la única forma de saber qué pasó.
- Mantener el tráfico de internet acotado por admisión, tasa y switch, para no degradar el canal.
- Reutilizar el enlace de malla del tracker sin duplicar código.

**Non-Goals:**

- Allowlist de nodos o callsigns, firmas o HMAC: v1 es abierto al canal.
- Reproducir fielmente todas las reglas de un igate APRS: solo se normaliza el path y se filtra por pinhole.
- Interfaz web remota, OTA, o administración desde la app oficial de Meshtastic.
- Cambios en el firmware del tracker.

## Decisions

### D1 · Igate propio con monitor KISS, en lugar de un bridge KISS hacia Direwolf

Un segundo tracker haciendo de bridge KISS dejaría que Direwolf hiciera todo el trabajo de APRS-IS sin escribir código nuevo, y sería más robusto (disco, logs, filtros maduros). Pero exige una computadora encendida, que es exactamente lo que el proyecto quiere evitar. El monitor KISS recupera la parte de observabilidad que se perdería, y el igate propio mantiene el equipo autónomo.

**Alternativa descartada**: bridge KISS + Direwolf en una Pi. Se documenta como plan B para quien prefiera la robustez de Direwolf sobre la autonomía.

### D2 · El gateway no habla AX.25 salvo para el monitor

La inyección a APRS-IS es una línea de texto y la publicación en la malla es otro texto: el gateway nunca necesita construir ni parsear AX.25 en su camino principal. Solo el monitor lo hace, para entregar tramas a clientes KISS. Eso justifica separar `ax25` de `lib/kiss` en una librería `lib/aprs`, y deja el camino caliente sin conversiones.

### D3 · Política abierta con trazabilidad, no allowlist

El canal es privado con PSK propia, así que la clave funciona como membresía y una allowlist duplicaría esa decisión en dos lugares. El riesgo que queda es real y hay que decirlo: cualquiera con la PSK puede inyectar bajo cualquier callsign. Se compensa con registro por inyección (nodo, callsign, resultado), límite de tasa y un interruptor de cierre inmediato.

**Alternativa descartada para v1**: allowlist de nodos y callsigns. Queda como evolución natural si el canal se comparte con más gente.

### D4 · Filtro de APRS-IS escrito por el operador, saneado y no interpretado

Elegir filtros por el usuario hubiera requerido un parser y una interfaz que nunca alcanza. El campo de texto libre le da todo el poder de APRS-IS, y el gateway solo lo concatena en la línea de login. La sanitización es obligatoria: sin saltos de línea ni caracteres de control, porque el valor termina dentro de un protocolo de líneas.

**Trade-off**: un filtro demasiado estrecho produce silencio sin error. Por eso el estado del portal separa "recibidos" de "admitidos".

### D5 · Bajada marcada con `TCPIP*` y limpieza de constructs `q`

Un paquete que viene de internet no debe parecer escuchado por radio. Se descartan `qAC`, `qAR`, `qAS` y el pseudo-digi `TCPIP*` original, y se publica con un único `TCPIP*` marcado como repetido. Es la convención que usan los igates para que otro igate no lo vuelva a inyectar.

**Trade-off**: la marca es una convención, no un candado. Un cliente APRS mal configurado todavía puede reinyectar el paquete; la deduplicación de APRS-IS lo absorbe, pero la documentación debe advertirlo.

### D6 · Flujos observados localmente, sin estado compartido

El gateway no coordina nada con el tracker: ve el mismo texto de subida en el canal y abre el flujo por su cuenta. No hay protocolo entre equipos, no hay sesión que mantener y un reinicio del gateway solo vacía la tabla de flujos. La contrapartida es que el TTL se mide desde que el gateway vio la subida, no desde que el tracker la originó; la diferencia es de milisegundos.

### D7 · Buffer de subida de 30 segundos

APRS-IS deduplica en una ventana de 30 segundos, así que reinyectar algo más viejo es peor que perderlo: genera duplicados que los demás igates ya filtraron y ensucia los logs. El buffer es acotado, descarta lo más viejo y cuenta cada descarte.

### D8 · Sin Bluetooth: binario más chico y placa potencialmente más barata

El gateway no tiene SPP que proteger, así que WiFi y LoRa conviven sin el turnado de radio que complicaba al tracker. Además, el binario no enlaza Bluedroid; si entra en la partición por defecto, se evita `huge_app.csv`. Y abre la puerta a un S3/C3 en el futuro, aunque v1 se compila para T-Beam.

### D9 · Reorganización de librerías compartidas

```
lib/aprs/    ax25, TNC2 y framing KISS (nuevo, extraído de lib/kiss)
lib/kiss/    hub de clientes KISS      (solo tracker)
lib/mesh/    proto, cifrado, frame, dedupe, cola de TX
src/         main del tracker
src/gateway/ main del gateway, cliente APRS-IS, flujos, admisión, monitor
```

El tracker se actualiza para incluir `lib/aprs`; el gateway enlaza `lib/aprs`, `lib/mesh` y `lib/gateway` pero no `lib/kiss`. El codec KISS se movió con AX.25 porque el monitor también lo necesita para entregar tramas a sus clientes; el hub sí es exclusivo del tracker.

### D10 · Tres contadores como diagnóstico de primera clase

Un gateway tiene dos formas de no entregar nada: que el filtro no traiga paquetes, o que el pinhole los rechace. Sin separarlos, el operador no sabe qué mirar. El estado expone **recibidos**, **admitidos** y **rechazados por admisión**, más inyectados, descartados y retenidos.

### D11 · Identidad del gateway en la malla

`long_name` igual al callsign del gateway (con SSID distinto al del tracker, por ejemplo `-10`), rol `CLIENT_MUTE`, `hop_limit` configurable (3 por defecto) y NodeInfo periódico. No reemite tráfico ajeno. El callsign es también el de la línea de login y el del construct `qAR`.

### D12 · Entrada manual al portal con pulsación larga

La T-Beam v1.1 no tiene botón BOOT: sus tres botones son PWR, IO38 y RST, y GPIO0 sólo es el pin de strapping. La entrada manual es una pulsación de tres segundos en el botón de usuario (IO38) en operación, que deja una bandera en RTC y reinicia al soltar, igual que en el tracker. Es la única forma de forzar el portal sin un botón dedicado.

### D13 · Módem compartido con el tracker

El gateway no define su propio modulador: reutiliza el bloque de módem de `lib/config` para región, preset, parámetros `CUSTOM` y número de saltos. Así el portal del gateway expone los mismos campos, los mismos valores por defecto y la misma validación que el tracker, y un desajuste de radio se detecta comparando las huellas efectivas de los dos portales. El resto de la configuración del gateway —APRS-IS, WiFi, flujos y tasas— sigue siendo independiente.

El detalle de la resolución del módem, los rangos admitidos y la estimación de aire está en el diseño del tracker (D15), para no duplicar la decisión. En el portal del gateway la sugerencia de aire se aplica a `downlinkPerMinute`, que es la tasa que consume LoRa; la subida va a APRS-IS y no gasta aire de malla.

## Risks / Trade-offs

- **Relay abierto hacia APRS-IS** → mitigado con registro, tasa y switch; no autenticado. Si el canal deja de ser privado, hay que volver a la allowlist.
- **`TCPIP*` no impide bucles** → un cliente APRS que igatea puede reinyectar la bajada; APRS-IS la deduplica, pero se documenta el efecto.
- **Punto único de falla** → si el gateway se cae, el tracker sigue emitiendo a la malla y nada llega a APRS-IS. Sin ACKs, solo el monitor lo revela.
- **Filtro mal escrito** → silencio con apariencia de normalidad; los tres contadores son la defensa.
- **Airtime de bajada** → el pinhole y el límite de tasa acotan el volumen, pero un flujo con conversación activa puede competir con el chat del canal.
- **Coexistencia WiFi AP+STA + LoRa** → sin Bluetooth hay menos presión, pero hay que medir la recepción durante transferencias.
- **Política de APRS-IS** → identificador `APZ`, `qAR` y registro; el enlace no es RF y el paquete lo declara solo parcialmente. Conviene documentarlo antes de publicar.
- **Legalidad** → banda SRD, licencia de radioaficionado y responsabilidad sobre lo que se inyecta.

## Migration Plan

1. Compilar `server_tbeam` y medir tamaño; decidir partición.
2. Provisionar por portal: callsign del gateway, credenciales APRS-IS, filtro conservador, y canal, PSK, módem y saltos iguales al tracker.
3. Banco: verificar login verificado, subida con `qAR` y bajada por pinhole con un APRS-IS real, mirando el monitor con Direwolf.
4. Campo: comprobar el paquete en `aprs.fi` y la respuesta en el cliente APRS del tracker.
5. Rollback: el T-Beam vuelve al firmware oficial de Meshtastic con el flasher web.

## Open Questions

- ~~¿El binario sin Bluedroid entra en la partición por defecto de 1.31 MB?~~ Resuelto: 960 KB y entra, sin `huge_app.csv`.
- ¿`aprsc` acepta el `qAR` de este login con el comportamiento esperado? (tarea 8.4)
- ¿Qué latencia y pérdida introduce la coexistencia AP+STA con la recepción LoRa a la tasa prevista? (tarea 8.5)
