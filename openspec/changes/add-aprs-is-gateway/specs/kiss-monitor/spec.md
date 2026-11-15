## Purpose

Expone un puerto KISS de solo lectura con todo el tráfico que cruza el gateway, para depurar el enlace y conectar clientes APRS de escritorio sin habilitar un segundo igate ni permitir transmisiones.

## ADDED Requirements

### Requirement: Puerto KISS de solo lectura

El equipo SHALL escuchar en un puerto TCP dedicado y aceptar varios clientes KISS simultáneos, entregándoles las mismas tramas a todos. Cualquier byte que un cliente envíe SHALL descartarse y contabilizarse, sin publicarse en la malla ni inyectarse en APRS-IS.

#### Scenario: Cliente de escritorio se conecta
- **WHEN** Direwolf o Xastir se conecta al puerto del monitor
- **THEN** el equipo lo acepta y comienza a entregarle las tramas que crucen el gateway

#### Scenario: Cliente intenta transmitir
- **WHEN** un cliente envía una trama KISS por el monitor
- **THEN** el equipo la descarta, la cuenta y no la publica en ningún lado

#### Scenario: Varios monitores
- **WHEN** dos clientes están conectados al mismo tiempo
- **THEN** ambos reciben exactamente las mismas tramas

### Requirement: Reflejo de las subidas

El equipo SHALL entregar al monitor cada trama que inyecta en APRS-IS, convertida a una trama AX.25 de tipo UI lista para un cliente KISS, después de que la inyección haya sido aceptada localmente.

#### Scenario: Trama subida
- **WHEN** el gateway inyecta una trama en APRS-IS
- **THEN** el monitor entrega a sus clientes la misma información como trama AX.25

### Requirement: Reflejo de las bajadas

El equipo SHALL entregar al monitor cada trama que publica en el canal de malla, convertida a una trama AX.25 de tipo UI, con el path reescrito que efectivamente viaja al canal.

#### Scenario: Respuesta bajada
- **WHEN** el gateway publica una respuesta admitida en el canal
- **THEN** el monitor entrega la trama con el path `TCPIP*` que se publicó

### Requirement: Robustez del monitor

El equipo SHALL descartar sin cortar el servicio cualquier texto que no pueda convertirse en una trama AX.25 válida, y SHALL expulsar a los clientes cuyas escrituras fallen, sin afectar la sesión APRS-IS ni la recepción de la malla.

#### Scenario: Texto inválido
- **WHEN** llega al monitor un texto con prefijo que no es un TNC2 válido
- **THEN** no se entrega ninguna trama y el evento queda registrado

#### Scenario: Cliente caído
- **WHEN** un cliente del monitor deja de aceptar bytes
- **THEN** el equipo lo expulsa y sigue sirviendo a los demás
