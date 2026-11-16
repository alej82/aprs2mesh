## Purpose

Publica y recibe las tramas APRS como texto plano dentro de un canal Meshtastic compatible —público o privado— respetando la configuración de radio del canal y el aire compartido con el resto de la malla.

## ADDED Requirements

### Requirement: Configuración de enlace compatible con Meshtastic

El equipo SHALL transmitir y recibir utilizando exactamente la región, el preset de módem, el nombre de canal y la clave precompartida configurados, de forma que un nodo Meshtastic estándar con la misma configuración pueda leer y recibir sus mensajes. Cuando el preset sea `CUSTOM`, el equipo SHALL usar la frecuencia, el ancho de banda, el factor de dispersión y la tasa de codificación configurados, equivalentes a `use_preset = false` y `override_frequency` en Meshtastic.

#### Scenario: Nodo estándar con la misma configuración
- **WHEN** el equipo publica un texto en el canal configurado y existe un nodo Meshtastic estándar con región, preset, canal y clave idénticos
- **THEN** ese nodo recibe el texto y lo muestra como mensaje de canal

#### Scenario: Nodo estándar con módem CUSTOM
- **WHEN** el equipo y un nodo Meshtastic estándar usan `CUSTOM` con la misma frecuencia, ancho de banda, factor de dispersión y tasa de codificación
- **THEN** ambos se reciben mutuamente igual que con un preset fijo

#### Scenario: Configuración de canal incorrecta
- **WHEN** la región, el preset, los parámetros custom o la clave configurados no coinciden con los de la malla
- **THEN** el equipo no recibe tráfico y sus transmisiones no son legibles por los demás nodos, sin generar errores hacia los clientes KISS

### Requirement: Resolución del módem efectivo

El equipo SHALL derivar la frecuencia de un preset fijo a partir de la banda de la región, el ancho de banda y el nombre de canal, con el mismo criterio de reparto de canales que Meshtastic, y SHALL usar los parámetros configurados cuando el preset sea `CUSTOM`. La frecuencia custom SHALL ocupar un ancho de banda completo dentro de la banda de la región.

#### Scenario: Preset fijo en EU868
- **WHEN** el equipo usa un preset fijo de 250 kHz en EU868
- **THEN** selecciona 869.525 MHz, único centro que deja el ancho de banda completo dentro de la banda

#### Scenario: Preset fijo en US915
- **WHEN** el equipo usa un preset fijo en US915
- **THEN** la frecuencia corresponde al slot que Meshtastic calcula para el nombre de canal y el ancho de banda

#### Scenario: Parámetros custom fuera de la banda
- **WHEN** el operador configura una frecuencia custom cuya ocupación se sale de la banda de la región
- **THEN** el equipo rechaza la configuración y no inicia el enlace de radio con esos valores

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

### Requirement: Prefijo fijo de identificación APRS

El equipo SHALL reconocer como candidatos a trama APRS únicamente los textos que comienzan con el prefijo compartido `A#`, definido como constante en el código del enlace, y SHALL ignorar cualquier otro texto del canal. El prefijo no SHALL ser configurable, para que tracker y gateway no puedan desincronizarse.

#### Scenario: Texto sin prefijo
- **WHEN** un usuario de la malla escribe un mensaje normal en el canal
- **THEN** el equipo lo ignora por completo y no entrega nada a los clientes KISS

#### Scenario: Texto con prefijo pero sin forma de trama
- **WHEN** un texto comienza con el prefijo pero su contenido no es un TNC2 válido
- **THEN** el equipo lo descarta y registra el motivo

#### Scenario: Marca distinta
- **WHEN** un texto comienza con otra marca parecida pero no con `A#`
- **THEN** el equipo lo trata como chat y no intenta interpretarlo

### Requirement: Identidad del nodo en la malla

El equipo SHALL presentarse en la malla como un nodo de rol `CLIENT_MUTE` cuyo nombre largo es el callsign configurado, y SHALL anunciar su información de nodo de forma periódica para que las aplicaciones Meshtastic lo muestren con nombre en lugar de identificador desconocido.

#### Scenario: Apariencia en la aplicación
- **WHEN** el equipo anuncia su información de nodo en el canal
- **THEN** la aplicación Meshtastic muestra el nodo con el callsign configurado como nombre

#### Scenario: Sin callsign configurado
- **WHEN** el equipo arranca sin callsign válido
- **THEN** no se presenta en la malla ni transmite hasta que la configuración sea válida

### Requirement: No reemisión de tráfico ajeno

El equipo SHALL no retransmitir paquetes originados por otros nodos, y SHALL fijar en sus propias transmisiones el límite de saltos configurado, entre 0 y 7, para que los relays de la malla las reenvíen. El mismo límite SHALL aplicarse a todos los paquetes propios, incluidos los textos APRS y el NodeInfo.

#### Scenario: Paquete de otro nodo
- **WHEN** el equipo recibe un paquete originado por un tercero que no necesita repetir
- **THEN** no lo retransmite y no altera el tráfico de la malla

#### Scenario: Alcance multihop de las propias transmisiones
- **WHEN** el equipo publica un texto o anuncia su NodeInfo con un límite de saltos mayor que uno
- **THEN** los nodos intermedios pueden reenviarlo y estaciones fuera de alcance directo lo reciben

#### Scenario: Límite de saltos cero
- **WHEN** el operador configura cero saltos
- **THEN** el equipo sólo alcanza a sus vecinos directos y no depende de relays

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
