## Purpose

Permite configurar el gateway desde un navegador, incluyendo las credenciales y la política de APRS-IS, sin recompilar y sin depender de la aplicación oficial de Meshtastic.

## ADDED Requirements

### Requirement: Portal web en modo punto de acceso simultáneo

El equipo SHALL exponer un portal de configuración accesible en `192.168.4.1` mediante un punto de acceso WiFi protegido con clave, y SHALL poder mantener la conexión como cliente de la red configurada al mismo tiempo, para administrar el equipo sin cortar el igate.

#### Scenario: Acceso con el igate conectado
- **WHEN** el operador abre el portal mientras el equipo está conectado a APRS-IS por WiFi
- **THEN** el portal responde y el igate continúa funcionando

#### Scenario: Acceso sin credenciales de red
- **WHEN** un dispositivo intenta asociarse al punto de acceso sin la clave correcta
- **THEN** no obtiene acceso al portal

### Requirement: Parámetros configurables

El portal SHALL permitir configurar al menos: callsign del gateway, servidor y puerto de APRS-IS, passcode, filtro de recepción, habilitación del gate, TTL de flujos, cantidad máxima de flujos, límite de tasa de subida, límite de tasa de bajada, SSID y clave de la red WiFi, nombre de canal, clave precompartida del canal, región, preset de módem y número de saltos. El preset SHALL incluir una opción `CUSTOM` que habilite la configuración de frecuencia, ancho de banda, factor de dispersión y tasa de codificación, con los mismos campos y valores por defecto que el tracker.

#### Scenario: Cambio de filtro
- **WHEN** el operador modifica el filtro de recepción y guarda
- **THEN** el equipo reabre la sesión APRS-IS con el filtro nuevo

#### Scenario: Cambio de canal
- **WHEN** el operador modifica el nombre de canal o la clave precompartida
- **THEN** el equipo persiste los valores y los aplica al reiniciar el enlace de radio

#### Scenario: Módem CUSTOM
- **WHEN** el operador elige `CUSTOM` e introduce una frecuencia, un ancho de banda, un factor de dispersión y una tasa de codificación válidos
- **THEN** el gateway persiste esos valores y los usa para unirse al canal en el siguiente arranque, igual que el tracker

#### Scenario: Saltos independientes
- **WHEN** el operador cambia el número de saltos
- **THEN** el gateway aplica el nuevo límite a todos sus paquetes propios, incluidos los textos y el NodeInfo, sin alterar el modulador

#### Scenario: Valores custom conservados
- **WHEN** el operador cambia de `CUSTOM` a un preset fijo y más tarde vuelve a `CUSTOM`
- **THEN** los últimos valores custom siguen disponibles y sólo se validan mientras `CUSTOM` está activo

#### Scenario: Estimación de aire
- **WHEN** el operador edita el ancho de banda o el factor de dispersión
- **THEN** el portal muestra en vivo el aire estimado por trama, sugiere un límite de bajadas sostenible y ofrece aplicarlo, sin bloquear el guardado

### Requirement: Validación de la configuración

El equipo SHALL validar los valores antes de aplicarlos y SHALL rechazar la configuración indicando el campo inválido, conservando los valores anteriores. El filtro SHALL rechazarse si contiene saltos de línea, caracteres de control o excede la longitud máxima admitida, porque se concatena en la línea de login. En el modo `CUSTOM`, la frecuencia SHALL validarse con todo el ancho de banda ocupado dentro de la banda de la región, y el ancho de banda, el factor de dispersión y la tasa de codificación SHALL pertenecer al subconjunto admitido. El número de saltos SHALL estar entre 0 y 7.

#### Scenario: Filtro con salto de línea
- **WHEN** el operador pega un filtro que contiene un salto de línea
- **THEN** el portal rechaza el valor e indica el motivo, sin abrir una sesión modificada

#### Scenario: Passcode no numérico
- **WHEN** el operador guarda un passcode con caracteres que no son dígitos
- **THEN** el portal rechaza el cambio y conserva la configuración previa

