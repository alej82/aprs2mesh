## 1. Base del proyecto y del banco

- [x] 1.1 Crear el proyecto PlatformIO para T-Beam v1.1 (core Arduino-ESP32, RadioLib) y la estructura de `src/` descrita en el diseño
- [ ] 1.2 Inicializar la PMU AXP192 cuando la revisión la incluya y verificar alimentación de LoRa y GPS
- [ ] 1.3 Medir RAM libre en arranque con Bluedroid, WiFi y RadioLib inicializados, y registrar la línea base
- [ ] 1.4 Implementar un eco SPP mínimo y verificar emparejamiento y conexión desde un cliente real
- [ ] 1.5 Implementar un servidor TCP de eco en el puerto 8001 y verificarlo con `nc` y con Direwolf
- [ ] 1.6 Medir latencia y pérdida de SPP con el punto de acceso de configuración activo, y dejar registrado el resultado

## 2. Capa KISS y AX.25

- [x] 2.1 Implementar el codec KISS: delimitación con FEND, escape FESC/TFEND/TFESC, resincronización y tamaño máximo
- [x] 2.2 Clasificar tramas KISS: datos por puerto 0, comandos de módem consumidos, tipos desconocidos ignorados
- [x] 2.3 Escribir pruebas unitarias del codec con vectores que incluyan `0xC0` y `0xDB` embebidos, tramas vacías y basura
- [x] 2.4 Implementar el parser AX.25 → TNC2 con validación de longitudes y separación de origen, destino, path e información
- [x] 2.5 Implementar el constructor TNC2 → AX.25 UI con control `0x03`, PID `0xF0` y direcciones desplazadas
- [x] 2.6 Escribir pruebas de ida y vuelta AX.25 ↔ TNC2 y casos malformados
- [ ] 2.7 Conectar el transporte SPP real al codec, reemplazando el eco de prueba
- [ ] 2.8 Conectar el transporte TCP real con varios clientes y escritura serializada
- [ ] 2.9 Validar la capa completa con Direwolf: enviar y recibir una trama por KISS TCP sin radio

## 3. Enlace de malla

- [x] 3.1 Implementar el codificador/decodificador protobuf manual de `MeshPacket`, `Data` y `User`, con pruebas
- [x] 3.2 Implementar el hash de canal y AES-CTR con nonce `(id, from)`, con pruebas contra vectores conocidos
- [ ] 3.3 Configurar RadioLib por región y preset (frecuencia, SF, BW, CR, sync word, preamble, CRC)
- [ ] 3.4 Transmitir un `TEXT_MESSAGE_APP` de prueba y verificarlo en un nodo Meshtastic estándar
- [ ] 3.5 Recibir y descifrar un `TEXT_MESSAGE_APP` emitido por un nodo estándar
- [ ] 3.6 Anunciar NodeInfo con `long_name` igual al callsign, rol `CLIENT_MUTE` y número de nodo derivado del hardware
- [ ] 3.7 Filtrar la recepción por prefijo configurado y descartar los textos que no lo llevan
- [ ] 3.8 Implementar deduplicación de recepción por `(from, id)` con ventana de 60 s
- [ ] 3.9 Implementar la cola de transmisión acotada con límite de tasa e intervalo mínimo, y sus contadores de descarte
- [ ] 3.10 Descartar en silencio paquetes no descifrables, de otros canales o de portnums distintos de texto

## 4. Integración KISS ↔ malla

- [ ] 4.1 Implementar el pipeline de subida: KISS → AX.25 → TNC2 → recorte de path → prefijo → cola de malla
- [ ] 4.2 Implementar deduplicación de subida por `(src, payload)` con ventana de 30 s
- [ ] 4.3 Implementar el pipeline de bajada: prefijo → validación TNC2 → AX.25 → entrega a todos los clientes KISS
- [ ] 4.4 Suprimir el eco de las tramas propias comparando el número de nodo
- [ ] 4.5 Rechazar y registrar textos o tramas que excedan el tamaño máximo admitido
- [ ] 4.6 Exponer contadores de tramas subidas, bajadas y descartadas con su motivo

## 5. Configuración y portal

- [ ] 5.1 Implementar `Config` en NVS con escritura atómica y valores por defecto que dejen el equipo en modo portal
- [ ] 5.2 Implementar la validación de callsign, PSK, región y preset con error por campo y conservación de la configuración previa
- [ ] 5.3 Implementar el punto de acceso protegido y el servidor web con formulario y página de estado
- [ ] 5.4 Implementar los modos de arranque: portal sin configuración válida o con el botón pulsado, y reinicio del enlace al guardar
- [ ] 5.5 Mostrar en el portal el modo, la versión, los clientes KISS, los contadores, la memoria libre y el último error
- [ ] 5.6 Verificar que el punto de acceso no interfiere con el servicio KISS ni con la recepción de la malla

## 6. Validación en banco y campo

- [ ] 6.1 Verificar que una baliza enviada desde APRSdroid por SPP aparece en el chat de un nodo estándar
- [ ] 6.2 Verificar que una respuesta escrita con el prefijo en la app aparece como paquete en APRSdroid
- [ ] 6.3 Verificar que un texto sin prefijo en el canal no genera ninguna trama KISS
- [ ] 6.4 Verificar que Direwolf por TCP envía y recibe correctamente y en paralelo con un cliente SPP
- [ ] 6.5 Medir el aire consumido por trama con los presets LONG_FAST y MEDIUM_FAST y ajustar los límites configurados
- [ ] 6.6 Verificar que el equipo funciona sin ninguna conexión a internet y que no intenta conexiones salientes
- [ ] 6.7 Documentar advertencias legales (banda SRD, licencia) y visibilidad del canal
- [ ] 6.8 Documentar flasheo, provisión inicial y rollback al firmware oficial de Meshtastic
