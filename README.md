# UWB650_MC - UWB650 Module Controller / Test Bench

Тестовый стенд для настройки, отладки и тестирования UWB-модулей **UWB650** (Serial Ranging Communication and Positioning Module). Работает на микроконтроллере **ESP32-S3** и предоставляет веб-интерфейс для полного управления модулем.

## Быстрый старт

### Требования

- **ESP-IDF 5.5** (Espressif IoT Development Framework)
- **ESP32-S3** development board (например, CTRL-ROC-GRAPHITE-S3)
- **UWB650** модуль, подключённый по UART

### Сборка и прошивка

```bash
# Через ESP-IDF CLI
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor

# Или через build.bat (Windows, пути настроены под конкретную машину)
build.bat
```

### Подключение к устройству

1. После прошивки ESP32-S3 создаёт WiFi точку доступа: `UWB650MC-XXXX` (пароль: `12345678`)
2. Подключитесь к ней и откройте `http://192.168.4.1`
3. Три страницы: **Dashboard** (рейнджинг), **Settings** (конфигурация модуля), **WiFi** (сеть)

## Назначение проекта

Самостоятельная утилита (тестовый стенд) для работы с модулями UWB650 перед их интеграцией в другие системы. Позволяет:

- Конфигурировать все параметры модуля (PAN ID, адрес, мощность, data rate, preamble code и др.)
- Запускать одиночный и непрерывный рейнджинг (измерение расстояния) до целевого модуля
- Мониторить результаты в реальном времени через WebSocket
- Отправлять произвольные AT-команды через веб-консоль
- Сохранять конфигурацию в NVS (энергонезависимая память ESP32) и Flash модуля

## Аппаратное подключение

```
ESP32-S3                    UWB650 Module
┌─────────┐                ┌──────────┐
│  GPIO 17 ├───── TX ─────>│ RX       │
│  GPIO 18 ├───── RX ─────<│ TX       │
│  GND     ├───────────────│ GND      │
│  3.3V    ├───────────────│ VCC      │
└─────────┘                └──────────┘
UART2, 115200 baud, 8N1
```

## Структура проекта

```
UWB650_MC/
├── README.md                      # Этот файл
├── CLAUDE.md                      # Контекст для AI-ассистентов
├── CMakeLists.txt                 # Корневой CMake (ESP-IDF проект)
├── sdkconfig.defaults             # Конфигурация SDK (ESP32-S3, WebSocket, WiFi)
├── partitions.csv                 # Таблица разделов Flash (NVS 24KB + App 3MB)
├── build.bat                      # Windows-скрипт сборки
│
├── main/
│   ├── CMakeLists.txt
│   └── main.c                     # Точка входа: app_main() — инициализация всех компонентов
│
├── components/
│   ├── device_config/             # Хранение конфигурации в NVS
│   │   ├── include/
│   │   │   ├── device_config.h    # Структура device_config_t, API загрузки/сохранения
│   │   │   └── board_config.h     # Пины UART (GPIO 17/18), baudrate, буфер
│   │   └── device_config.c        # Реализация: NVS blob, CRC32, defaults
│   │
│   ├── uwb650_driver/             # Драйвер модуля UWB650
│   │   ├── include/
│   │   │   └── uwb650_driver.h    # Типы (range_result, stats), полный API
│   │   └── uwb650_driver.c        # UART TX/RX, парсинг ответов, рейнджинг
│   │
│   ├── webserver/                 # HTTP-сервер + WebSocket + REST API
│   │   ├── include/
│   │   │   └── webserver.h        # Публичный API (init, broadcast)
│   │   ├── pages.h                # Объявления HTML-страниц
│   │   ├── http_server.c          # Инициализация HTTPD, маршрутизация страниц
│   │   ├── rest_api.c             # 16 REST-эндпоинтов (JSON, cJSON)
│   │   ├── ws_handler.c           # WebSocket: до 4 клиентов, broadcast
│   │   └── pages.c                # Встроенные HTML/CSS/JS страницы (C-строки)
│   │
│   └── wifi_manager/              # Управление WiFi AP+STA
│       ├── include/
│       │   └── wifi_manager.h     # API: init, connect, scan
│       └── wifi_manager.c         # AP всегда включена, STA опционально
│
└── docs/                          # Документация
    ├── ARCHITECTURE.md            # Архитектура и компоненты
    ├── API_REFERENCE.md           # REST API и WebSocket протокол
    ├── UWB650_PROTOCOL.md         # AT-команды модуля UWB650
    ├── CONFIGURATION.md           # Все параметры конфигурации
    └── *.pdf                      # Даташиты модуля UWB650 (V2.2, V11)
```

