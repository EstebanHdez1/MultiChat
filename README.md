# Multistream Chat (C++20 / Qt 6)

Un agregador de chats multistream de alto rendimiento, modular y moderno desarrollado en **C++20** con el framework **Qt 6** (Widgets, Network, WebSockets, Svg).

Permite centralizar en tiempo real en una sola ventana los chats, recuentos de espectadores y eventos de monetización de transmisiones en vivo de las principales plataformas:
- 🟣 **Twitch**
- 🔴 **YouTube**
- 🟢 **Kick**
- 🎵 **TikTok**

---

## 📋 Tabla de Contenidos
1. [Objetivo y Filosofía](#-objetivo-y-filosofía)
2. [Características Principales](#-características-principales)
3. [Arquitectura del Sistema](#-arquitectura-del-sistema)
4. [Mecanismos Técnicos por Plataforma](#-mecanismos-técnicos-por-plataforma)
5. [Estructura del Proyecto](#-estructura-del-proyecto)
6. [Requisitos y Compilación](#-requisitos-y-compilación)
7. [Guía de Uso y Configuración](#-guía-de-uso-y-configuración)
8. [Uso como Overlay en OBS Studio](#-uso-como-overlay-en-obs-studio)
9. [Guía de Extensión para Desarrolladores e IA](#-guía-de-extensión-para-desarrolladores-e-ia)

---

## 🎯 Objetivo y Filosofía

El software actúa como una estación de control central para streamers, creadores de contenido y moderadores que transmiten en simultáneo (*multistreaming*). 

### Principios de Diseño:
- **Sin inicio de sesión obligatorio**: El usuario no necesita ingresar contraseñas ni realizar flujos OAuth complejos para visualizar el chat y los eventos públicos de un canal.
- **Independencia total entre plataformas**: Cada plataforma corre de manera asíncrona y aislada. Si una plataforma se desconecta, entra en rate-limit o está fuera de línea, las demás continúan operando normalmente.
- **Eficiencia y Bajo Consumo**: Desarrollado en C++ nativo con buffers circulares y gestión optimizada de memoria, asegurando consumo mínimo de CPU y RAM mientras se juega o transmite.
- **Legibilidad y Modularidad**: Código desacoplado con arquitectura orientada a eventos mediante señales y ranuras (Signals/Slots de Qt), facilitando su mantenimiento y ampliación.

---

## 🚀 Características Principales

### 1. Chat Unificado en Tiempo Real
- **Insignias y Estilos Visuales**: Identificación inmediata de la plataforma (iconos vectoriales SVG) y colores personalizados por usuario.
- **Soporte de Emotes Dinámicos**: Carga asíncrona de emoticonos desde sus respectivas CDN (Twitch v2, Kick CDN, YouTube Emojis).
- **Filtros Avanzados**: Filtrado instantáneo por plataforma (Twitch, YouTube, Kick, TikTok) y por tipo de mensaje (Normal, Donaciones, Suscripciones, Gifts, Eventos Live).
- **Auto-scroll Inteligente**: Desplazamiento automático desactivable para permitir la lectura pausada sin interrupciones por nuevos mensajes.
- **Simulador Integrado**: Botones para generar mensajes normales, donaciones y eventos de prueba sin requerir una transmisión real activa.

### 2. Notificaciones Flotantes y Anti-Spam (Live Toasts)
- Los eventos transitorios de TikTok (usuarios que se unen, comparten la transmisión, siguen o dan likes) se muestran como tarjetas flotantes animadas con desvanecimiento temporal configurable.
- **Anti-Spam de Likes**: Cooldown personalizable por usuario para evitar que el bombardeo de toques en pantalla sature la interfaz.

### 3. Top Donaciones & Monetización Consolidada
- Registro centralizado de:
  - Super Chats y Super Stickers (YouTube).
  - Bits y Cheers (Twitch).
  - Suscripciones y Gift Subs (Kick y Twitch).
  - Regalos de TikTok (Coins, Rosas, León, etc.).
- Tabla interactiva con ordenación por importe, usuario, plataforma y fecha.

### 4. Audiencia y Estadísticas Globales
- Consulta periódica de espectadores concurrentes en cada plataforma conectada.
- Cálculo en tiempo real del total acumulado de viewers consolidados.

### 5. Personalización Visual y Temas
- **5 Presets Listos para Usar**:
  - `Dark`: Estilo moderno oscuro de alto contraste.
  - `Light`: Tema claro y limpio.
  - `Streamer`: Fondo ultra-oscuro con acentos dorados y violetas.
  - `Minimal`: Vista compacta monocromática.
  - `Transparent`: Fondo transparente para capturar directamente como ventana Overlay en OBS Studio.
- Ajustes de tipografía, tamaño de fuente, redondeo de bordes y visibilidad de marcas de tiempo o avatares.

### 6. Consola de Logs en Vivo
- Registro en tiempo real de operaciones de red, peticiones HTTP, handshakes WebSocket y errores.
- Filtro por niveles (`INFO`, `WARN`, `ERROR`, `DEBUG`) y motor de búsqueda de texto integrado.

---

## 🏗️ Arquitectura del Sistema

El proyecto sigue una arquitectura desacoplada en capas:

```
┌──────────────────────────────────────────────────────────┐
│                     CAPA DE INTERFAZ (UI)                │
│  MainWindow ── Sidebar ── ChatWidget ── ChannelManager   │
│  DonationsWidget ── StatisticsWidget ── StyleSettings    │
└────────────────────────────┬─────────────────────────────┘
                             │ Signals / Slots
┌────────────────────────────▼─────────────────────────────┐
│                    CAPA CORE & GESTORES                  │
│  EventManager (Bus Central)       ChatManager (Buffer)   │
│  ConnectionManager (Ciclo Vida)   DonationManager (Rank) │
│  ConfigManager (JSON Settings)    Logger (Thread-safe)   │
└────────────────────────────┬─────────────────────────────┘
                             │ Modelos y Eventos
┌────────────────────────────▼─────────────────────────────┐
│                 CAPA DE PLATAFORMAS STREAMING            │
│  IStreamingPlatform ◄── BasePlatform                     │
│    ├── TwitchPlatform  (IRC over WebSocket + GQL)        │
│    ├── YouTubePlatform (InnerTube LiveChat Parser)       │
│    ├── KickPlatform    (API v2 + Pusher WebSocket)       │
│    └── TikTokPlatform  (Webcast Room + Protobuf GZIP)    │
└────────────────────────────┬─────────────────────────────┘
                             │ Peticiones de Red
┌────────────────────────────▼─────────────────────────────┐
│                   CAPA DE RED (NETWORK)                  │
│     HttpHelper (Asíncrono)    WebSocketWrapper (Auto-Rec)│
└──────────────────────────────────────────────────────────┘
```

---

## 🔌 Mecanismos Técnicos por Plataforma

### 🟣 Twitch (`TwitchPlatform`)
- **Conexión de Chat**: Se conecta mediante un WebSocket seguro al servidor IRC de Twitch (`wss://irc-ws.chat.twitch.tv:443`).
- **Autenticación Anónima**: Utiliza una cuenta anónima de solo lectura (`NICK justinfanXXXXX`) enviando el comando `PASS SCHMOOPIE`.
- **Decodificación**: Solicita capacidades avanzadas `twitch.tv/tags twitch.tv/commands` para recibir metadatos de usuario (colores, badges, id de mensaje, emotes e importe de bits).
- **Espectadores**: Sondeo periódico asíncrono para verificar el estado online y audiencia del canal.

### 🔴 YouTube (`YouTubePlatform`)
- **Resolución**: Acepta `@nombre_canal`, ID de canal (`UC...`), ID de video (11 caracteres) o la URL completa de la transmisión.
- **Handshake InnerTube**: Realiza una petición inicial a la página `/live` para extraer dinámicamente la `INNERTUBE_API_KEY` y el `continuationToken` de `ytInitialData`.
- **Polling de Chat**: Consulta asíncrona hacia `https://www.youtube.com/youtubei/v1/live_chat/get_live_chat` mediante peticiones JSON POST con cabeceras de cliente web.
- **Eventos**: Procesa `liveChatTextMessageRenderer`, `liveChatPaidMessageRenderer` (Super Chat), `liveChatPaidStickerRenderer` y membresías.

### 🟢 Kick (`KickPlatform`)
- **Resolución de Canal**: Consulta la API pública v2 (`https://kick.com/api/v2/channels/<slug>`) con fallback a extracción de HTML para obtener el `chatroomId` y el estado del stream.
- **Streaming Pusher**: Establece conexión WebSocket con el cluster Pusher de Kick (`wss://ws-us2.pusher.com/app/32cbd69e4b950bf97679`).
- **Eventos**: Se suscribe a `chatrooms.<chatroomId>.v2` y `channel.<channelId>`, procesando `ChatMessageEvent`, `SubscriptionEvent` y `GiftedSubscriptionsEvent`, decodificando emoticonos con etiquetas `[emote:id:name]`.

### 🎵 TikTok (`TikTokPlatform`)
- **Resolución de Sala**: Convierte el usuario `@streamer` en su `roomId` público mediante la API de directo o el extractor web móvil.
- **Recepción de Mensajes**: Realiza peticiones IM Webcast (`https://webcast.tiktok.com/webcast/im/fetch/`) y apertura de WebSocket firmado (`/webcast/im/push/v2/`).
- **Decodificación Protobuf & GZIP**: Decodifica el empaquetado binario Protobuf con descompresión GZIP para extraer `ChatMessage`, `GiftMessage`, `MemberMessage`, `SocialMessage` (follows/shares) y `LikeMessage`.
- **Anti-Spam**: Incorpora control de intervalo mínimo entre likes del mismo usuario para proteger la visualización.

---

## 📁 Estructura del Proyecto

```text
MultistreamChat/
├── CMakeLists.txt                # Configuración de compilación con CMake
├── README.md                     # Documentación completa del proyecto
├── .gitignore                    # Reglas de exclusión para Git y compilación
├── build.bat                     # Script interactivo de configuración y compilación
├── compilar.bat                  # Acceso directo para compilar desde terminal
├── resources/
│   ├── icons/                    # Iconos vectoriales SVG (plataformas, controles, etc.)
│   └── resources.qrc             # Catálogo de recursos embebidos de Qt
└── src/
    ├── main.cpp                  # Punto de entrada de la aplicación
    ├── models/                   # Estructuras de datos puras
    │   ├── AppStyleConfig.h      # Modelo de temas y estilos visuales
    │   ├── ChannelConfig.h       # Configuración por plataforma/canal
    │   ├── ChatMessage.h         # Modelo normalizado de mensaje y emotes
    │   ├── Donation.h            # Modelo para eventos de monetización
    │   └── PlatformStatus.h      # Enumeraciones de estado (ONLINE, OFFLINE, etc.)
    ├── core/                     # Lógica de negocio y gestores centrales
    │   ├── ChatManager.h/.cpp    # Buffer circular de mensajes y filtros
    │   ├── ConfigManager.h/.cpp  # Carga y persistencia en JSON
    │   ├── ConnectionManager.h/.cpp # Coordinador del ciclo de vida de plataformas
    │   ├── DonationManager.h/.cpp# Ranking y clasificación de donaciones
    │   ├── EventManager.h/.cpp   # Bus central de señales de la aplicación
    │   └── Logger.h/.cpp         # Sistema de registros thread-safe
    ├── network/                  # Capa de transporte y comunicaciones
    │   ├── HttpHelper.h/.cpp     # Cliente HTTP asíncrono con timeouts
    │   └── WebSocketWrapper.h/.cpp # Wrapper WebSocket con reconexión automática
    ├── platforms/                # Adaptadores de conexión por plataforma
    │   ├── IStreamingPlatform.h  # Interfaz base obligatoria
    │   ├── BasePlatform.h/.cpp   # Implementación común y timers de sondeo
    │   ├── TwitchPlatform.h/.cpp # Cliente IRC WebSocket de Twitch
    │   ├── YouTubePlatform.h/.cpp# Extractor y cliente InnerTube de YouTube
    │   ├── KickPlatform.h/.cpp   # Conector Pusher y API de Kick
    │   └── TikTokPlatform.h/.cpp # Motor Webcast Protobuf de TikTok
    └── ui/                       # Componentes gráficos de la interfaz (Qt Widgets)
        ├── MainWindow.h/.cpp     # Ventana principal y contenedor de páginas
        ├── Sidebar.h/.cpp        # Menú lateral colapsable y viewers globales
        ├── ChatWidget.h/.cpp     # Vista multichat, filtros y toasts flotantes
        ├── ChannelManagerWidget.h/.cpp # Tarjetas de configuración de canales
        ├── DonationsWidget.h/.cpp# Tabla interactiva de donaciones y monetización
        ├── StatisticsWidget.h/.cpp # Métricas de audiencia por plataforma
        ├── StyleSettingsWidget.h/.cpp # Selector de temas y estilos visuales
        ├── GeneralSettingsWidget.h/.cpp# Opciones generales y anti-spam
        └── LogViewerWidget.h/.cpp# Consola en vivo de registros y depuración
```

---

## 🛠️ Requisitos y Compilación

### Requisitos del Sistema
- **Sistema Operativo**: Windows 10/11 (64 bits), Linux o macOS.
- **Compilador C++**: Compatible con C++20 (GCC 12+, Clang 15+, MSVC 2022+).
- **Qt 6**: Módulos `Core`, `Gui`, `Widgets`, `Network`, `WebSockets`, `Svg`.
- **Dependencias**: `ZLIB` y `nlohmann_json` (disponibles en el entorno MinGW o vcpkg).
- **CMake**: Versión 3.20 o superior.
- **Generador**: Ninja (recomendado) o Make.

### Compilación Rápida en Windows

El proyecto incluye dos scripts automáticos en la raíz:

1. **Compilación y ejecución directa**:
   ```cmd
   compilar.bat
   ```
   o bien:
   ```cmd
   build.bat
   ```

2. **Compilación manual con CMake**:
   ```bash
   # 1. Configurar la carpeta de construcción con Ninja
   cmake -B build -G Ninja

   # 2. Compilar el ejecutable
   cmake --build build

   # 3. Lanzar la aplicación
   build\MultistreamChat.exe
   ```

---

## 📖 Guía de Uso y Configuración

### 1. Configurar los Canales
1. Abre la aplicación y haz clic en la sección **Canales** en el menú lateral.
2. Ingresa el nombre o identificador de tu canal en cada plataforma deseada:
   - **Twitch**: Nombre de usuario (ejemplo: `shroud`, `ibai`).
   - **YouTube**: `@nombre_canal` (ejemplo: `@LofiGirl`), ID del canal o URL del directo.
   - **Kick**: Slug del canal (ejemplo: `xqc`, `westcol`).
   - **TikTok**: Nombre de usuario sin `@` (ejemplo: `streamer_name`).
3. Haz clic en **Guardar Canales**.

### 2. Iniciar la Conexión
- Pulsa el botón **CONECTAR TODOS** en la barra lateral o conecta plataformas de forma individual desde la tarjeta de cada canal.
- El semáforo de estado en el panel lateral cambiará a verde (● ONLINE) para cada plataforma que esté transmitiendo en vivo.

### 3. Persistencia de Configuración
Las preferencias de estilo, canales configurados y parámetros anti-spam se guardan automáticamente en:
`%AppData%/Roaming/Multistream/multistream_chat_config.json`

---

## 🌐 Modo Web / Móvil y Overlay OBS (Servidor Local Integrado)

MultiChat cuenta con un **servidor web y WebSocket HTTP local integrado**:
- **Acceso en PC**: Abre `http://localhost:8080` en cualquier navegador.
- **Acceso en Móvil (Wi-Fi)**: Abre `http://<IP-LOCAL>:8080` desde tu teléfono conectado a la misma red local para leer el chat en vivo sin necesidad de un segundo monitor.
- **Overlay OBS Studio transparente**: Agrega una fuente de tipo **Navegador** en OBS con la URL `http://localhost:8080/?overlay=true` para superponer el chat en vivo con fondo transparente sobre tu transmisión.
- La aplicación de escritorio es la interfaz principal y el servidor web se activa con un solo clic desde la sección **Modo Web / Móvil**.

---

## 🎥 Uso como Overlay en OBS Studio

Puedes mostrar el chat unificado en OBS de dos formas sencillas:

### Opción 1: Fuente de Navegador (Recomendada)
1. En MultiChat, activa el **Modo Web / Móvil**.
2. Copia la URL del Overlay: `http://localhost:8080/?overlay=true`.
3. En OBS Studio, añade una fuente **Navegador (Browser Source)** con esa URL.
4. Ajusta el ancho y alto deseado (ej. 450x700 px). Tendrás un chat flotante transparente con animaciones fluidas y sin bordes.

### Opción 2: Captura de Ventana
1. En MultiChat, ve a **Estilo del chat** y selecciona el preset **Transparent**.
2. En OBS Studio, añade una fuente **Captura de Ventana** seleccionando `MultiChat`.

---

## 📦 Versiones y Descargas (Portable Release)

No requieres instalar compiladores ni herramientas si solo vas a usar la aplicación:
1. Dirígete a la sección de [Releases](https://github.com/EstebanHdez1/MultiChat/releases).
2. Descarga el paquete ZIP portable: `MultiChat-v1.0.0-Windows-Portable.zip`.
3. Descomprime en cualquier carpeta y ejecuta `MultiChat.exe`. ¡Listo!

---

## 🛠️ Instalación de Dependencias y Compilación

Si deseas desarrollar o compilar desde el código fuente:
- Revisa el archivo [`DEPENDENCIAS.txt`](file:///d:/Proyectos/HTML/Chat/DEPENDENCIAS.txt).
- Ejecuta `INSTALAR_DEPENDENCIAS.bat` para instalar automáticamente el toolchain con MSYS2.
- Para empaquetar una nueva versión portable, ejecuta `powershell scripts/package_portable.ps1`.

---

## 💡 Guía de Extensión para Desarrolladores e IA

### Agregar una Nueva Plataforma
1. Crear `NewPlatform.h` y `NewPlatform.cpp` en `src/platforms/` heredando de `BasePlatform`.
2. Implementar los métodos virtuales puros:
   - `doConnect()`
   - `doDisconnect()`
   - `startChatListener()`
   - `stopChatListener()`
3. Emitir mensajes recibidos llamando a `emitChatMessage(msg)` y `emitDonation(don)`.
4. Registrar la nueva plataforma en `ConnectionManager::initialize()` y en `CMakeLists.txt`.
5. Añadir el icono en `resources/icons/` y registrarlo en `resources.qrc`.

### Agregar Nuevos Tipos de Mensajes
1. Extender el enum `MessageType` en `src/models/ChatMessage.h`.
2. Añadir la regla de filtrado en `src/core/ChatManager.h` (`ChatFilter`).
3. Añadir el renderizado HTML y colores correspondientes en `ChatWidget::formatMessageHtml()`.
