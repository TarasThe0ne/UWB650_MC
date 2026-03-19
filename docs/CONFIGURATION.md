# Конфигурация устройства

Вся конфигурация хранится в структуре `device_config_t` (файл `components/device_config/include/device_config.h`).

## Хранение

- **NVS namespace:** `uwb650mc`
- **NVS key:** `config`
- **Формат:** бинарный blob (вся структура целиком)
- **Валидация:** magic `0x55574230` ("UWB0") + CRC32
- **При ошибке валидации:** автоматически применяются дефолтные значения

## Параметры

### Параметры UWB650 модуля

| Параметр | Поле | Тип | Диапазон | По умолчанию | AT-команда | REST API key |
|---|---|---|---|---|---|---|
| Скорость UART | `baudrate` | uint8 | 0–5 | 1 (115200) | BAUDRATE | `baudrate` |
| Скорость данных UWB | `data_rate` | uint8 | 0–1 | 1 (6.8M) | DATARATE | `dataRate` |
| PAN ID | `pan_id` | uint16 | 0x0000–0xFFFE | 0x0000 | DEVICEID | `panId` (hex str) |
| Адрес узла | `node_addr` | uint16 | 0x0000–0xFFFE | 0x0000 | DEVICEID | `nodeAddr` (hex str) |
| Мощность TX | `tx_power` | uint8 | 0–10 | 10 (27.7dBm) | POWER | `txPower` |
| Код преамбулы | `preamble_code` | uint8 | 9–24 | 9 | PREAMBLECODE | `preambleCode` |
| CCA | `cca_enable` | uint8 | 0/1 | 0 | CCAENABLE | `ccaEnable` |
| ACK | `ack_enable` | uint8 | 0/1 | 0 | ACKENABLE | `ackEnable` |
| Шифрование | `security_enable` | uint8 | 0/1 | 0 | SECURITY | `securityEnable` |
| AES ключ | `security_key` | char[33] | 32 hex | "" | SECURITY | `securityKey` |
| Показ источника | `rx_show_src` | uint8 | 0/1 | 1 | RXSHOWSRC | `rxShowSrc` |
| LED статус | `led_status` | uint8 | 0/1 | 1 | LEDSTATUS | `ledStatus` |
| Приёмник | `rx_enable` | uint8 | 0/1 | 1 | RXENABLE | `rxEnable` |
| Sniff режим | `sniff_enable` | uint8 | 0/1 | 0 | SNIFFEN | `sniffEnable` |
| Задержка антенны | `ant_delay` | uint16 | 0–65535 | 16400 | ANTDELAY | `antDelay` |
| Коррекция расст. | `dist_offset_cm` | int16 | -500..500 | 0 | DISTOFFSET | `distOffsetCm` |
| Координата X | `coord_x_cm` | int32 | -100000..100000 | 0 | COORDINATE | `coordX` |
| Координата Y | `coord_y_cm` | int32 | -100000..100000 | 0 | COORDINATE | `coordY` |
| Координата Z | `coord_z_cm` | int32 | -100000..100000 | 0 | COORDINATE | `coordZ` |

### Цель рейнджинга

| Параметр | Поле | Тип | Диапазон | По умолчанию | REST API key |
|---|---|---|---|---|---|
| PAN ID цели | `target_pan_id` | uint16 | 0x0000–0xFFFF | 0x0000 | `targetPanId` (hex str) |
| Адрес цели | `target_addr` | uint16 | 0x0000–0xFFFF | 0x0001 | `targetAddr` (hex str) |

### WiFi STA

| Параметр | Поле | Тип | Макс. длина | По умолчанию | REST API key |
|---|---|---|---|---|---|
| SSID | `wifi_ssid` | char[33] | 32 | "" (нет) | `wifiSsid` |
| Пароль | `wifi_pass` | char[65] | 64 | "" (нет) | `wifiPass` |

### Служебные поля

| Поле | Тип | Описание |
|---|---|---|
| `magic` | uint32 | `0x55574230` — маркер валидной конфигурации |
| `version` | uint32 | `1` — версия формата структуры |
| `crc` | uint32 | CRC32 всех полей до `crc` |

## Таблица уровней мощности

| Код (`tx_power`) | dBm | Описание |
|---|---|---|
| 0 | -5.0 | Минимум |
| 1 | -2.5 | |
| 2 | 0.0 | |
| 3 | 2.5 | |
| 4 | 5.0 | |
| 5 | 7.5 | |
| 6 | 10.0 | |
| 7 | 12.5 | |
| 8 | 15.0 | |
| 9 | 20.0 | |
| 10 | 27.7 | Максимум (по умолчанию) |

## Таблица скоростей UART

| Код (`baudrate`) | Скорость |
|---|---|
| 0 | 230400 бод |
| 1 | 115200 бод (по умолчанию) |
| 2 | 57600 бод |
| 3 | 38400 бод |
| 4 | 19200 бод |
| 5 | 9600 бод |

## API конфигурации (C)

```c
// Загрузить из NVS или применить дефолты
esp_err_t device_config_load(void);

// Получить указатель (read-only)
const device_config_t *device_config_get(void);

// Получить мутабельный указатель
device_config_t *device_config_get_mut(void);

// Сохранить в NVS
esp_err_t device_config_save(void);

// Установить дефолтные значения (не сохраняет в NVS)
void device_config_set_defaults(device_config_t *cfg);

// Стереть NVS + применить дефолты
esp_err_t device_config_factory_reset(void);

// Вывести конфигурацию в лог
void device_config_log(const device_config_t *cfg);
```

## Два уровня сохранения

1. **NVS ESP32** (`device_config_save()`) — конфигурация загружается при каждом старте ESP32 и применяется к модулю
2. **Flash модуля UWB650** (`uwb650_flash_save()` / AT-команда `FLASH`) — конфигурация сохраняется в памяти самого модуля

При вызове `uwb650_apply_config()` выполняются оба сохранения: параметры отправляются AT-командами и затем записываются в Flash модуля.
