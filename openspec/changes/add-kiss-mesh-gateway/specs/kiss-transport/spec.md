## Purpose

Expone un TNC KISS estándar, por Bluetooth Classic y por TCP, que convierte tramas APRS hacia y desde el enlace de malla, de modo que clientes APRS existentes lo usen sin modificaciones ni software intermediario.

## ADDED Requirements

### Requirement: Interfaz KISS por Bluetooth Classic SPP

El equipo SHALL exponer un servicio de puerto serie Bluetooth Classic (SPP) identificable por su nombre de dispositivo, aceptar conexiones de clientes APRS y mantener el servicio disponible tras desconexiones del cliente.

#### Scenario: Cliente se conecta y envía una trama
- **WHEN** un cliente APRS se empareja y abre el puerto SPP
- **THEN** el equipo acepta la conexión y procesa las tramas KISS que recibe por ella

#### Scenario: Cliente se reconecta
- **WHEN** un cliente se desconecta y vuelve a conectarse minutos después
- **THEN** el equipo acepta la nueva conexión sin reiniciar y sigue procesando tramas

### Requirement: Interfaz KISS por TCP

El equipo SHALL escuchar en el puerto TCP 8001 y aceptar varios clientes KISS simultáneos, serializando la transmisión de modo que dos tramas nunca se entrelacen en el medio del mismo flujo.

#### Scenario: Dos clientes simultáneos
- **WHEN** un cliente SPP y un cliente TCP están conectados al mismo tiempo
- **THEN** ambos pueden enviar tramas y ambos reciben las tramas de bajada

#### Scenario: Transmisiones concurrentes
- **WHEN** dos clientes envían una trama en el mismo instante
- **THEN** cada trama se entrega completa y en orden dentro de su propia conexión

### Requirement: Framing KISS

El equipo SHALL delimitar y desescapar las tramas según KISS: `0xC0` como delimitador, `0xDB 0xDC` como `0xC0` escapado y `0xDB 0xDD` como `0xDB` escapado. Las tramas vacías SHALL descartarse sin efecto, y una secuencia inválida SHALL provocar resincronización sin cortar la conexión.

#### Scenario: Payload con bytes escapados
- **WHEN** una trama contiene `0xC0` o `0xDB` dentro de su payload, escapados por el cliente
- **THEN** el equipo reconstruye el payload original byte por byte

#### Scenario: Basura en el flujo
- **WHEN** el cliente envía bytes que no forman una trama válida
- **THEN** el equipo descarta esa porción y sigue procesando la próxima trama correcta

#### Scenario: Trama mayor al máximo admitido
- **WHEN** llega una trama que supera el tamaño máximo soportado
- **THEN** el equipo la descarta, registra el evento y no transmite nada por la malla ni por KISS

### Requirement: Separación entre tramas de datos y comandos KISS

El equipo SHALL reenviar únicamente las tramas KISS de tipo dato (puerto 0). Los comandos de configuración de módem (TXDELAY, persistencia, slot time, TXtail, full duplex, control de hardware) SHALL consumirse en silencio y nunca publicarse en la malla.

#### Scenario: Cliente configura parámetros de módem
- **WHEN** el cliente envía comandos de TXDELAY o persistencia al conectarse
- **THEN** el equipo no publica nada en la malla y la siguiente trama de datos se procesa normalmente

#### Scenario: Tipo de trama desconocido
- **WHEN** el cliente envía una trama con un tipo no soportado
- **THEN** el equipo la ignora sin cerrar la conexión ni alterar su estado

### Requirement: Conversión de AX.25 a texto TNC2

El equipo SHALL extraer origen, destino, path e información de cada trama AX.25 de datos y construir la representación textual `ORIGEN>DESTINO,PATH:información`. Una trama AX.25 malformada, truncada o con campos inválidos SHALL descartarse y registrarse, sin publicarse en la malla.

#### Scenario: Trama de posición válida
- **WHEN** llega una trama AX.25 de `EA1ABC-9` hacia `APRS` con path `WIDE1-1,WIDE2-1` e información de posición
- **THEN** el equipo produce `EA1ABC-9>APRS,WIDE1-1,WIDE2-1:!4332.10N/00752.30W-Test`

#### Scenario: Trama truncada
- **WHEN** llega una trama AX.25 con menos bytes que los que declaran sus direcciones
- **THEN** el equipo la descarta, registra el motivo y no transmite por la malla

### Requirement: Conversión de texto TNC2 a AX.25

El equipo SHALL construir una trama AX.25 de tipo UI (control `0x03`, PID `0xF0`) con direcciones correctamente desplazadas a partir de un texto TNC2 recibido de la malla, y entregarla como trama de datos KISS en el puerto 0.

#### Scenario: Mensaje APRS desde la malla
- **WHEN** el equipo admite el texto `A#EA2XYZ>APRS::EA1ABC-9 :hola{13`
- **THEN** entrega a los clientes KISS una trama AX.25 cuyo campo de información es exactamente `:EA1ABC-9 :hola{13`

#### Scenario: Destino que no es una dirección válida
- **WHEN** el texto tiene forma de trama pero el destino no es un callsign válido
- **THEN** el equipo lo descarta y registra el motivo

### Requirement: Preservación del path antes de publicar

El equipo SHALL conservar el path de digipeaters en el texto que publica en la malla, porque el gateway APRS-IS lo necesita para reconstruir la trama original. El recorte SHALL ser una opción de configuración desactivada por defecto; cuando esté activa, el equipo elimina el path y registra que lo hizo.

#### Scenario: Path conservado por defecto
- **WHEN** se publica en la malla una trama con path `WIDE1-1,WIDE2-1`
- **THEN** el texto publicado contiene ese path

#### Scenario: Recorte activado por el operador
- **WHEN** el operador activa el recorte de path en el portal
- **THEN** el texto publicado no contiene digipeaters y el equipo registra el recorte

#### Scenario: Trama solo con origen y destino
- **WHEN** la trama no trae digipeaters
- **THEN** el texto publicado conserva exactamente origen, destino e información

### Requirement: Supresión del eco

El equipo SHALL no entregar a los clientes KISS una trama que él mismo haya publicado, aunque la reciba de vuelta a través de relays de la malla.

#### Scenario: Trama propia retransmitida por un relay
- **WHEN** el equipo recibe por LoRa una copia del texto que él mismo inyectó minutos antes
- **THEN** no entrega ninguna trama a los clientes KISS por ese paquete