#### Scenario: Callsign con formato inválido
- **WHEN** el operador guarda un callsign que no cumple el formato de radioaficionado
- **THEN** el portal muestra el error y no transmite con esa identidad

#### Scenario: Parámetros de módem fuera de rango
- **WHEN** el operador guarda en `CUSTOM` una frecuencia cuya ocupación se sale de la banda de la región, un ancho de banda incompatible o un factor de dispersión o tasa de codificación fuera del conjunto admitido
- **THEN** el portal rechaza el valor, indica el campo y conserva la configuración anterior

#### Scenario: Saltos fuera de rango
- **WHEN** el operador introduce un número de saltos mayor que 7
- **THEN** el portal rechaza el valor y conserva el límite anterior

### Requirement: Persistencia atómica en NVS

El equipo SHALL almacenar la configuración en memoria no volátil de forma atómica, de modo que un corte de energía durante el guardado no deje la configuración corrupta ni pierda por completo la anterior.

#### Scenario: Corte durante el guardado
- **WHEN** se interrumpe la alimentación mientras el equipo escribe la configuración
- **THEN** al arrancar utiliza la configuración anterior completa y sigue operativo

### Requirement: Modos de arranque

El equipo SHALL arrancar en modo operación cuando exista una configuración válida y SHALL arrancar en modo portal cuando no la haya o cuando el operador mantenga pulsado el botón de usuario (IO38) durante unos segundos.

#### Scenario: Primer arranque
- **WHEN** el equipo se enciende sin configuración válida
- **THEN** arranca en modo portal, no abre sesión APRS-IS, no transmite por la malla y el estado indica que falta una configuración válida

#### Scenario: Entrada manual al portal
- **WHEN** el operador mantiene pulsado el botón de usuario (IO38) durante tres segundos en modo operación
- **THEN** el equipo reinicia en modo portal aunque tenga configuración válida

### Requirement: Interruptor del gate en caliente

El portal SHALL permitir abrir y cerrar el gate sin reiniciar el equipo, y el cambio SHALL aplicarse de inmediato a la siguiente trama.

#### Scenario: Cierre inmediato
- **WHEN** el operador cierra el gate
- **THEN** la siguiente trama con prefijo no se inyecta y el evento queda registrado

#### Scenario: Reapertura
- **WHEN** el operador vuelve a abrir el gate con la sesión activa
- **THEN** las tramas siguientes se inyectan sin reiniciar

### Requirement: Estado y diagnóstico con contadores separados

El portal SHALL mostrar el estado de la sesión APRS-IS (deshabilitada, desconectada, esperando login, verificada o no verificada) con el último error visible, los flujos abiertos con su antigüedad, el último paquete procesado y contadores separados para paquetes recibidos de APRS-IS, admitidos por el pinhole, rechazados por admisión, inyectados, descartados por tasa o tamaño, y retenidos en el buffer. SHALL mostrar además la huella de módem efectiva —frecuencia, ancho de banda, factor de dispersión, tasa de codificación y saltos— para compararla con la del tracker.

#### Scenario: Filtro demasiado estrecho
- **WHEN** el filtro no trae paquetes dirigidos a las estaciones activas
- **THEN** el portal muestra recepciones en cero y el operador puede distinguirlo de un rechazo por admisión

#### Scenario: Admisión rechazando
- **WHEN** llegan paquetes de APRS-IS pero ninguno coincide con un flujo vigente
- **THEN** el contador de admitidos queda en cero y el de rechazados sube, con el último motivo visible

#### Scenario: Sesión no verificada
- **WHEN** APRS-IS rechaza el passcode
- **THEN** el portal muestra la sesión como no verificada y el contador de inyectados no avanza

#### Scenario: Comparación con el tracker
- **WHEN** el operador compara la huella de módem del gateway con la del tracker
- **THEN** ambos muestran frecuencia, ancho de banda, factor de dispersión, tasa de codificación y saltos, y puede detectar un desajuste antes de buscar silencio
