# UWB650_MC — Контекст для AI-ассистентов

## Что это

Тестовый стенд (прошивка ESP32-S3) для настройки и тестирования UWB-модулей **UWB650** по UART. Веб-интерфейс для управления. Планируется использование модулей в системе морского позиционирования molt_rtls (отдельный репозиторий).

## Технологии

- **Язык:** C (ESP-IDF 5.5)
- **MCU:** ESP32-S3
- **Сборка:** CMake через `idf.py` (или `build.bat` на Windows)
- **Зависимости:** только ESP-IDF компоненты (нет внешних библиотек)

## Структура (4 компонента)

```
main/main.c                    — app_main(), инициализация, ~73 строки
components/
  device_config/               — NVS хранение конфигурации (device_config_t)
  uwb650_driver/               — UART драйвер модуля UWB650 (AT-команды)
  webserver/                   — HTTP :80 + REST API (17 эндпоинтов, включая OTA) + WebSocket /ws
  wifi_manager/                — WiFi AP (всегда) + STA (опционально)
```

Полный размер кодовой базы: ~2700 строк C + ~580 строк встроенного HTML/CSS/JS.

## Ключевые файлы для понимания

1. `README.md` — обзор проекта, структура, архитектура
2. `docs/ARCHITECTURE.md` — компоненты, FreeRTOS-задачи, потоки данных, константы
3. `docs/API_REFERENCE.md` — все REST-эндпоинты и WebSocket-протокол с примерами
4. `docs/UWB650_PROTOCOL.md` — AT-команды модуля UWB650
5. `docs/CONFIGURATION.md` — все параметры device_config_t

## Аппаратное подключение

- ESP32-S3 GPIO17 (TX) → UWB650 RX
- ESP32-S3 GPIO18 (RX) ← UWB650 TX
- UART2, 115200 бод, 8N1

## Протокол общения с UWB650

- TX: `UWBRFAT+{CMD}\r\n`
- RX: ответ + `OK\r\n` или `ERROR\r\n`
- Рейнджинг: `+RANGING=({distance_m}),({rssi_dBm})`
- Таймаут команды: 1 сек, рейнджинга: 2 сек

## Конфигурация

- Хранится в NVS blob (namespace `uwb650mc`, key `config`)
- Структура `device_config_t` (~120 байт, magic + CRC32)
- Двойное сохранение: NVS ESP32 + Flash модуля

## WiFi

- AP всегда: `UWB650MC-XXXX`, пароль `12345678`, IP `192.168.4.1`
- STA опционально: настраивается через Web UI или NVS

## Соглашения по коду

- Префикс `s_` для static-переменных модуля (синглтон `s_drv`, `s_config`, `s_clients`)
- FreeRTOS семафоры для синхронизации (cmd_mutex, resp_sem, range_sem, data_mutex)
- ESP-IDF logging: `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`, `ESP_LOGD`
- Возвращаемые значения: `esp_err_t` (ESP_OK, ESP_FAIL, ESP_ERR_TIMEOUT и т.д.)
- Один FW_VERSION define в main.c и rest_api.c — при обновлении менять в обоих местах

## Особенности и подводные камни

- **BAUDRATE не применяется** в `uwb650_apply_config()` — смена скорости UART разорвёт связь
- **HTML/CSS/JS встроены в C-строки** в `pages.c` — редактировать аккуратно (экранирование кавычек!)
- **FW_VERSION дублируется** в `main/main.c:15` и `components/webserver/rest_api.c:18`
- **WiFi STA автореконнект** — при разрыве пытается переподключиться каждые 5 секунд (блокирует event handler)
- **OTA** — обновление прошивки по HTTP (`POST /api/system/ota`), две OTA-партиции ~1.9 МБ каждая
- **Нет аутентификации** — Web UI и API открыты для всех в WiFi-сети

## Контекст: связь с molt_rtls

Модули UWB650 планируются как замена Makerfabs MaUWB (STM32 + DW3000) в системе морского позиционирования molt_rtls. Этот проект — тестовый стенд для отладки модулей до интеграции. Даташиты UWB650 V2.2 и V11 находятся в `docs/`.
