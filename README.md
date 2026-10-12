# REST-API сервер на C++17 с SQLite и RBAC

Многопоточный REST-сервер для статистического анализа числовых
последовательностей и решения индивидуальных расчётных задач.

Проект собирается **без внешних зависимостей** (кроме SQLite) и работает
на Windows, Linux и macOS.

---

## Содержание

- [Возможности](#возможности)
- [Структура проекта](#структура-проекта)
- [Быстрый старт](#быстрый-старт)
- [Конфигурация](#конфигурация)
- [API Reference](#api-reference)
  - [Аутентификация](#аутентификация)
  - [Публичные эндпоинты](#публичные-эндпоинты)
  - [Административные эндпоинты](#административные-эндпоинты)
  - [Модуль EXAMPLE — статистика](#модуль-example--статистика)
  - [Модуль INDIVIDUAL — индивидуальное задание](#модуль-individual--индивидуальное-задание)
- [Безопасность](#безопасность)
- [Индивидуальное задание](#индивидуальное-задание)
- [Тестирование](#тестирование)
- [Решение проблем](#решение-проблем)

---

## Возможности

- **Многопоточность** — `httplib` обрабатывает каждый запрос в отдельном
  потоке из пула.
- **Авторизация через Bearer-токены** — с ограниченным временем жизни
  (TTL настраивается).
- **RBAC** — три роли: `user`, `admin`, `owner`. Доступ к защищённым
  эндпоинтам проверяется.
- **Безопасные пароли** — соль + SHA-256. В БД пароли в открытом виде
  не хранятся.
- **SQL-защита** — все запросы параметризованы через `?` (нет
  SQL-инъекций).
- **JSON** — все ответы возвращаются в формате JSON.
- **Единый формат ошибок** — middleware перехватывает исключения и
  стандартизирует ответы.
- **CORS** — браузерные клиенты (fetch/AJAX) работают из коробки.
- **Healthcheck** — `GET /health` для мониторинга.
- **Логирование** — каждый запрос пишется в консоль и в таблицу `logs`.

---

## Структура проекта

```
REST-API/
├── CMakeLists.txt                          Сборка
├── main.cpp                                Точка входа + middleware
├── README.md                               Этот файл
└── src/
    ├── json.hpp                            Сборка JSON-ответов
    ├── crypto.hpp                          SHA-256, пароли, токены
    ├── database.hpp                        Работа с SQLite + конфиг
    │
    ├── routes/
    │   ├── auth.hpp                        8 эндпоинтов авторизации
    │   └── public.hpp                      Счётчики, история, admin-эндпоинты
    │
    ├── modules/
    │   ├── example/
    │   │   └── stats.hpp                   Демо: статистика (не трогать)
    │   └── individual/
    │       ├── task.hpp                    Каркас ИЗ (не трогать)
    │       ├── variant_roi.hpp             Пример: ROMI/ROAS
    │       └── README.md                   Инструкция студенту
    │
    └── lib/
        ├── httplib.h                       Сторонний HTTP-движок
        └── sqlite_loader.hpp               Загрузчик SQLite
```


---

## Быстрый старт

### Windows (w64devkit)

Проект протестирован с портативным GCC-дистрибутивом
[w64devkit](https://github.com/skeeto/w64devkit/releases).

```powershell
# 1. Убедитесь, что sqlite3.dll лежит рядом с .exe, ИЛИ
#    используется встроенный winsqlite3.dll (Windows 10+).

# 2. Сборка
cd C:\wp\cpp\REST-API
cmake .
cmake --build .

# 3. Запуск
.\rest_server.exe
```

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y build-essential cmake sqlite3 libsqlite3-dev

cd REST-API
cmake .
cmake --build .
./rest_server
```

### macOS

```bash
xcode-select --install
brew install cmake sqlite

cd REST-API
cmake .
cmake --build .
./rest_server
```

После запуска в консоли:

```
=============================================
  Server running on http://localhost:8080
  Healthcheck: http://localhost:8080/health
=============================================
```

### Стартовые пользователи

При первом запуске создаются три аккаунта:

| Логин    | Пароль   | Роль    |
|----------|----------|---------|
| `owner1` | `owner1` | owner   |
| `admin1` | `admin1` | admin   |
| `user1`  | `user1`  | user    |

**Смените пароли в продакшене!**

## используйте VS Code с расширением Bruno (коллекция прилагается)

### Тестовый запрос


```bash
# Healthcheck
curl http://localhost:8080/health
# → {"status":"ok"}

# Логин
curl -X POST http://localhost:8080/api/v1.1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"user1","password":"user1"}'
# → {"token":"TK...","role":"user"}

# Сохраните токен в переменную окружения
export TOKEN="TK..."    # Linux/macOS
$env:TOKEN = "TK..."    # PowerShell

# Расчёт статистики (example)
curl -X POST http://localhost:8080/api/v1.1/works/calc \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"data":[1,2,3,4,5]}'
# → {"count":5,"mean":3,"median":3,"mode":null,"variance":2.5,"std_deviation":1.58...}
```

---

## Конфигурация

Все параметры — в одном месте: **`src/database.hpp`**, в самом низу:

```cpp
inline constexpr int TOKEN_TTL_SECONDS = 300;   // 5 минут
inline constexpr int MAX_HISTORY_LIMIT = 20;    // лимит записей
```

| Параметр | Описание |
|----------|----------|
| `TOKEN_TTL_SECONDS` | Время жизни токена в секундах. По умолчанию 300 (5 минут) |
| `MAX_HISTORY_LIMIT` | Максимум записей в одном ответе `/history` |

**Изменение** → пересобрать проект (`cmake --build .`).

---

## API Reference

Базовый URL: `http://localhost:8080/api/v1.1`

### Аутентификация

#### POST `/auth/register`

Регистрация нового пользователя.

**Body:**
```json
{"username": "alice", "password": "secret123"}
```

**Ответ (200):**
```json
{"message": "User registered"}
```

**Ошибки:**
- `400` — пустой username или пароль короче 4 символов.
- `409` — username уже занят.

---

#### POST `/auth/login`

Получение токена доступа.

**Body:**
```json
{"username": "user1", "password": "user1"}
```

**Ответ (200):**
```json
{"token": "TK5e8be4916c3cd235c2dc82693858d4a2", "role": "user"}
```

**Ошибки:**
- `401` — неверные учётные данные.

---

#### GET `/auth/profile`

Информация о текущем пользователе и оставшемся времени жизни токена.

**Headers:** `Authorization: Bearer <TOKEN>`

**Ответ (200):**
```json
{"username": "user1", "role": "user", "ttl_seconds_left": 297}
```

**Ошибки:**
- `401` — токен отсутствует или просрочен.

---

#### POST `/auth/refresh`

Продление срока жизни текущего токена.

**Headers:** `Authorization: Bearer <TOKEN>`

**Ответ (200):**
```json
{"message": "Token refreshed"}
```

---

#### GET `/auth/users` 🔒

Список всех пользователей. **Только `admin` / `owner`.**

**Headers:** `Authorization: Bearer <TOKEN>`

**Ответ (200):**
```json
[
  {"id": 1, "username": "owner1", "role": "owner"},
  {"id": 2, "username": "admin1", "role": "admin"},
  {"id": 3, "username": "user1",  "role": "user"}
]
```

**Ошибки:**
- `401` — нет токена.
- `403` — недостаточно прав.

---

#### PUT `/auth/users/password`

Смена пароля. Себе — любой авторизованный. Другому — только
`admin` / `owner`.

**Headers:** `Authorization: Bearer <TOKEN>`

**Body:**
```json
{"username": "user1", "new_password": "newpass123"}
```

Если `username` пропущен — меняется пароль текущего пользователя.

**Ответ (200):**
```json
{"status": "Password updated"}
```

**Ошибки:**
- `400` — пароль короче 4 символов.
- `403` — попытка сменить чужой пароль без прав.
- `404` — пользователь не найден.

---

#### DELETE `/auth/users` 🔒

Удаление пользователя. **Только `admin` / `owner`.**

**Headers:** `Authorization: Bearer <TOKEN>`

**Body:**
```json
{"username": "user2"}
```

**Ответ (200):**
```json
{"status": "User deleted"}
```

**Ошибки:**
- `400` — пустой username или попытка удалить `owner1`.
- `403` — недостаточно прав.
- `404` — пользователь не найден.

---

#### POST `/auth/logout`

Выход из системы. Удаляет текущую сессию.

**Headers:** `Authorization: Bearer <TOKEN>`

**Ответ (200):**
```json
{"message": "Logged out"}
```

---

### Публичные эндпоинты

#### GET `/counters`

Агрегированные показатели системы.

**Ответ (200):**
```json
{
  "total_accounts": 4,
  "successful_calculations": 15,
  "failed_validations": 2
}
```

---

#### GET `/history`

Общий журнал расчётов с пагинацией.

**Query-параметры:**
- `limit` — максимум записей (не больше `MAX_HISTORY_LIMIT`).
- `offset` — смещение.

**Ответ (200):**
```json
[
  {
    "id": 42,
    "username": "alice",
    "datetime": "2026-10-13 14:30:00",
    "input": {"data":[1,2,3]},
    "result": {"mean":2,"median":2,"variance":1}
  }
]
```

---

### Административные эндпоинты

#### GET `/admin/stats` 🔒

Системный лог. **Только `admin` / `owner`.**

**Ответ (200):**
```json
[
  {"id": 1, "datetime": "2026-10-13 14:00:00", "status": "SUCCESS", "message": "Login: user1"}
]
```

---

#### GET `/admin/run-tests` 🔒

Встроенное самотестирование. Возвращает отчёт из 20 тестов.
**Только `admin` / `owner`.**

**Ответ (200):**
```json
{
  "status": "success",
  "total_tests": 20,
  "passed": 20,
  "failed": 0,
  "test_cases": [
    {"tc_id": 1, "name": "DB handle active", "status": "PASSED"},
    ...
  ]
}
```

---

### Модуль EXAMPLE — статистика

Демонстрационный расчёт. **Не трогать.**

#### POST `/works/calc`

Статистический анализ числовой последовательности.

**Headers:** `Authorization: Bearer <TOKEN>`

**Body:**
```json
{"data": [1, 2, 3, 4, 5]}
```

**Ответ (200):**
```json
{
  "count": 5,
  "mean": 3.0,
  "median": 3.0,
  "mode": null,
  "variance": 2.5,
  "std_deviation": 1.5811388300841898
}
```

**Что считается:**
- `count` — количество элементов.
- `mean` — среднее арифметическое.
- `median` — медиана.
- `mode` — мода (если уникальна, иначе `null`).
- `variance` — выборочная дисперсия.
- `std_deviation` — среднеквадратичное отклонение.

**Ошибки:**
- `400` — нет массива, пустой массив или не-число в элементах.
- `401` — нет токена.

---

#### GET `/works/history`

Личная история расчётов текущего пользователя.

**Query-параметры:** `limit`, `offset`.

**Ответ (200):** массив записей (см. `/history`).

---

### Модуль INDIVIDUAL — индивидуальное задание

Расчёт по вашему варианту. Студент реализует **свой вариант**
(см. раздел [Индивидуальное задание](#индивидуальное-задание)).

#### POST `/works/calc-individual`

**Headers:** `Authorization: Bearer <TOKEN>`

**Body (пример для ROMI/ROAS):**
```json
{
  "marketing_spend": 10000.0,
  "attributed_revenue": 25000.0,
  "agency_fee": 1500.0,
  "production_cost": 500.0,
  "period_days": 30
}
```

**Ответ (200):**
```json
{
  "roas": 2.5,
  "net_marketing_profit": 13000.0,
  "romi_percentage": 108.333
}
```

**Ошибки:**
- `400` — не пройдена валидация. Тело ответа содержит описание:
  ```json
  {"error": "marketing_spend must be >= 1000.0"}
  ```

---

#### GET `/works/history-individual`

Личная история по индивидуальному варианту.

---

## Безопасность

| Механизм | Реализация |
|----------|-----------|
| Хеширование паролей | Соль + SHA-256 (`<salt>$<hash>`) |
| Генерация токенов | `std::mt19937_64` + `std::random_device` |
| SQL-инъекции | Все запросы параметризованы через `?` |
| JSON-инъекции | Все строки проходят через `jesc()` |
| RBAC | Проверка роли в защищённых эндпоинтах |
| TTL токена | Сессии истекают через `TOKEN_TTL_SECONDS` |
| Многопоточность | Мьютекс `db_lock` на всех запросах к БД |
| Размер запроса | Ограничение 1 МБ в `main.cpp` |

**Для продакшена дополнительно:**
- Использовать PBKDF2/bcrypt/argon2 вместо SHA-256.
- Настроить HTTPS (TLS).
- Разрешить CORS не для `*`, а для конкретных доменов.
- Ограничить rate-limit по IP.

---

## Индивидуальное задание

### Что уже готово

- **`src/modules/example/stats.hpp`** — демонстрационный модуль
  статистики. **Не трогать.**
- **`src/modules/individual/task.hpp`** — каркас роута. **Не трогать.**
- **`src/modules/individual/variant_roi.hpp`** — пример (ROMI/ROAS).

### Как включить свой вариант

1. Открой `src/modules/individual/README.md` — там подробная инструкция.
2. Скопируй `variant_roi.hpp` → `variant_mine.hpp`.
3. Отредактируй формулы под свою задачу.
4. В `task.hpp` замени одну строку `#include`:
   ```cpp
   // #include "variant_roi.hpp"
   #include "variant_mine.hpp"
   ```
5. Пересобери: `cmake --build .`.

### Пример проверки

```bash
curl -X POST http://localhost:8080/api/v1.1/works/calc-individual \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"marketing_spend":10000,"attributed_revenue":25000,"agency_fee":1500,"production_cost":500,"period_days":30}'
```

---

## Тестирование

### Встроенные тесты

```bash
# Логин как admin
TOKEN=$(curl -s -X POST http://localhost:8080/api/v1.1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin1","password":"admin1"}' | jq -r .token)

# Запуск тестов
curl http://localhost:8080/api/v1.1/admin/run-tests \
  -H "Authorization: Bearer $TOKEN"
```

### Проверка SQL-инъекций

```bash
curl -X POST http://localhost:8080/api/v1.1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"'"'"' OR 1=1 --","password":"x"}'
# → 401 Invalid credentials (инъекция не проходит)
```

### Проверка RBAC

```bash
# Логин как обычный user
curl -X POST http://localhost:8080/api/v1.1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"user1","password":"user1"}'

# Попытка получить список пользователей — 403
curl http://localhost:8080/api/v1.1/auth/users \
  -H "Authorization: Bearer <USER_TOKEN>"
```

### Postman / Bruno

Файл коллекции (если есть) — `bruno_collection/`. Импортируйте в клиент,
создайте окружение `local`, добавьте переменную `authToken`. В запросе
Login добавьте Post Response Script:

```javascript
const data = res.getBody();
bru.setEnvVar("authToken", data.token, { persist: true });
```

После логина переменная обновится, и все запросы, где используется
`{{authToken}}`, заработают.

---

## Решение проблем

### `sqlite3.dll not found`

**Причина:** Windows не нашла SQLite.
**Решение:**
- Скачайте `sqlite3.dll` с https://www.sqlite.org/download.html
  (раздел **Precompiled Binaries for Windows**).
- Положите рядом с `rest_server.exe`.
- **Или** запустите на Windows 10/11 — подхватится встроенный
  `winsqlite3.dll` из `C:\Windows\System32`.

### `[ERROR] bind: Address already in use`

**Причина:** порт 8080 занят другим процессом.
**Решение:**

```powershell
Get-Process | Where-Object { $_.ProcessName -match "rest_server" }
Stop-Process -Name rest_server -Force
```

Или измените порт в `main.cpp`:

```cpp
svr.listen("0.0.0.0", 8081);
```

### 401 Unauthorized после логина

**Причины:**
1. Токен просрочен (по умолчанию 5 минут).
2. Сервер был перезапущен — таблица `sessions` очищается при старте.
3. В заголовке пропущен `Bearer `.

**Решение:** залогиньтесь заново, скопируйте свежий токен.

### 403 Forbidden на `/auth/users`

**Причина:** у текущего пользователя роль `user`, а не `admin`/`owner`.
**Решение:** залогиньтесь как `admin1` / `admin1`.

### 404 в Bruno при работе с `{{authToken}}`

**Причина:** окружение не выбрано («No Environment»), переменная не
подставляется.
**Решение:** выберите окружение `local` в правом верхнем углу.

### Очистить базу

```powershell
# Остановите сервер, удалите файл, перезапустите
Remove-Item data\rest.db
.\rest_server.exe
```

При следующем запуске схема пересоздастся, стартовые пользователи
появятся снова.

---

## Лицензия

Учебный проект. Сторонние библиотеки:
- [`cpp-httplib`](https://github.com/yhirose/cpp-httplib) — MIT.
- [`SQLite`](https://sqlite.org) — Public Domain.