## Purpose

Permite configurar el equipo desde un navegador, sin recompilar el firmware y sin depender de la aplicación oficial de Meshtastic ni de ninguna conexión a internet, guardando los parámetros de forma persistente y segura.

## ADDED Requirements

### Requirement: Portal web en modo punto de acceso

El equipo SHALL exponer un portal de configuración accesible en `192.168.4.1` mediante un punto de acceso WiFi protegido con clave, sin asociarse como cliente a ninguna red externa, y SHALL permitir acceder a él desde cualquier navegador sin instalar software.

#### Scenario: Acceso al portal
- **WHEN** el operador se conecta al punto de acceso del equipo y abre `192.168.4.1`
- **THEN** el equipo muestra el formulario de configuración con los valores actuales

#### Scenario: Acceso sin credenciales de red
- **WHEN** un dispositivo intenta asociarse al punto de acceso sin la clave correcta
- **THEN** no obtiene acceso al portal

#### Scenario: Equipo en operación
- **WHEN** el equipo está en modo operación y no hay nadie conectado a su punto de acceso
- **THEN** no intenta ninguna conexión de red saliente

### Requirement: Parámetros configurables

El portal SHALL permitir configurar al menos: callsign del operador, nombre Bluetooth del equipo, nombre de canal, clave precompartida del canal, región, preset de módem, recorte de path, límite de tasa de transmisión e intervalo mínimo entre transmisiones. El preset SHALL incluir una opción `CUSTOM` que habilite la configuración de frecuencia, ancho de banda, factor de dispersión y tasa de codificación. El número de saltos SHALL configurarse de forma independiente del preset, en el rango de 0 a 7.

#### Scenario: Cambio de canal y clave
- **WHEN** el operador modifica el nombre de canal y la clave precompartida y guarda
- **THEN** el equipo persiste los valores y los aplica en el siguiente arranque del enlace

#### Scenario: Ajuste del límite de aire
- **WHEN** el operador cambia el límite de tasa de transmisión
- **THEN** el nuevo límite se aplica sin necesidad de recompilar el firmware

#### Scenario: Preset CUSTOM
- **WHEN** el operador elige `CUSTOM`, introduce una frecuencia, un ancho de banda, un factor de dispersión y una tasa de codificación válidos, y guarda
- **THEN** el equipo persiste esos valores y los aplica al configurar la radio, en lugar de los de un preset fijo

#### Scenario: Valores custom conservados
- **WHEN** el operador cambia de `CUSTOM` a un preset fijo y más tarde vuelve a `CUSTOM`
- **THEN** los últimos valores custom siguen disponibles y sólo se validan mientras `CUSTOM` está activo

#### Scenario: Valores iniciales de CUSTOM
- **WHEN** el operador elige `CUSTOM` sin haber usado ese modo antes
- **THEN** el portal precarga 62.5 kHz, SF7, CR 4/5 y la frecuencia efectiva actual, que el operador puede modificar

#### Scenario: Saltos independientes
- **WHEN** el operador cambia el número de saltos sin cambiar el preset
- **THEN** el equipo aplica el nuevo límite a todos sus paquetes propios, incluidos los textos APRS y el NodeInfo, sin alterar el modulador

#### Scenario: Estimación de aire
- **WHEN** el operador edita el ancho de banda o el factor de dispersión
- **THEN** el portal muestra en vivo el aire estimado por trama, sugiere un límite de tasa sostenible y ofrece aplicarlo, sin bloquear el guardado

### Requirement: Validación de la configuración

El equipo SHALL validar los valores antes de aplicarlos y SHALL rechazar la configuración indicando el campo inválido, conservando los valores anteriores. Un callsign, una clave, una región, un preset o un juego de parámetros de módem inválidos SHALL impedir que el equipo transmita.

