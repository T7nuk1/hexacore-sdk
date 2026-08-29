# Свой проект

## Точка входа — `app_main()`

`main()` писать не нужно, он уже есть в SDK и находится в BSP. Приложение
предоставляет `app_main()`, BSP её вызывает.

```c
void app_main(void)
{
        /* здесь всё ваше */
}
```

Так сделано, чтобы приложение не зависело от режима сборки: под FreeRTOS `main()`
поднимает тактирование, таймер ядра и планировщик, а в bare metal он пустой. При
смене режима код приложения не переписывается — меняется только строка в
`CMakeLists.txt` (см. [03-modes.md](03-modes.md)).

## Заготовка

Шаблоны лежат в `cmake/template/` (bare metal) и `cmake/template-freertos/`
(FreeRTOS). Скопируй нужную папку целиком к себе, открой её в редакторе и
собери — дальше всё как с примерами.

В папке два файла: `CMakeLists.txt` и `main.c` с пустым `app_main()`.
Имя файла с исходником роли не играет — оно передаётся в `sdk_add_executable()`.

Проще всего начинать не с шаблона, а с копии ближайшего примера: там уже есть
рабочий `app_main()`, который можно постепенно переделать под себя.

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SDK_PATH)
        if(DEFINED ENV{HEXACORE_SDK})
                set(SDK_PATH $ENV{HEXACORE_SDK})
        else()
                message(FATAL_ERROR "Установите SDK_PATH или HEXACORE_SDK переменную окружения")
        endif()
endif()

set(CMAKE_TOOLCHAIN_FILE ${SDK_PATH}/cmake/toolchain.cmake)

project(my_app C ASM)

include(${SDK_PATH}/cmake/sdk.cmake)

sdk_add_executable(${PROJECT_NAME} main.c)
```

Путь к SDK подставляет расширение, поэтому блок `if(NOT DEFINED SDK_PATH)`
трогать не нужно — он есть во всех примерах и работает как есть.

Порядок здесь не косметический:

1. `SDK_PATH` вычисляется первым — от него зависит путь к тулчейну.
2. `CMAKE_TOOLCHAIN_FILE` ставится **до** `project()`: после вызова `project()`
   CMake уже определил компилятор, и менять его поздно.
3. `include(sdk.cmake)` — после `project()`: там объявляются цели-библиотеки,
   а для этого нужен уже включённый язык C.
4. `ASM` в списке языков обязателен: в сборку входит `crt0.S`, а под FreeRTOS
   ещё и `portASM.S`.

Для FreeRTOS-проекта добавляется одна строка перед `project()`:

```cmake
set(SDK_FREERTOS ON)
```

## Что делает `sdk_add_executable()`

`cmake/sdk.cmake`, коротко:

- добавляет ваши исходники и `shared/runtime/crt0.S`;
- подключает BSP выбранного режима (`bsp/bsp_main.c` либо `bsp/freertos/bsp_main.c`
  плюс `mik32_freertos_glue.c`);
- линкует `mik32_hal` и `hexa_drivers`, а под FreeRTOS ещё `mik32_freertos`;
- задаёт скрипт линковки `shared/ldscripts/spifi.ld` — образ размещается во
  внешней SPIFI-флеш;
- постбилдом делает `.hex` и `.bin`.

Заголовки драйверов и `hexa_board.h` попадают в проект автоматически:
`hexa_drivers` экспортирует свои include-пути как `PUBLIC`.

Несколько файлов:

```cmake
sdk_add_executable(${PROJECT_NAME} main.c sensors.c ui.c)
```

## Флаги компиляции

Заданы в `cmake/toolchain.cmake` и применяются ко всему, включая ваш код:

```
-march=rv32imc_zicsr_zifencei -mabi=ilp32 -mcmodel=medlow
-Os -ffreestanding -flto -fno-common -fno-strict-aliasing
```

Следствия, о которых стоит помнить:

- **`ilp32` без буквы `f`/`d` в `-march`** — аппаратной плавающей точки нет.
  `float` и `double` компилируются в программную эмуляцию: медленно и жирно.
  Поэтому все драйверы SDK оперируют целыми (см. [06-sensors.md](06-sensors.md)).
- **`-ffreestanding` и `-nostdlib`** — полноценной libc нет. `printf` из stdio
  использовать нельзя, вместо него `xprintf`.
- **`-flto`** — ошибки, связанные с несовпадением объявлений в разных единицах
  трансляции, всплывают на этапе линковки, а не компиляции.

## Типичный скелет приложения

```c
#include "hexa_board.h"
#include "hexa_time.h"
#include "xprintf.h"

void app_main(void)
{
        hexa_board_init();      /* I2C + UART + xprintf */
        hexa_time_init();       /* задержки */

        /* инициализация нужных датчиков */

        while (1) {
                /* работа */
                hexa_delay_ms(1000);
        }
}
```

`hexa_board_init()` поднимает только шину I2C и отладочный UART. Датчики она
не трогает — у каждого свой `init`.

В bare metal перед `hexa_board_init()` нужно ещё поднять тактирование —
об этом [03-modes.md](03-modes.md).

## Задания

### Уровень 1 — повторить

- [ ] Сделать свой проект из копии `examples/blink`, переименовать его в
      `CMakeLists.txt` и собрать.
- [ ] Добавить в `app_main()` вызов `hexa_board_init()` и одну строку
      `xprintf("привет\n")`. Убедиться, что текст виден в мониторе порта.

### Уровень 2 — изменить

- [ ] Вынести мигание светодиодом в отдельный файл `led.c` с заголовком
      `led.h`, добавить файл в `sdk_add_executable()`. Собрать.
- [ ] Убрать `ASM` из строки `project()` и попробовать собрать. Прочитать
      ошибку и объяснить, при чём тут `crt0.S`.
- [ ] Переставить `set(CMAKE_TOOLCHAIN_FILE ...)` **после** `project()`.
      Собрать и объяснить, каким компилятором CMake начал собирать проект.

### Уровень 3 — сделать своё

- [ ] Собрать проект, в котором `app_main()` только печатает значение
      `BOARD_REV_MAJOR` и `BOARD_REV_MINOR` из `hexa_board.h`.
- [ ] Разложить свой проект на три файла (логика, вывод, `main`) так, чтобы
      `app_main()` умещался в десять строк.
