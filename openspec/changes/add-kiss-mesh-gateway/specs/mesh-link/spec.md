## Purpose

Publica y recibe las tramas APRS como texto plano dentro de un canal Meshtastic compatible —público o privado— respetando la configuración de radio del canal y el aire compartido con el resto de la malla.

## ADDED Requirements

### Requirement: Configuración de enlace compatible con Meshtastic

El equipo SHALL transmitir y recibir utilizando exactamente la región, el preset de módem, el nombre de canal y la clave precompartida configurados, de forma que un nodo Meshtastic estándar con la misma configuración pueda leer y recibir sus mensajes.

#### Scenario: Nodo estándar con la misma configuración
- **WHEN** el equipo publica un texto en el canal configurado y existe un nodo Meshtastic estándar con región, preset, canal y clave idénticos
- **THEN** ese nodo recibe el texto y lo muestra como mensaje de canal

#### Scenario: Configuración de canal incorrecta
- **WHEN** la región, el preset o la clave configurados no coinciden con los de la malla
- **THEN** el equipo no recibe tráfico y sus transmisiones no son legibles por los demás nodos, sin generar errores hacia los clientes KISS

### Requirement: Publicación de una trama como un único texto

El equipo SHALL publicar cada trama de datos admitida como exactamente un mensaje de texto en el canal configurado, compuesto por el prefijo configurado seguido de la representación TNC2 de la trama.

#### Scenario: Publicación de una baliza
- **WHEN** un cliente KISS entrega una trama de posición válida
- **THEN** el equipo publica un único mensaje de texto con prefijo y TNC2 en el canal configurado

#### Scenario: Reintento del cliente
- **WHEN** el cliente entrega la misma trama dos veces dentro de la ventana de deduplicación
- **THEN** el equipo publica un solo mensaje en la malla

### Requirement: Límite de tamaño del mensaje

El equipo SHALL rechazar las tramas cuyo texto resultante supere el tamaño máximo de mensaje admitido por el canal, registrando el evento, en lugar de truncarlas o fragmentarlas.

#### Scenario: Trama demasiado larga
- **WHEN** la trama produce un texto que excede el máximo admitido
- **THEN** el equipo no publica nada, registra el descarte y no entrega nada a los clientes KISS

### Requirement: Prefijo de identificación APRS

El equipo SHALL reconocer como candidatos a trama APRS únicamente los textos del canal que comienzan con el prefijo configurado (por defecto `A#`), y SHALL ignorar cualquier otro texto del canal.

#### Scenario: Texto sin prefijo
- **WHEN** un usuario de la malla escribe un mensaje normal en el canal
- **THEN** el equipo lo ignora por completo y no entrega nada a los clientes KISS

#### Scenario: Texto con prefijo pero sin forma de trama
- **WHEN** un texto comienza con el prefijo pero su contenido no es un TNC2 válido
- **THEN** el equipo lo descarta y registra el motivo

### Requirement: Identidad del nodo en la malla

El equipo SHALL presentarse en la malla como un nodo de rol `CLIENT_MUTE` cuyo nombre largo es el callsign configurado, y SHALL anunciar su información de nodo de forma periódica para que las aplicaciones Meshtastic lo muestren con nombre en lugar de identificador desconocido.

#### Scenario: Apariencia en la aplicación
- **WHEN** el equipo anuncia su información de nodo en el canal
- **THEN** la aplicación Meshtastic muestra el nodo con el callsign configurado como nombre

#### Scenario: Sin callsign configurado
- **WHEN** el equipo arranca sin callsign válido
- **THEN** no se presenta en la malla ni transmite hasta que la configuración sea válida

### Requirement: No reemisión de tráfico ajeno

El equipo SHALL no retransmitir paquetes originados por otros nodos, y SHALL fijar un límite de saltos en sus propias transmisiones para que los relays de la malla las reenvíen.

#### Scenario: Paquete de otro nodo
- **WHEN** el equipo recibe un paquete originado por un tercero que no necesita repetir
- **THEN** no lo retransmite y no altera el tráfico de la malla

#### Scenario: Alcance multihop de las propias transmisiones
- **WHEN** el equipo publica un texto con límite de saltos mayor que uno
- **THEN** los nodos intermedios pueden reenviarlo y estaciones fuera de alcance directo lo reciben

### Requirement: Deduplicación de recepción

El equipo SHALL entregar como máximo una trama KISS por paquete de malla recibido, aun cuando el mismo paquete llegue repetido por varios relays dentro de la ventana de deduplicación.

#### Scenario: Mismo paquete por tres relays
- **WHEN** el mismo paquete de malla con prefijo se recibe tres veces por caminos distintos
- **THEN** los clientes KISS reciben una sola trama

### Requirement: Disciplina de airtime

El equipo SHALL limitar su propia tasa de transmisión a un máximo configurable y SHALL descartar las tramas que excedan ese límite en lugar de acumularlas indefinidamente, registrando cada descarte.

#### Scenario: Ráfaga de tramas
- **WHEN** un cliente entrega más tramas de las permitidas por el límite configurado
- **THEN** el equipo transmite hasta el límite, descarta el excedente y lo registra

#### Scenario: Canal congestionado
- **WHEN** la cola de transmisión está llena por tráfico previo
- **THEN** el equipo descarta la trama entrante sin bloquear la conexión KISS

### Requirement: Robustez ante tráfico no interpretable

El equipo SHALL descartar en silencio los paquetes que no pueda descifrar, que pertenezcan a otros canales o que usen tipos de aplicación distintos del texto, sin reiniciarse ni interrumpir la recepción.

#### Scenario: Paquete de otro canal
- **WHEN** el equipo recibe un paquete cifrado con una clave distinta de la configurada
- **THEN** lo descarta y continúa operando normalmente

#### Scenario: Paquete de posición nativo
- **WHEN** un nodo de la malla emite una posición nativa de Meshtastic
- **THEN** el equipo no la convierte en trama APRS ni la entrega a los clientes KISS
