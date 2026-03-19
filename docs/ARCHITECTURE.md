# Архитектура UWB650_MC

## Обзор системы

ESP32-S3 выступает контроллером для UWB650-модуля. Общение с модулем — по UART через AT-команды. Пользователь управляет устройством через веб-интерфейс, доступный по WiFi.

```
┌──────────────┐     WiFi AP/STA      ┌──────────────────────────────────┐
│  Web Browser  │ ◄──────────────────► │          ESP32-S3                │
│  (Dashboard,  │   HTTP :80           │                                  │
│  Settings,    │   WebSocket /ws      │  ┌───────────┐  ┌────────────┐  │
│  WiFi)        │                      │  │ webserver  │  │wifi_manager│  │
└──────────────┘                       │  └─────┬─────┘  └────────────┘  │
                                       │        │                         │
                                       │  ┌─────▼─────┐  ┌────────────┐  │
                                       │  │uwb650_    │  │device_     │  │
                                       │  │driver     │  │config      │  │
                                       │  └─────┬─────┘  │(NVS)      │  │
                                       │        │        └────────────┘  │
                                       └────────┼────────────────────────┘
                                                │ UART2 (GPIO 17→TX, 18←RX)
                                                ▼ 115200 baud, 8N1
                                       ┌──────────────┐
                                       │  UWB650      │
                                       │  Module      │
                                       └──────────────┘
```

## Компоненты

### 1. `main/main.c` — Точка входа

Файл: `main/main.c` (73 строки)

Функция `app_main()` инициализирует все компоненты в строгом порядке:

1. **NVS Flash** — хранилище конфигурации (с авто-восстановлением при повреждении)
2. **device_config** — загрузка конфигурации из NVS или установка дефолтов
3. **wifi_manager** — запуск WiFi AP (всегда) + STA (если настроено)
4. **webserver** — HTTP-сервер на порту 80 с REST API и WebSocket
5. **uwb650_driver** — инициализация UART, подключение к модулю
6. **uwb650_apply_config()** — отправка сохранённых параметров в модуль

После инициализации — бесконечный цикл с логированием heap каждые 30 секунд.

### 2. `device_config` — Хранение конфигурации

Файлы:
- `components/device_config/include/device_config.h` — структура `device_config_t`, константы
- `components/device_config/include/board_config.h` — аппаратные пины
- `components/device_config/device_config.c` — NVS операции, CRC32

**Принцип работы:**
- Вся конфигурация хранится в одном NVS blob (namespace `uwb650mc`, key `config`)
- Валидация: magic `0x55574230` ("UWB0") + CRC32
- При некорректных данных автоматически применяются дефолтные значения
- Глобальный синглтон `s_config`, доступ через `device_config_get()` / `device_config_get_mut()`

**Зависимости:** `nvs_flash`, `esp_log`

### 3. `uwb650_driver` — Драйвер модуля

Файлы:
- `components/uwb650_driver/include/uwb650_driver.h` — полный API
- `components/uwb650_driver/uwb650_driver.c` — реализация (661 строка)

**Архитектура драйвера:**

```
┌──────────────────────────────────────────────────┐
│                uwb650_driver                      │
│                                                    │
│  ┌─────────────┐     ┌──────────────────────────┐ │
│  │ rx_task      │     │  Публичный API            │ │
│  │ (FreeRTOS)   │     │                          │ │
│  │ Prio: 5      │     │  uwb650_send_cmd()       │ │
│  │              │     │  uwb650_set_*()          │ │
│  │ Читает UART  │     │  uwb650_query()          │ │
│  │ побайтово,   │     │  uwb650_range_single()   │ │
│  │ собирает     │     │  uwb650_ranging_start()  │ │
│  │ строки       │     │  uwb650_ranging_stop()   │ │
│  └──────┬───────┘     └──────────────────────────┘ │
│         │                                          │
│  ┌──────▼───────┐     ┌──────────────────────────┐ │
│  │ process_line()│     │  ranging_task             │ │
│  │              │     │  (FreeRTOS)               │ │
│  │ Распознаёт:  │     │  Prio: 6                  │ │
│  │ - OK/ERROR   │     │                          │ │
│  │ - +RANGING=  │     │  Цикл 10 Hz:             │ │
│  │ - Finished   │     │  range_single()           │ │
│  │   Startup    │     │  → update stats           │ │
│  └──────────────┘     │  → call user callback    │ │
│                        └──────────────────────────┘ │
│  Синхронизация:                                     │
│  - cmd_mutex   (сериализация AT-команд)            │
│  - resp_sem    (ожидание OK/ERROR)                 │
│  - range_sem   (ожидание +RANGING= ответа)         │
│  - data_mutex  (защита last_result и stats)        │
└──────────────────────────────────────────────────────┘
```