## Архитектура (обзор)

```
┌─────────────────────────────────────────────────────┐
│                    Web Browser                       │
│          http://192.168.4.1 (WiFi AP)               │
└──────────┬────────────────────┬─────────────────────┘
           │ HTTP/REST          │ WebSocket /ws
           ▼                    ▼
┌─────────────────────────────────────────────────────┐
│              ESP32-S3 (FreeRTOS)                    │
│                                                      │
│  ┌────────────┐  ┌────────────┐  ┌────────────────┐ │
│  │ webserver   │  │ wifi_      │  │ device_config  │ │
│  │ (HTTP+WS+  │  │ manager    │  │ (NVS storage)  │ │
│  │  REST API)  │  │ (AP+STA)   │  │                │ │
│  └──────┬─────┘  └────────────┘  └────────────────┘ │
│         │                                            │
│  ┌──────▼──────────────────────────────────────────┐ │
│  │            uwb650_driver                         │ │
│  │  UART TX/RX task → AT-команды → парсинг ответов │ │
│  └──────┬──────────────────────────────────────────┘ │
└─────────┼───────────────────────────────────────────┘
          │ UART2 (GPIO 17/18, 115200 baud)
          ▼
   ┌──────────────┐
   │  UWB650      │
   │  Module      │
   └──────────────┘
```

**Основные потоки данных:**

1. **Конфигурация:** Web UI → REST API (`POST /api/config`) → `device_config` (NVS) → `uwb650_driver` (AT-команды)
2. **Рейнджинг:** Web UI → `POST /api/ranging/start` → `uwb650_driver` (цикл 10 Hz) → callback → WebSocket broadcast → Web UI
3. **Raw-команды:** Web UI → `POST /api/uwb/command` → `uwb650_driver` (`uwb650_send_cmd`) → ответ модуля → Web UI

## Подробная документация

| Документ | Описание |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Детальная архитектура, компоненты, потоки данных, FreeRTOS-задачи |
| [docs/API_REFERENCE.md](docs/API_REFERENCE.md) | Все 16 REST-эндпоинтов + WebSocket-протокол с примерами |
| [docs/UWB650_PROTOCOL.md](docs/UWB650_PROTOCOL.md) | AT-команды модуля UWB650, формат `UWBRFAT+`, парсинг ответов |
| [docs/CONFIGURATION.md](docs/CONFIGURATION.md) | Все параметры `device_config_t`, допустимые значения, дефолты |

## Ключевые технические решения

- **Встроенный Web UI** — HTML/CSS/JS хранятся как C-строки в `pages.c` (не файловая система). Три страницы: Dashboard, Settings, WiFi
- **Синхронный протокол команд** — `cmd_mutex` сериализует AT-команды, `resp_sem` ожидает OK/ERROR. Таймаут 1 сек.
- **Непрерывный рейнджинг** — отдельная FreeRTOS-задача с частотой 10 Hz, результаты через callback → WebSocket
- **Двойное сохранение конфигурации** — NVS (ESP32) для автозагрузки при старте + Flash модуля (AT+FLASH)
- **WiFi AP всегда включена** — гарантированный доступ к устройству; STA-подключение к внешней сети опционально

## Связь с проектом molt_rtls

Этот проект — **тестовый стенд** для модулей UWB650. Модули UWB650 планируются как замена Makerfabs DW3000 (STM32 + DW3000) в системе позиционирования [molt_rtls](../molt_rtls/). Даташиты модуля находятся в `docs/`.

## Версия

- **Firmware:** 0.1.0
- **ESP-IDF:** 5.5
- **Target:** ESP32-S3
