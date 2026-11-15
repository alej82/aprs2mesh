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

El portal SHALL permitir configurar al menos: callsign del gateway, servidor y puerto de APRS-IS, passcode, filtro de recepción, habilitación del gate, TTL de flujos, cantidad máxima de flujos, límite de tasa de subida, límite de tasa de bajada, SSID y clave de la red WiFi, nombre de canal, clave precompartida del canal, región y preset de módem.

#### Scenario: Cambio de filtro
- **WHEN** el operador modifica el filtro de recepción y guarda
- **THEN** el equipo reabre la sesión APRS-IS con el filtro nuevo

#### Scenario: Cambio de canal
- **WHEN** el operador modifica el nombre de canal o la clave precompartida
- **THEN** el equipo persiste los valores y los aplica al reiniciar el enlace de radio

### Requirement: Validación de la configuración

El equipo SHALL validar los valores antes de aplicarlos y SHALL rechazar la configuración indicando el campo inválido, conservando los valores anteriores. El filtro SHALL rechazarse si contiene saltos de línea, caracteres de control o excede la longitud máxima admitida, porque se concatena en la línea de login.

#### Scenario: Filtro con salto de línea
- **WHEN** el operador pega un filtro que contiene un salto de línea
- **THEN** el portal rechaza el valor e indica el motivo, sin abrir una sesión modificada

#### Scenario: Passcode no numérico
- **WHEN** el operador guarda un passcode con caracteres que no son dígitos
- **THEN** el portal rechaza el cambio y conserva la configuración previa

#### Scenario: Callsign con formato inválido
- **WHEN** el operador guarda un callsign que no cumple el formato de radioaficionado
- **THEN** el portal muestra el error y no transmite con esa identidad

### Requirement: Persistencia atómica en NVS

El equipo SHALL almacenar la configuración en memoria no volátil de forma atómica, de modo que un corte de energía durante el guardado no deje la configuración corrupta ni pierda por completo la anterior.

#### Scenario: Corte durante el guardado
- **WHEN** se interrumpe la alimentación mientras el equipo escribe la configuración
- **THEN** al arrancar utiliza la configuración anterior completa y sigue operativo

### Requirement: Modos de arranque

El equipo SHALL arrancar en modo operación cuando exista una configuración válida y SHALL arrancar en modo portal cuando no la haya o cuando el operador mantenga pulsado el botón durante el arranque.

#### Scenario: Primer arranque
- **WHEN** el equipo se enciende sin configuración válida
- **THEN** arranca en modo portal, no abre sesión APRS-IS y no transmite por la malla

#### Scenario: Entrada manual al portal
- **WHEN** el operador mantiene pulsado el botón durante el arranque
- **THEN** el equipo arranca en modo portal aunque tenga configuración válida

### Requirement: Interruptor del gate en caliente

El portal SHALL permitir abrir y cerrar el gate sin reiniciar el equipo, y el cambio SHALL aplicarse de inmediato a la siguiente trama.

#### Scenario: Cierre inmediato
- **WHEN** el operador cierra el gate
- **THEN** la siguiente trama con prefijo no se inyecta y el evento queda registrado

#### Scenario: Reapertura
- **WHEN** el operador vuelve a abrir el gate con la sesión activa
- **THEN** las tramas siguientes se inyectan sin reiniciar

### Requirement: Estado y diagnóstico con contadores separados

El portal SHALL mostrar el estado de la sesión APRS-IS (conectada, no verificada, deshabilitada o en error), los flujos abiertos con su antigüedad, el último paquete procesado y contadores separados para paquetes recibidos de APRS-IS, admitidos por el pinhole, rechazados por admisión, inyectados, descartados por tasa o tamaño, y retenidos en el buffer.

#### Scenario: Filtro demasiado estrecho
- **WHEN** el filtro no trae paquetes dirigidos a las estaciones activas
- **THEN** el portal muestra recepciones en cero y el operador puede distinguirlo de un rechazo por admisión

#### Scenario: Admisión rechazando
- **WHEN** llegan paquetes de APRS-IS pero ninguno coincide con un flujo vigente
- **THEN** el contador de admitidos queda en cero y el de rechazados sube, con el último motivo visible

#### Scenario: Sesión no verificada
- **WHEN** APRS-IS rechaza el passcode
- **THEN** el portal muestra la sesión como no verificada y el contador de inyectados no avanza