**Протокол команд:**
- TX: `UWBRFAT+{CMD}\r\n`
- RX: `{data line}\r\nOK\r\n` или `ERROR\r\n`
- Рейнджинг: `+RANGING=({distance}),({rssi})`
- Таймаут команды: 1 сек, рейнджинга: 2 сек
- Задержка между командами: 50 мс

**Зависимости:** `driver` (UART), `esp_log`, `esp_timer`, `device_config`

### 4. `webserver` — HTTP-сервер и WebSocket

Файлы:
- `components/webserver/include/webserver.h` — публичный API
- `components/webserver/http_server.c` — инициализация HTTPD
- `components/webserver/rest_api.c` — 16 REST-эндпоинтов
- `components/webserver/ws_handler.c` — WebSocket (до 4 клиентов)
- `components/webserver/pages.h` / `pages.c` — встроенные HTML-страницы

**Конфигурация HTTPD:**
- Порт: 80
- Макс. URI handlers: 32
- Макс. открытых сокетов: 8
- Размер стека: 8 KB
- LRU purge: включён
- Wildcard URI matching

**Страницы (встроенные HTML/CSS/JS):**

| URL | Страница | Описание |
|---|---|---|
| `/` | Dashboard | Рейнджинг (расстояние, RSSI), системный статус, UWB-статистика |
| `/settings` | Settings | Все параметры модуля + Raw AT Console |
| `/wifi` | WiFi | Текущий статус, подключение к сети, сканирование |

**UI-стек:**
- Тёмная тема (monospace, зелёный акцент)
- Vanilla JS (без фреймворков)
- WebSocket для real-time обновлений
- Автореконнект WS через 3 секунды
- Периодический polling: status каждые 5 сек, stats каждые 3 сек

**Зависимости:** `esp_http_server`, `esp_log`, `esp_timer`, `device_config`, `uwb650_driver`, `wifi_manager`, `json` (cJSON), `esp_wifi`, `esp_system`

### 5. `wifi_manager` — Управление WiFi

Файлы:
- `components/wifi_manager/include/wifi_manager.h` — API
- `components/wifi_manager/wifi_manager.c` — реализация (193 строки)

**Режимы работы:**
- **AP (всегда):** SSID `UWB650MC-{MAC_LAST_4}`, пароль `12345678`, IP `192.168.4.1`, канал 1, до 4 клиентов
- **STA (опционально):** подключение к внешней WiFi, IP по DHCP, автореконнект через 5 сек

При наличии STA-конфигурации запускается режим APSTA (обе сети одновременно).

**Зависимости:** `esp_wifi`, `esp_event`, `esp_netif`, `esp_log`, `nvs_flash`

## Граф зависимостей компонентов

```
main
  ├── device_config  (nvs_flash, esp_log)
  ├── wifi_manager   (esp_wifi, esp_event, esp_netif, esp_log, nvs_flash)
  ├── uwb650_driver  (driver/uart, esp_log, esp_timer, device_config)
  └── webserver      (esp_http_server, esp_log, esp_timer)
        └── [PRIV] device_config, uwb650_driver, wifi_manager, json, esp_wifi, esp_system
```

## FreeRTOS-задачи

