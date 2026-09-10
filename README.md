# Y-Bot

![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Framework](https://img.shields.io/badge/framework-Arduino-teal)
![Build](https://img.shields.io/badge/build-PlatformIO-orange)
![WiFi](https://img.shields.io/badge/comms-WiFi-brightgreen)
![BLE](https://img.shields.io/badge/comms-BLE-9cf)

ESP32 firmware (PlatformIO, Arduino framework) that reacts to a physical
button press and to remote commands sent over WiFi or BLE, routing both
into shared modules such as the onboard LED.

## Features

### Physical interaction handling

A press on the BOOT button is captured by an ISR and published onto an
event bus, decoupling hardware input from module logic.

```mermaid
sequenceDiagram
    participant Btn as Button (BOOT_PIN)
    participant ISR as ButtonDriver::isr
    participant Bus as EventBus
    participant Led as LedModule

    Btn->>ISR: FALLING interrupt
    ISR->>Bus: publishFromISR(PAT_DETECTED)
    Bus->>Led: onEvent(event)
    Led->>Led: toggle()
```

### WiFi command handling

An HTTP server accepts `GET /cmd?module=...&action=...` requests and
queues them as commands.

```mermaid
sequenceDiagram
    participant Client as HTTP Client
    participant Wifi as WifiManager
    participant Router as CommandRouter
    participant Led as LedModule

    Client->>Wifi: GET /cmd?module=led&action=toggle
    Wifi->>Router: submit(Command)
    Wifi-->>Client: 200 "queued: led/toggle"
    Router->>Led: handler("led")(cmd)
    Led->>Led: toggle() / on() / off()
```

### BLE command handling

A BLE GATT characteristic accepts writes in `module:action` format and
queues them the same way as WiFi.

```mermaid
sequenceDiagram
    participant Client as BLE Client
    participant Ble as BleManager
    participant Router as CommandRouter
    participant Led as LedModule

    Client->>Ble: write "led:toggle"
    Ble->>Ble: handleWrite() parses module:action
    Ble->>Router: submit(Command)
    Router->>Led: handler("led")(cmd)
    Led->>Led: toggle() / on() / off()
```

### System overview

```mermaid
graph LR
    Button([Button]) -->|IRQ| ButtonDriver
    ButtonDriver -->|publishFromISR| EventBus
    EventBus --> LedModule

    HTTPClient([HTTP Client]) --> WifiManager
    BLEClient([BLE Client]) --> BleManager
    WifiManager -->|submit| CommandRouter
    BleManager -->|submit| CommandRouter
    CommandRouter --> LedModule

    LedModule --> LED([LED])
```

## Project layout

- `src/app/` — `EventBus` (ISR-safe pub/sub) and `CommandRouter` (module command dispatch)
- `src/comms/wifi/` — `WifiManager`, HTTP command endpoint
- `src/comms/ble/` — `BleManager`, BLE GATT command endpoint
- `src/comms/protocol/` — shared `Command` definition
- `src/drivers/` — hardware input drivers (`ButtonDriver`)
- `src/modules/display/` — output modules (`LedModule`)
- `src/config/Pins.h` — pin assignments

## Build

```
pio run
pio run -t upload
pio device monitor
```
