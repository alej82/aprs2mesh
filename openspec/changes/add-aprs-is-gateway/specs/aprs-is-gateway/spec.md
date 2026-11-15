## Purpose

Conecta la malla Meshtastic con APRS-IS: publica el tráfico que llega del canal y admite únicamente las respuestas dirigidas a estaciones que iniciaron un intercambio reciente, sin inundar el aire compartido con tráfico de internet.

## ADDED Requirements

### Requirement: Sesión APRS-IS con credenciales y software experimental

El equipo SHALL abrir una sesión TCP con el servidor APRS-IS configurado usando el callsign del gateway, el passcode y un identificador de software que comience con `APZ`, y SHALL declarar el filtro configurado en la misma línea de login.

#### Scenario: Autenticación aceptada
- **WHEN** el equipo se conecta con callsign y passcode válidos
- **THEN** el servidor acepta el login y el equipo queda habilitado para inyectar tráfico

#### Scenario: Passcode rechazado
- **WHEN** el servidor indica que el login no está verificado
- **THEN** el equipo no inyecta ninguna trama, registra el error y lo refleja en el estado del portal

#### Scenario: Filtro enviado como una sola línea
- **WHEN** el operador configura un filtro de recepción
- **THEN** el equipo lo envía al servidor en una única línea, sin agregar instrucciones propias

### Requirement: Keepalive y reconexión

El equipo SHALL mantener la sesión activa y SHALL reconectarse con espera creciente ante caídas del servidor o de la red, sin interrumpir la recepción de la malla ni el monitor KISS.

#### Scenario: Caída de la red
- **WHEN** se pierde la conexión con APRS-IS
- **THEN** el equipo sigue recibiendo textos del canal y reintenta la conexión con espera creciente

#### Scenario: Servidor cierra la sesión
- **WHEN** el servidor cierra la conexión por inactividad o mantenimiento
- **THEN** el equipo vuelve a conectarse y reenvía el filtro sin intervención del operador

### Requirement: Subida exclusiva de textos recibidos del canal

El equipo SHALL inyectar en APRS-IS únicamente los textos con prefijo `A#` que recibió desde la malla, SHALL excluir los paquetes originados por él mismo y SHALL inyectar una sola vez los duplicados dentro de la ventana de deduplicación.

#### Scenario: Texto propio retransmitido
- **WHEN** el equipo recibe de vuelta un texto que él mismo publicó como bajada
- **THEN** no lo inyecta en APRS-IS

#### Scenario: Duplicado por relay
- **WHEN** el mismo texto llega dos veces por caminos distintos dentro de la ventana
- **THEN** se inyecta una sola vez

#### Scenario: Texto originado en APRS-IS
- **WHEN** el equipo recibe un paquete desde APRS-IS y lo publica en la malla
- **THEN** ese paquete no se vuelve a inyectar en APRS-IS

### Requirement: Construcción del paquete inyectado

El equipo SHALL inyectar la trama tal como llegó del canal, conservando su path de digipeaters cuando esté presente, y SHALL agregar el construct `qAR` con el callsign del gateway.

#### Scenario: Trama con path de RF
- **WHEN** sube un texto con path `WIDE1-1,WIDE2-1`
- **THEN** el paquete inyectado conserva ese path y agrega `qAR` con el callsign del gateway

#### Scenario: Trama sin path
- **WHEN** sube un texto sin digipeaters
- **THEN** el paquete inyectado no inventa un path y agrega igualmente `qAR` con el callsign del gateway

### Requirement: Buffer corto durante cortes de internet

El equipo SHALL retener las subidas pendientes durante un máximo de 30 segundos mientras la sesión no esté disponible, y SHALL descartar el excedente contabilizándolo en lugar de acumularlo indefinidamente.

#### Scenario: Corte breve
- **WHEN** la sesión cae y se restablece dentro de los 30 segundos
- **THEN** las tramas pendientes se inyectan al reconectar

#### Scenario: Corte prolongado
- **WHEN** una trama supera los 30 segundos de espera
- **THEN** se descarta, se cuenta el descarte y no se inyecta retroactivamente