| Задача | Стек | Приоритет | Описание |
|---|---|---|---|
| `uwb_rx` | 4096 | 5 | Побайтовое чтение UART, сборка строк, парсинг ответов |
| `uwb_range` | 4096 | 6 | Непрерывный рейнджинг (10 Hz), создаётся/удаляется динамически |
| HTTPD (системная) | 8192 | — | Обработка HTTP/WS запросов |
| `app_main` | — | 1 | Инициализация + периодический лог heap |

## Потоки данных

### 1. Конфигурация модуля

```
Browser → POST /api/config (JSON) → config_post_handler()
  → device_config_get_mut() — модификация RAM-структуры
  → device_config_save() — запись NVS blob с CRC
  → ответ "Config saved to NVS"

Browser → POST /api/config/apply → config_apply_handler()
  → uwb650_apply_config()
    → uwb650_reset() — перезагрузка модуля
    → uwb650_set_*() — 15 AT-команд последовательно (кроме BAUDRATE)
    → uwb650_flash_save() — сохранение в Flash модуля
```

### 2. Непрерывный рейнджинг

```
Browser → POST /api/ranging/start → ranging_start_handler()
  → uwb650_ranging_start(target_pan, target_addr, ranging_ws_cb)
    → создаёт FreeRTOS-задачу ranging_task

ranging_task (10 Hz loop):
  → uwb650_range_single() — AT+RANGING=1,PAN,ADDR
    → ожидание +RANGING=(dist),(rssi)
    → parse_ranging()
  → обновление stats и last_result (под data_mutex)
  → ranging_ws_cb() → webserver_ws_broadcast_ranging()
    → JSON → broadcast() → все WS-клиенты

Browser: ws.onmessage → {type:"ranging", data:{distance, rssi, valid, seq, timestamp}}
```

### 3. Инициализация при старте

```
app_main():
  1. nvs_flash_init() — с авто-восстановлением
  2. device_config_load() — NVS → s_config (или defaults)
  3. wifi_manager_init(ssid, pass) — AP + опциональный STA
  4. webserver_init() — HTTPD + WS + REST + pages
  5. uwb650_init() — UART2 + rx_task
  6. uwb650_apply_config(cfg) — reset + 15 AT-команд + flash save
  7. while(1): heap logging каждые 30 сек
```

## Flash Layout (partitions.csv)

| Раздел | Тип | Смещение | Размер | Назначение |
|---|---|---|---|---|
| nvs | data/nvs | 0x9000 | 24 KB | Конфигурация устройства |
| phy_init | data/phy | 0xF000 | 4 KB | WiFi PHY калибровка |
| factory | app/factory | 0x10000 | 3 MB | Прошивка |

## Ключевые константы

| Константа | Значение | Где | Описание |
|---|---|---|---|
| `FW_VERSION` | `"0.1.0"` | main.c, rest_api.c | Версия прошивки |
| `UWB_UART_NUM` | UART_NUM_2 | board_config.h | Номер UART для UWB650 |
| `UWB_UART_TX_PIN` | 17 | board_config.h | GPIO TX |
| `UWB_UART_RX_PIN` | 18 | board_config.h | GPIO RX |
| `UWB_UART_BAUD` | 115200 | board_config.h | Скорость UART |
| `UWB_UART_BUF_SIZE` | 1024 | board_config.h | Буфер UART |
| `MAX_WS_CLIENTS` | 4 | ws_handler.c | Макс. WebSocket клиентов |
| `CMD_DELAY_MS` | 50 | uwb650_driver.c | Пауза между AT-командами |
| `UWB650_CMD_TIMEOUT_MS` | 1000 | uwb650_driver.h | Таймаут AT-команды |
| `UWB650_RANGE_TIMEOUT_MS` | 2000 | uwb650_driver.h | Таймаут рейнджинга |
| `UWB650_STARTUP_WAIT_MS` | 3000 | uwb650_driver.h | Ожидание "Finished Startup" |
| `AP_PASSWORD` | `"12345678"` | wifi_manager.c | Пароль WiFi AP |
| `AP_MAX_CONN` | 4 | wifi_manager.c | Макс. клиентов AP |
| `CONFIG_MAGIC` | 0x55574230 | device_config.h | Magic для валидации ("UWB0") |
