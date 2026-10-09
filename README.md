Кроссплатформенный REST API сервер на C++ с поддержкой ролевой модели доступа и контролем сессий.

```text
.                              # Корневая директория проекта
│
├── CMakeLists.txt              # Кроссплатформенный файл сборки CMake
├── main.cpp                    # Точка входа, Middleware и регистрация путей
├── .gitignore                  # Конфигурация исключений для Git-репозитория
│
├── data/                       # Папка статических веб-ресурсов бэкенда
│   ├── index.html              # Главная страница (ТЗ + Публичная зона API)
│   ├── protected.html          # Вторая страница (Интерфейс защищенной зоны)
│   ├── style.css               # Общий адаптивный CSS файл стилей
│   └── users.txt               # Текстовая база аккаунтов (username:password:role)
│
└── src/                        # Исходный код ядра бэкенда
    ├── httplib.h               # Многопоточный HTTP-движок сервера
    ├── models.hpp              # Структуры сессий и моделей данных
    │
    └── routes/                 # Файлы изолированных модулей маршрутизации
        ├── admin_routes.hpp    # Системные логи и автотесты
        ├── auth_routes.hpp     # Управление аккаунтами (профиль, пароли)
        ├── public_routes.hpp   # Счетчики, логин и регистрация гостей
        └── works_routes.hpp    # Математический модуль вычислений и история
```

## Установка

Необходим инструмент автоматизации **CMake (версии 3.15+)** и компилятор C++ с поддержкой стандарта **C++17** (например, GCC, Clang или MinGW/w64devkit под Windows)

Скачать можно здесь: https://cmake.org/download/ или здесь: https://github.com/skeeto/w64devkit/releases

## Пошаговое развертывание и запуск

Все команды выполняются из **корневой директории проекта**

Откройте терминал (PowerShell / Bash) и выполните:
```bash
cmake . ; cmake --build .
```
*Примечание для Windows:* Конфигуратор CMake автоматически обнаружит платформу и прилинкует системную библиотеку сокетов `ws2_32` (Winsock2), необходимую для сетевой работы.

### Запуск
Запустите скомпилированный исполняемый файл из корня проекта:

* **Для Windows:**
  ```powershell
  .\CppRestApiServer.exe
  ```
* **Для Linux / macOS:**
  ```bash
  ./CppRestApiServer
  ```

После запуска в консоли отобразится:
```text
[INIT] Starting...
[RUN]  Server is running successfully at: http://localhost:8080
```


### Примеры запросов для проверки через командную строку (cmd.exe)

Команды адаптированы под `cmd.exe` (кавычки JSON экранированы символом `\"`).

#### Запрос статистики:
```cmd
curl -X GET "http://localhost:8080/api/v1.1/guest/counters"
```

#### Регистрация аккаунта:
```cmd
curl -X POST "http://localhost:8080/api/v1.1/auth/register" -H "Content-Type: application/json" -d "{\"username\": \"newuser\", \"password\": \"securepass789\"}"
```

#### Аутентификация и получение токена:
```cmd
curl -X POST "http://localhost:8080/api/v1.1/auth/login" -H "Content-Type: application/json" -d "{\"username\": \"newuser\", \"password\": \"securepass789\"}"
```

#### Основная обработка:
```cmd
curl -X POST "http://localhost:8080/api/v1.1/analytics/calc" -H "Authorization: Bearer TK12345" -H "Content-Type: application/json" -d "{\"data\": [10.5, \"строка\", 20.0, null, 45.3]}"
```