### Requirement: Apertura de flujos de bajada

El equipo SHALL abrir o refrescar un flujo para el callsign de origen de cada trama inyectada con éxito, y SHALL limitar la cantidad de flujos simultáneos descartando el más antiguo cuando se exceda el máximo configurado.

#### Scenario: Inyección exitosa
- **WHEN** una trama originada por `EA1ABC-9` se inyecta correctamente
- **THEN** el flujo de `EA1ABC-9` queda abierto con la marca de tiempo actual

#### Scenario: Tabla llena
- **WHEN** se abre un flujo y ya existen tantos como el máximo configurado
- **THEN** el flujo más antiguo se descarta para dar lugar al nuevo

### Requirement: Admisión de bajada por pinhole con TTL

El equipo SHALL admitir un paquete de APRS-IS únicamente si es un mensaje APRS cuyo destinatario coincide con el callsign de un flujo vigente, considerando el TTL configurable (60 minutos por defecto). Todo paquete que no cumpla esa condición SHALL descartarse y contabilizarse.

#### Scenario: Respuesta dentro del TTL
- **WHEN** llega un mensaje dirigido a una estación con flujo abierto hace menos del TTL
- **THEN** el equipo lo admite y lo publica en el canal

#### Scenario: Respuesta después del TTL
- **WHEN** llega un mensaje dirigido a una estación cuyo flujo expiró
- **THEN** el equipo lo descarta y registra el motivo

#### Scenario: Tráfico no solicitado
- **WHEN** llega un beacon, un objeto, un boletín o un paquete de terceros
- **THEN** el equipo lo descarta aunque provenga de una estación con flujo abierto

### Requirement: Reescritura del path de bajada

El equipo SHALL eliminar los constructs `q` y el pseudo-digi `TCPIP*` que vengan de APRS-IS, y SHALL publicar el paquete con un path compuesto únicamente por `TCPIP*` marcado como repetido, de modo que quede identificado como tráfico originado en internet.

#### Scenario: Paquete con q-construct
- **WHEN** APRS-IS entrega un paquete con path `TCPIP*,qAC,T2SPAIN`
- **THEN** el texto publicado en la malla lleva como único digi `TCPIP*`

#### Scenario: Paquete de una estación local vía internet
- **WHEN** APRS-IS entrega un paquete que ya fue escuchado por radio en otra región
- **THEN** el equipo no conserva el path de radio ajeno y lo marca igualmente como `TCPIP*`

### Requirement: Límites de tasa y deduplicación de bajada

El equipo SHALL limitar los mensajes de bajada publicados en la malla a una tasa máxima configurable y un intervalo mínimo entre ellos, y SHALL descartar los duplicados del mismo mensaje dentro de la ventana de deduplicación.

#### Scenario: Ráfaga de respuestas
- **WHEN** llegan más mensajes admitidos de los que permite el límite de bajada
- **THEN** el equipo publica hasta el límite y contabiliza el excedente

#### Scenario: Duplicado de varios igates
- **WHEN** el mismo mensaje llega repetido por distintas rutas de APRS-IS
- **THEN** se publica una sola vez en el canal

### Requirement: Política abierta con trazabilidad

El equipo SHALL aceptar como candidatos de subida cualquier texto con prefijo `A#` proveniente del canal, sin allowlist de nodos ni de callsigns, y SHALL registrar por cada inyección el número de nodo de origen, el callsign de la trama y el resultado. El operador SHALL poder cerrar el gate desde el portal sin reiniciar el equipo.

#### Scenario: Inyección desde un nodo desconocido
- **WHEN** llega un texto con prefijo desde un nodo que el gateway nunca vio
- **THEN** el equipo lo inyecta si la sesión está habilitada y registra nodo, callsign y resultado

#### Scenario: Gate cerrado
- **WHEN** el operador cierra el gate
- **THEN** el equipo deja de inyectar de inmediato, sigue recibiendo la malla y el monitor sigue funcionando

#### Scenario: Límite de subida excedido
- **WHEN** llegan más tramas por minuto que el límite configurado
- **THEN** el equipo descarta el excedente y lo contabiliza
