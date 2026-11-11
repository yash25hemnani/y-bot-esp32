# Handover: Y-Bot walkthrough from zero

_Written 2026-11-12, for the session on 2026-11-13. Plan details are in `docs/roadmap.md`._

## How to start the next chat

Paste this:

> Read `docs/handover.md` and `docs/roadmap.md`. Today we walk through Y-Bot from zero: first BLE (firmware + app), then WebSocket (firmware + backend), then the whole architecture. I've forgotten most of it. Teach, don't write code.

## Rules for the assistant

- **Learning mode.** Explain, ask check questions, and review what the user types. Don't edit code unless the user explicitly asks for a specific change ("do it", "wire it up").
- **Explain at a high level first**, then go down to the code. Use the file:line references below rather than pasting whole files.
- **One build at a time:** `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`. Two builds at once corrupt `.pio`; fix it with `pio run -t clean`.
- **Commits:** the user makes them on `main`, with a one-line message and no co-author.
- **Secrets** (`wifi_password`, `hmac_secret`) are never readable over BLE and never logged, in the firmware or the backend. The backend also never logs the signature.
- **Hardware:**
  - The TP4056 never goes on the battery.
  - Bench test in stages, starting from USB only.
  - Confirm the user has a multimeter before any power stage.

## Where things stand

- **Firmware:** WebSocket client with HMAC login is done; `Command.source` was added today. It builds (RAM 18.7 %, flash 56.7 %), but **none of it has been run on the robot yet**. The bench test plan is the "Testing" section at the end.
- **Uncommitted work** (the user commits it):
  - Firmware:
    - `feat: add WebSocket client with HMAC login`
    - `feat: tag commands with their source (BLE or WS)`
    - If the parser change isn't committed yet: `refactor: extract shared CommandParser from BleManager`
  - Backend: the panel refactor + `src/ws/` (`feat: add device WebSocket endpoint with HMAC login`).
- **Next coding step after the walkthrough:** roadmap step 1, sending replies back the way the command came (the WS outbox and `Responder`).

---

## Part 0: The big picture (~15 min)

```
            ┌──────────── BLE (setup, nearby control) ────────────┐
            │                                                     ▼
  Flutter app ──HTTPS (future)──► Backend (Express) ◄──WebSocket── ESP32 robot
                                   + admin panel         (robot dials out)
```

Five ideas to rebuild first:
1. **The robot has two ways in:** BLE (someone standing nearby) and the WebSocket (over the internet, through the backend). Both carry the **same text commands**.
2. **The protocol** is `module:action[:key[:value]]`, for example `led:toggle` or `settings:write:wifi_name:Home`. Everything after the 3rd colon is the value, so passwords can contain `:`.
3. **Every command takes the same path:** parse → `Command` struct → `CommandRouter` queue → that module's `onCommand`.
4. **The robot runs on FreeRTOS tasks**, separate loops running in parallel. `loop()` does nothing. The tasks are:

   | Task | Core | Priority | Job |
   |---|---|---|---|
   | `cmd_router` | 1 | 5 | Takes commands off the queue and calls the module |
   | `wifi_mgr` | 0 | 4 | Joins and rejoins WiFi, sends `wifi:status` |
   | `ws_client` | 0 | 3 | The WebSocket connection |
   | BLE stack | — | — | Calls `onWrite` when the phone writes |
   | Motor watchdog | — | — | Coasts the motors 500 ms after the last command |

5. **Queues and flags connect the tasks.** One task never calls another task's hardware or library directly. The rule "only `ws_client` may call `ws.*`" is why the reply outbox is needed.

Read: [src/main.cpp](../src/main.cpp), the setup order. Commands are registered on the router before BLE/WiFi/WS start.

---

## Part 1: BLE, from firmware to app (~1.5 h)

### Concepts first
- **Peripheral and central.** The robot is the *peripheral*: it advertises and waits. The phone is the *central*: it scans and connects.
- **GATT.** A *service* (one UUID) groups *characteristics* (each has a UUID). Y-Bot has one service with two characteristics:
  - `...90cd` **RX**, `PROPERTY_WRITE`: phone → robot (commands).
  - `...90ce` **TX**, `PROPERTY_NOTIFY`: robot → phone (replies).
- **Notify** only works after the phone subscribes. That's the `BLE2902` descriptor (CCCD).
- **MTU.**
  - The default BLE packet carries only 20 bytes.
  - `settings:write:hmac_secret:<64 hex>` is about 90 bytes. It works because flutter_blue_plus asks for a larger MTU (512) when it connects on Android.
  - In nRF Connect you may need to request the MTU by hand.
