# ZET 017 TCP/IP Библиотека для коммуникации

Кроссплатформенная библиотека на языке C для взаимодействия с устройствами ZET 017, ZET 038 и ZET 028 по протоколу TCP/IP.

## Возможности

- **Кроссплатформенная поддержка**: Совместимость с Windows и Linux/Unix
- **Управление несколькими устройствами**: Одновременная работа с несколькими устройствами
- **Сбор данных в реальном времени**: Потоковая передача данных АЦП с настраиваемой частотой дискретизации (2,5-50 кГц)
- **Поддержка двухканального ЦАП**: Возможности цифро-аналогового преобразования
- **Управление конфигурацией**: Программная установка и получение параметров устройства
- **Потокобезопасная архитектура**: Построена на мьютексах и условных переменных
- **TCP/IP коммуникация**: Подключение к устройствам через сетевые интерфейсы

## Supported Hardware

Системы сбора данных серий ZET 017, ZET 038 и ZET 028 с:
- До 8 каналов АЦП с программируемыми усилениями (1x, 10x, 100x)
- До 2 каналов ЦАП
- Настраиваемые частоты дискретизации:
  - АЦП: 2,5 кГц, 5 кГц, 25 кГц, 50 кГц
  - ЦАП: Настраивается до 200 кГц
- Поддержка ICP (Integrated Circuit Piezoelectric) датчиков
- Функциональность цифрового ввода/вывода

## Структура проекта

```bash
zet017tcp/
├── include/
│ └── zet017tcp.h # Публичный API заголовочный файл
├── src/
│ └── zet017tcp.c # Реализация библиотеки
├── example/
│ └── example_zet017tcp.c # Пример использования
├── CMakeLists.txt # Конфигурация сборки
├── README.md
└── README.en.md
```

## Требования

- **CMake** (версия 3.10 или выше)
- **Компилятор C** с поддержкой C99
- **Платформенные библиотеки**:
  - Windows: Winsock2 (`ws2_32`)
  - Linux/Unix: pthreads

## Сборка

### Использование CMake (Рекомендуется)

```bash
# Создание директории сборки
mkdir build
cd build

# Конфигурация проекта
cmake ..

# Сборка библиотеки и примера
cmake --build .
```

## Обзор API

### Основные функции

```c
// Управление сервером
zet017_server_create(struct zet017_server** server_ptr);
zet017_server_free(struct zet017_server** server_ptr);

// Управление устройствами
zet017_server_add_device(struct zet017_server* server, const char* ip);
zet017_server_remove_device(struct zet017_server* server, const char* ip);

// Операции с устройствами
zet017_device_get_info(struct zet017_server* server, uint32_t number, struct zet017_info* info);
zet017_device_get_state(struct zet017_server* server, uint32_t number, struct zet017_state* state);
zet017_device_get_config(struct zet017_server* server, uint32_t number, struct zet017_config* config);
zet017_device_set_config(struct zet017_server* server, uint32_t number, struct zet017_config* config);
zet017_device_start(struct zet017_server* server, uint32_t number);
zet017_device_stop(struct zet017_server* server, uint32_t number);

// Сбор данных
zet017_channel_get_data(struct zet017_server* server, uint32_t number, uint32_t channel, 
                       uint32_t pointer, float* data, uint32_t size);
```

### Структуры данных

```c
struct zet017_config {
    uint32_t sample_rate_adc;    // Частота дискретизации АЦП в Гц
    uint32_t sample_rate_dac;    // Частота дискретизации ЦАП в Гц
    uint32_t mask_channel_adc;   // Битовая маска включенных каналов АЦП
    uint32_t mask_icp;           // Битовая маска каналов с поддержкой ICP
    uint32_t gain[8];            // Настройки усиления для каждого канала (1, 10 или 100)
};

struct zet017_info {
    char ip[16];                 // IP-адрес устройства
    char name[16];               // Имя устройства
    uint32_t serial;             // Серийный номер
    char version[32];            // Версия прошивки
};

struct zet017_state {
    uint16_t connected;          // Статус подключения
    uint32_t pointer_adc;        // Текущая позиция в буфере АЦП
    uint32_t buffer_size_adc;    // Общий размер буфера АЦП
    uint32_t pointer_dac;        // Текущая позиция в буфере ЦАП
};
```

## Пример использования

```c
#include "zet017tcp.h"
#include <stdio.h>

int main() {
    struct zet017_server* server = NULL;
    
    // Инициализация сервера
    if (zet017_server_create(&server) != 0) {
        fprintf(stderr, "Ошибка создания сервера\n");
        return -1;
    }
    
    // Добавление устройства
    if (zet017_server_add_device(server, "192.168.1.100") != 0) {
        fprintf(stderr, "Ошибка добавления устройства\n");
        zet017_server_free(&server);
        return -1;
    }
    
    // Конфигурация устройства
    struct zet017_config config = {
        .sample_rate_adc = 25000,
        .sample_rate_dac = 0,
        .mask_channel_adc = 0x0E,  // Включить каналы 1, 2, 3
        .mask_icp = 0x02,          // Включить ICP на канале 1
        .gain = {1, 10, 100, 1, 1, 1, 1, 1}
    };
    
    if (zet017_device_set_config(server, 0, &config) != 0) {
        fprintf(stderr, "Ошибка конфигурации\n");
    }
    
    // Запуск сбора данных
    if (zet017_device_start(server, 0) != 0) {
        fprintf(stderr, "Ошибка запуска сбора данных\n");
    }
    
    // Чтение данных
    float data[1000];
    if (zet017_channel_get_data(server, 0, 0, 0, data, 1000) == 0) {
        printf("Успешно прочитано 1000 отсчетов с канала 0\n");
    }
    
    // Очистка ресурсов
    zet017_device_stop(server, 0);
    zet017_server_free(&server);
    
    return 0;
}
```

## Обработка ошибок

Все функции возвращают 0 при успехе и отрицательные значения при ошибке.

## Сетевой протокол

Библиотека взаимодействует с устройствами через три TCP-порта:

- **Командный порт**: 1808 - Конфигурация и управление устройством
- **Порт данных АЦП**: 2320 - Поток данных аналого-цифрового преобразователя
- **Порт данных ЦАП**: 3344 - Поток данных цифро-аналогового преобразователя

## Поддержка платформ

### Windows

- Требуется библиотека Winsock2 (ws2_32.lib)
- Файлы проектов Visual Studio можно сгенерировать с помощью CMake

### Linux/Unix

- Требуется библиотека pthreads
- Стандартный интерфейс сокетного программирования

## Лицензия

Проект распространяется под лицензией MIT. Подробности см. в файле COPYING.
