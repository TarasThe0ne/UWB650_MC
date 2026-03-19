# API Reference

Веб-сервер работает на порту 80. Все REST-эндпоинты возвращают JSON. WebSocket — на `/ws`.

## REST API

### Системные

#### `GET /api/status`

Общий статус устройства.

**Ответ:**
```json
{
  "version": "0.1.0",
  "uptime": 1234.0,
  "freeHeap": 245760,
  "minHeap": 220000,
  "uwbReady": true,
  "rangingState": "idle",
  "wifiSta": false,
  "apSsid": "UWB650MC-A1B2",
  "apIp": "192.168.4.1",
  "staIp": "",
  "wsClients": 1
}
```

Поле `rangingState`: `"idle"` | `"running"` | `"error"`

#### `POST /api/system/reboot`

Перезагрузка ESP32. Ответ отправляется до перезагрузки (задержка 500 мс).

**Ответ:** `{"success": true, "message": "Rebooting..."}`

#### `POST /api/system/reset`

Сброс конфигурации в NVS к заводским + перезагрузка.

**Ответ:** `{"success": true, "message": "Factory reset done, rebooting..."}`

#### `POST /api/system/ota`

Обновление прошивки по воздуху (OTA). Тело запроса — raw binary файл прошивки (`.bin`).

**Content-Type:** `application/octet-stream`

**Пример (curl):**
```bash
curl -X POST --data-binary @build/uwb650_mc.bin http://192.168.4.1/api/system/ota
```

**Успех:** `{"success": true, "message": "OTA update successful, rebooting..."}`
Устройство автоматически перезагружается через ~1 сек.

**Ошибки:**
- `400` — нет данных
- `500` — ошибка записи, невалидный образ, нет OTA-партиции

**Примечания:**
- Максимальный размер прошивки: ~1.9 МБ (размер OTA-партиции)
- Используются две OTA-партиции (ota_0, ota_1) — переключение при каждом обновлении
- При невалидном образе обновление отклоняется, текущая прошивка сохраняется

---

### Конфигурация

#### `GET /api/config`

Текущая конфигурация устройства (из RAM, загружается из NVS при старте).

**Ответ:**
```json
{
  "baudrate": 1,
  "dataRate": 1,
  "panId": "0000",
  "nodeAddr": "0000",
  "txPower": 10,
  "preambleCode": 9,
  "ccaEnable": 0,
  "ackEnable": 0,
  "securityEnable": 0,
  "securityKey": "",
  "rxShowSrc": 1,
  "ledStatus": 1,
  "rxEnable": 1,
  "sniffEnable": 0,
  "antDelay": 16400,
  "distOffsetCm": 0,
  "coordX": 0,
  "coordY": 0,
  "coordZ": 0,
  "targetPanId": "0000",
  "targetAddr": "0001",
  "wifiSsid": ""
}
```

Hex-поля (`panId`, `nodeAddr`, `targetPanId`, `targetAddr`) — строки, 4 символа, верхний регистр.

#### `POST /api/config`

Обновить конфигурацию в RAM + сохранить в NVS. Можно отправлять частичный JSON — обновятся только указанные поля.

**Тело запроса** (Content-Type: application/json):
```json
{
  "txPower": 8,
  "targetAddr": "0002"
}
```

**Ответ:** `{"success": true, "message": "Config saved to NVS"}`

**Важно:** Это сохраняет в NVS ESP32, но НЕ отправляет команды в модуль UWB650. Для применения используйте `/api/config/apply`.

#### `POST /api/config/apply`

Применить текущую конфигурацию (из RAM) к модулю UWB650. Выполняет:
1. `uwb650_reset()` — перезагрузка модуля
2. 15 AT-команд последовательно (все параметры кроме baudrate)
3. `uwb650_flash_save()` — сохранение в Flash модуля

**Ответ:** `{"success": true, "message": "Configuration applied to UWB650 module"}`

---

### Рейнджинг

#### `GET /api/ranging`

Последний результат рейнджинга.

**Ответ:**
```json
{
  "distance": 3.456,
  "rssi": -67.5,
  "valid": true,
  "seq": 42,
  "timestamp": 12345678,
  "state": "running"
}
```

| Поле | Тип | Описание |
|---|---|---|
| `distance` | float | Расстояние в метрах. -1.0 если измерение неудачное |
| `rssi` | float | Уровень сигнала в dBm. 0.0 если неудачно |
| `valid` | bool | `true` если distance >= 0 |
| `seq` | uint32 | Порядковый номер измерения (сбрасывается при старте) |
| `timestamp` | uint32 | Время в мс (esp_timer) |
| `state` | string | `"idle"` или `"running"` |