- **No security on BLE yet.** Anyone in range can connect and write. "Setup mode" (roadmap §6) fixes this.

### Firmware: read in this order

| # | File | What to understand |
|---|---|---|
| 1 | [BleManager.h:78-103](../src/comms/ble/BleManager.h#L78-L103) `init` | Creating the service and characteristics, advertising, restarting advertising on disconnect |
| 2 | [BleManager.h:41-75](../src/comms/ble/BleManager.h#L41-L75) `onWrite` → `handleWrite` | Runs on the BLE task. Parse → `submit` → immediate `ok:queued` / `error:*` |
| 3 | [CommandParser.h](../src/comms/protocol/CommandParser.h) | Splitting on the first 3 colons; "never truncate, reject instead" (`error:too_long`) |
| 4 | [Commands.h](../src/comms/protocol/Commands.h) | Fixed-size `char[]` fields (fit in a queue); the new `source` field |
| 5 | [CommandRouter.h](../src/app/CommandRouter.h) | Queue of 10, `submit` waits ≤10 ms (otherwise `error:busy`), dispatch loop on core 1 |
| 6 | [SettingsModule.h](../src/modules/settings/SettingsModule.h) | Preferences (NVS flash), allowed keys, write-only secrets, `wifi_ready` / `setup_completed` |
| 7 | [WifiManager.h:206-212](../src/comms/wifi/WifiManager.h#L206-L212) + `sendStatus` | Why `wifi:reconnect` only sets a flag; the status arrives later from another task |
| 8 | [BleManager.h:106-115](../src/comms/ble/BleManager.h#L106-L115) `sendResponse` | The mutex: several tasks can reply at the same time |

**Two replies per command:** `ok:queued` means "received". The module's real answer, for example `settings:write:wifi_name:ok`, comes **afterwards**, from the `cmd_router` task.

### App: read in this order (`C:\Users\it\StudioProjects\y_bot_app\lib\`)

| # | File | What to understand |
|---|---|---|
| 1 | `features/bluetooth/services/bluetooth_service.dart` | The same UUIDs as the firmware, scan, connect |
| 2 | `features/bluetooth/services/permission_service.dart` | Android BLE permissions |
| 3 | `features/bluetooth/providers/bluetooth_connection_provider.dart` + `models/…data.dart` | Riverpod state: which robot is connected |
| 4 | `features/bluetooth/screens/bluetooth_screen.dart` | Scan list → connect |
| 5 | `features/settings/services/robot_settings_link.dart` | **The key file.** The app side of the protocol: one command at a time, listen before writing, retry on `busy`, error codes → messages |
| 6 | `features/settings/screens/settings_screen.dart` | Save flow: write changed fields → `wifi:reconnect` → show the result |
| 7 | `features/home/screens/home_screen.dart`, `features/controller_pad/screens/controller_pad_screen.dart` | Direct writes (`led:on`, `motor:forward:speed:100`). Each screen sets up its own BLE link: to be replaced by one shared `RobotLink` |

### Trace exercises (do these out loud)
1. **"Save" with a new WiFi name.** Follow it from the button press to Preferences and back to the green notice. Name every task it passes through.
2. **What does the app see** if two commands are sent within the same millisecond? Answer: `robot_settings_link.dart` prevents it, and if it happened anyway the robot would reply `error:busy`.
3. **Why does `settings:read:hmac_secret` return `error:write_only`?** Who could read the secret otherwise?

### Check questions
- What's the difference between write and notify?
- Why does `onWrite` return straight away instead of running the module itself?
- Where would a reply go if the phone disconnected before the module finished? (`sendResponse` drops it.)

---

## Part 2: WebSocket, from firmware to backend (~1.5 h)

### Concepts first
- **WebSocket:**
  - It starts as an HTTP request, then "upgrades" to a long-lived two-way text connection.
  - Either side can send at any time.
- **The robot is the client.** It dials out to the server, so there's no port forwarding and no need to know its IP.
- **HMAC challenge–response login.** The secret is never sent. The steps:
  1. The server sends a random **nonce**: `auth:challenge:<nonce>`.
  2. The robot computes `hex(HMAC-SHA256(key = hmac_secret, msg = deviceId:nonce))` and sends `auth:login:<deviceId>:<sig>`.
  3. The server computes the same thing with its stored copy of the secret and compares in **constant time** (`timingSafeEqual`).
  4. It replies `auth:ok` or `auth:fail`.

  A new nonce for every connection means a recorded login can't be replayed.
- **Heartbeat:** both sides ping, so a dead connection is noticed within ~15–45 s.
- **Known gap:** only the login is protected. Messages after it are plain text until TLS (`wss://`), roadmap §6.

### Firmware: read in this order

| # | File | What to understand |
|---|---|---|
| 1 | [Server.h](../src/config/Server.h) | `WS_HOST` (PC LAN IP), port 4000, path `/device` |
| 2 | [Hmac.h](../src/comms/ws/Hmac.h) | mbedtls HMAC; the key is the 64-character hex **string**, not decoded bytes |
| 3 | [WsClient.h:15-21](../src/comms/ws/WsClient.h#L15-L21) | State machine: DISCONNECTED → WAIT_CHALLENGE → WAIT_AUTH_OK → ONLINE |
| 4 | [WsClient.h `taskLoop` / `start` / `stop`](../src/comms/ws/WsClient.h) | Starts when WiFi comes up and stops when it drops; keys are re-read on every connect |
| 5 | `onEvent` → `handleText` | Each message is handled according to the current state |
| 6 | `handleChallenge`, `handleAuthReply` | Login; 30 s retry after `auth:fail`; never logs the signature or secret |
| 7 | `handleCommand` | Same parser as BLE; `settings` is forbidden over WS; `source = WS`; `ok:queued` |

### Backend: read in this order (`C:\Users\it\Desktop\Projects\y-bot-admin-panel\src\`)

| # | File | What to understand |
|---|---|---|
| 1 | `server.ts` | The HTTP server and robot WebSocket share port 4000 (`attachDeviceSocket`) |
| 2 | `ws/deviceSocket.ts` | connection → nonce → `verifyLogin` → `onlineDevices` map → heartbeat → close/replaced. `sendToDevice` exists but nothing calls it yet |
| 3 | `db/models/Device.ts` | `id` (database key) vs `deviceId` (what the robot sends), `hmacSecret`, `isRevoked` |
| 4 | `controllers/panel/devices.ts` | Register (secret shown once), rotate/revoke/delete → `disconnect()` kicks the robot off |
| 5 | `middleware/panel.middleware.ts`, `routes/views.routes.ts` | Panel login with a cookie session; `ownedBy` checks |
| 6 | `app.ts`, `utils/tokens.ts`, `middleware/auth.middleware.ts`, `controllers/auth.controller.ts` | API login (JWT access token, refresh cookie). **The device API isn't mounted.** |
| 7 | `controllers/device.controller.ts` | The unused device API and its known bugs (roadmap §3) |
| 8 | `scripts/fake-robot.ts` | A robot simulated in Node. Useful for testing the backend without hardware |

### Trace exercises
1. **Power on the robot (WiFi and keys saved) until the panel shows "online".** List every message on the wire, and which task or function handles each one.
2. **Rotate the key in the panel.** What happens on the server, then on the robot, and why does it retry only every 30 s?
3. **The server sends `motor:forward:speed:100`.** Follow it to the motor. Where does the reply go today? (To BLE: the bug that roadmap step 1 fixes.)

---

## Part 3: The whole architecture (~30 min)

Draw the end state together:
```
App ──login──────────► /api/auth                      (exists)
App ──list/register──► /api/devices                   (controller exists; fix + mount)
App ──command────────► /api/devices/:id/commands      (missing)
                          └► sendToDevice ─► robot ─► Responder ─► WS outbox ─► reply ─► app
App ──BLE setup──────► robot                          (exists)
```
Then walk through the gaps in roadmap order (§1 → §7) and start §1: reply routing.

---

## Testing (when ready; USB power only, no battery, motors unplugged)
1. **Boot:** flash and open the monitor. Expect `Booting...` → `Ready...`, and `WS: device_id or hmac_secret not set` if WiFi is saved but the keys aren't.
2. **BLE (nRF Connect):** subscribe to `...90ce` and write to `...90cd`:
   - `led:toggle`, `battery:read`;
   - `settings:read:wifi_password` → `error:write_only`;
   - garbage → `error:format`.
3. **WiFi:** write `wifi_name` / `wifi_password`, reboot. It should connect, and the password must not appear in serial.
4. **WebSocket:**
   - Start the backend (`npm run dev`) and check the PC IP matches `Server.h` (`ipconfig`).
   - Register a device in the panel and write `device_id` + `hmac_secret` over BLE.
   - Expect `WS: connecting` → `connected, waiting for challenge` → `login sent` → `online`, and the panel shows it online.
   - Then rotate the key: expect `WS: auth failed` and a retry every 30 s.
5. **If it's stuck at `connecting`:** allow port 4000 inbound in Windows Firewall. **`auth failed`:** re-send the ID and secret.