En el modo `CUSTOM`, la frecuencia SHALL validarse de forma que todo el ancho de banda ocupado quede dentro de la banda de la región, y el ancho de banda, el factor de dispersión y la tasa de codificación SHALL pertenecer al subconjunto admitido por el equipo. El número de saltos SHALL estar entre 0 y 7.

#### Scenario: Callsign con formato inválido
- **WHEN** el operador guarda un callsign que no cumple el formato de radioaficionado
- **THEN** el portal muestra el error, no guarda el valor y la configuración previa sigue activa

#### Scenario: Clave de canal con longitud no soportada
- **WHEN** la clave precompartida tiene una longitud que no corresponde a un canal válido
- **THEN** el portal rechaza el cambio e indica el motivo

#### Scenario: Frecuencia fuera de la banda
- **WHEN** el operador introduce en `CUSTOM` una frecuencia cuya ocupación se sale de la banda de la región
- **THEN** el portal rechaza el valor, indica el campo y conserva la configuración anterior

#### Scenario: Ancho de banda incompatible con la región
- **WHEN** el operador elige en `CUSTOM` un ancho de banda mayor que la banda disponible de la región
- **THEN** el portal rechaza el valor con un mensaje explícito, sin aplicar un preset de reemplazo silencioso

#### Scenario: Saltos fuera de rango
- **WHEN** el operador introduce un número de saltos mayor que 7
- **THEN** el portal rechaza el valor y conserva el límite anterior

### Requirement: Persistencia atómica en NVS

El equipo SHALL almacenar la configuración en memoria no volátil de forma atómica, de modo que un corte de energía durante el guardado no deje la configuración corrupta ni pierda por completo la anterior.

#### Scenario: Corte durante el guardado
- **WHEN** se interrumpe la alimentación mientras el equipo escribe la configuración
- **THEN** al arrancar utiliza la configuración anterior completa y sigue operativo

#### Scenario: Reinicio normal
- **WHEN** el equipo se reinicia después de guardar una configuración válida
- **THEN** todos los parámetros guardados están activos

### Requirement: Modos de arranque

El equipo SHALL arrancar en modo operación cuando exista una configuración válida y SHALL arrancar en modo portal cuando no la haya o cuando el operador mantenga pulsado el botón de usuario (IO38) durante unos segundos. Al salir del modo portal, el equipo SHALL reiniciar el enlace con la configuración vigente.

#### Scenario: Primer arranque
- **WHEN** el equipo se enciende sin configuración válida
- **THEN** arranca en modo portal y no transmite por la malla

#### Scenario: Entrada manual al portal
- **WHEN** el operador mantiene pulsado el botón de usuario (IO38) durante tres segundos en modo operación
- **THEN** el equipo reinicia en modo portal aunque tenga configuración válida

#### Scenario: Salida del portal
- **WHEN** el operador guarda una configuración válida y solicita reiniciar
- **THEN** el equipo vuelve al modo operación y aplica el canal y la clave nuevos

### Requirement: Estado y diagnóstico visible

El portal SHALL mostrar el estado actual del equipo: modo, configuración activa, versión de firmware, clientes KISS conectados, contadores de tramas subidas, bajadas y descartadas, memoria libre y último error registrado. La configuración activa SHALL incluir la huella de módem efectiva: frecuencia, ancho de banda, factor de dispersión, tasa de codificación y número de saltos.

#### Scenario: Consulta de estado
- **WHEN** el operador abre la página de estado
- **THEN** ve la configuración activa, los contadores y la memoria libre disponible

#### Scenario: Huella de módem efectiva
- **WHEN** el operador consulta el estado
- **THEN** ve la frecuencia, el ancho de banda, el factor de dispersión, la tasa de codificación y los saltos que el equipo está usando realmente, tanto con un preset fijo como con `CUSTOM`

#### Scenario: Trama descartada
- **WHEN** el equipo descarta una trama por exceder el límite de aire o por tamaño inválido
- **THEN** el contador de descartes y el último motivo quedan visibles en el portal