#### `POST /api/ranging/start`

Запуск непрерывного рейнджинга (10 Hz). Цель берётся из конфигурации (`targetPanId`, `targetAddr`). Результаты транслируются через WebSocket.

**Ответ:** `{"success": true, "message": "Ranging started"}`

**Ошибки:**
- `400` — рейнджинг уже запущен
- `500` — не удалось создать задачу

#### `POST /api/ranging/stop`

Остановка непрерывного рейнджинга.

**Ответ:** `{"success": true, "message": "Ranging stopped"}`

---

### UWB-модуль (прямое управление)

#### `GET /api/uwb/stats`

Статистика драйвера.

**Ответ:**
```json
{
  "txCount": 150,
  "rxCount": 12400,
  "okCount": 145,
  "errorCount": 2,
  "timeoutCount": 3,
  "rangingCount": 100,
  "rangingOk": 95,
  "rangingFail": 5
}
```

#### `POST /api/uwb/stats/reset`

Сброс всех счётчиков статистики.

**Ответ:** `{"success": true, "message": "Statistics reset"}`

#### `POST /api/uwb/query`

Запрос текущего значения параметра из модуля (отправляет `UWBRFAT+{param}?\r\n`).

**Тело:**
```json
{"param": "POWER"}
```

**Ответ:**
```json
{
  "param": "POWER",
  "value": "10",
  "success": true
}
```

#### `POST /api/uwb/command`

Отправка произвольной AT-команды в модуль. Команда автоматически оборачивается в `UWBRFAT+{cmd}\r\n`.

**Тело:**
```json
{"cmd": "POWER=8"}
```

**Ответ:**
```json
{
  "cmd": "POWER=8",
  "response": "",
  "success": true
}
```

Поле `response` содержит данные, полученные до OK/ERROR (для set-команд обычно пустое, для query — значение параметра).

---

### WiFi

#### `GET /api/wifi/scan`

Сканирование WiFi-сетей (блокирующее).

**Ответ:**
```json
[
  {
    "ssid": "MyNetwork",
    "rssi": -45,
    "channel": 6,
    "auth": 3
  }
]
```

Значения `auth`: 0=Open, 1=WEP, 2=WPA, 3=WPA2, 4=WPA/WPA2, 5=WPA3, 6=WPA2/WPA3, 7=WAPI, 8=OWE

#### `POST /api/wifi/connect`

Подключение к WiFi-сети. Автоматически сохраняет SSID и пароль в NVS-конфигурацию.

**Тело:**
```json
{
  "ssid": "MyNetwork",
  "password": "mypassword"
}
```

**Ответ:** `{"success": true, "message": "Connecting..."}`

#### `POST /api/wifi/disconnect`

Отключение от WiFi STA.

**Ответ:** `{"success": true, "message": "Disconnected"}`

---

## WebSocket

Эндпоинт: `ws://{host}/ws`

Максимум 4 одновременных клиента. При отключении клиента слот освобождается автоматически.

### Входящие сообщения (от клиента)

| Сообщение | Ответ |
|---|---|
| Содержит `"ping"` | `{"type":"pong"}` |

### Исходящие сообщения (от сервера)

#### `ranging` — Результат измерения

Отправляется при каждом измерении в режиме непрерывного рейнджинга (10 Hz).

```json
{
  "type": "ranging",
  "data": {
    "distance": 3.456,
    "rssi": -67.50,
    "valid": true,
    "seq": 42,
    "timestamp": 12345678
  }
}
```

#### `status` — Обновление статуса

Отправляется при вызове `webserver_ws_broadcast_status()`.

```json
{
  "type": "status",
  "data": {
    "uwb": true,
    "wifi": false,
    "ranging": "running",
    "heap": 245760,
    "uptime": 1234
  }
}
```

#### `log` — Лог-сообщение

Отправляется при вызове `webserver_ws_broadcast_log()`.

```json
{
  "type": "log",
  "data": {
    "level": "INFO",
    "message": "Ranging started"
  }
}
```

---

## Формат ответов

### Успешный ответ

```json
{"success": true, "message": "описание"}
```

### Ошибка

HTTP status 400 или 500:
```json
{"success": false, "error": "описание ошибки"}
```

---

## HTML-страницы

| URL | Страница |
|---|---|
| `GET /` | Dashboard — рейнджинг, системный статус, статистика |
| `GET /settings` | Settings — конфигурация модуля, raw AT console |
| `GET /wifi` | WiFi — статус, подключение, сканирование |
| `GET /favicon.ico` | SVG-иконка |
