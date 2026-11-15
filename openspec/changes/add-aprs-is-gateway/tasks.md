## 1. Base y código compartido

- [x] 1.1 Crear el entorno `server_tbeam` en `platformio.ini` sobre la misma placa, sin enlazar `BluetoothSerial`
- [x] 1.2 Extraer AX.25/TNC2 de `lib/kiss` a `lib/aprs` y actualizar las referencias del tracker
- [x] 1.3 Verificar que `lib/mesh` expone dedupe y cola de transmisión; si el change del tracker aún no las entregó, implementarlas acá
- [x] 1.4 Medir el tamaño del binario sin Bluedroid y decidir si usa la partición por defecto o `huge_app.csv`
- [x] 1.5 Implementar la identidad del gateway: `long_name` igual al callsign, rol `CLIENT_MUTE`, hop limit 3 y NodeInfo periódico

## 2. Enlace de malla en el gateway

- [x] 2.1 Recibir textos del canal, exigir el prefijo fijo `A#` y validar la forma TNC2 antes de aceptarlos
- [x] 2.2 Publicar textos en el canal usando la cola con límite de tasa e intervalo mínimo
- [x] 2.3 Deduplicar la recepción por `(from, id)` con ventana de 60 s
- [x] 2.4 Suprimir el eco de las tramas propias comparando el número de nodo

## 3. Cliente APRS-IS

- [x] 3.1 Implementar la conexión TCP y la línea de login con callsign, passcode, identificador `APZ` y filtro
- [x] 3.2 Parsear las líneas entrantes, descartar los comentarios `#` y separar paquetes de mensajes del servidor
- [x] 3.3 Detectar el login verificado y no verificado, y bloquear la inyección en el segundo caso
- [x] 3.4 Implementar keepalive y reconexión con espera creciente sin afectar la malla ni el monitor
- [x] 3.5 Implementar el buffer de subida de 30 s con descarte contabilizado

## 4. Subida

- [x] 4.1 Construir la línea a inyectar conservando el path recibido y agregando `qAR` con el callsign del gateway
- [x] 4.2 Aplicar deduplicación de 30 s y límite de tasa de subida
- [x] 4.3 Registrar cada inyección con nodo de origen, callsign y resultado

## 5. Bajada

- [x] 5.1 Implementar la tabla de flujos: apertura al inyectar, refresco, TTL configurable, máximo de ocho y evicción del más antiguo
- [x] 5.2 Parsear mensajes APRS y comparar el addressee contra los flujos vigentes
- [x] 5.3 Admitir solo mensajes con flujo vigente; descartar beacons, objetos, boletines y paquetes de terceros
- [x] 5.4 Eliminar los constructs `q` y publicar con el path `TCPIP*` marcado como repetido
- [x] 5.5 Aplicar límite de tasa de bajada y deduplicación por `(src, msgid, texto)`
- [x] 5.6 Publicar cada bajada admitida como un único texto con prefijo en la cola de malla

## 6. Monitor KISS

- [x] 6.1 Implementar el servidor KISS/TCP de solo lectura con varios clientes simultáneos
- [x] 6.2 Emitir al monitor las subidas aceptadas y las bajadas publicadas como tramas AX.25 UI
- [x] 6.3 Descartar y contar las escrituras de los clientes, y expulsar a los que dejen de aceptar bytes

## 7. Portal del gateway

- [x] 7.1 Implementar `Config` en NVS con escritura atómica y gate deshabilitado por defecto
- [x] 7.2 Implementar la validación por campo, incluido el filtro en una sola línea y el passcode numérico
- [x] 7.3 Configurar el punto de acceso `aprs2mesh-gw` en modo AP+STA simultáneo
- [x] 7.4 Implementar los modos de arranque con entrada manual al portal
- [x] 7.5 Mostrar estado de sesión, flujos con antigüedad, último paquete y contadores separados de recibidos, admitidos y rechazados
- [x] 7.6 Implementar el interruptor del gate con efecto inmediato y sin reinicio

## 8. Validación

- [x] 8.1 Construir un APRS-IS falso en host y un lazo tracker↔gateway en memoria para el ciclo completo
- [x] 8.2 Probar en host la apertura y expiración de flujos, la admisión, la deduplicación y el buffer de 30 s
- [x] 8.3 Probar en host la limpieza de `qAC`/`qAR` y la publicación con `TCPIP*`
- [ ] 8.4 Verificar en banco la subida real y comprobar el paquete en `aprs.fi`
- [ ] 8.5 Verificar en banco la bajada por pinhole y el monitor con Direwolf
- [x] 8.6 Documentar la política abierta, la marca `TCPIP*`, el uso de `APZ`/`qAR` y los riesgos asociados
- [x] 8.7 Documentar flasheo, provisión del segundo equipo y rollback al firmware oficial de Meshtastic
