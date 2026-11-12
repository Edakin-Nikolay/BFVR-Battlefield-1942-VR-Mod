# План улучшения BFVR (v1.0.4) — рабочий документ

Статус: **актуально на 12.11.2026**. Это главный план работ и нарезки задач для
субагентов. `PROJECT_MIND.md` (корень workspace) остаётся файлом сессионных
чекпоинтов; при расхождении приоритет у этого файла для состава работ и у
`PROJECT_MIND.md` — для текущего статуса сессии.

Связанные документы:
- `PROJECT_MIND.md` — чекпоинты сессий, статус «где остановились».
- `notes/state.md` — сжатое состояние, референсы, окружение.
- `notes/protocol-aim-measurement.md` — протокол замера прицеливания (п.6).
- `test/README.md` — тестовый стенд (вне репозитория, `G:\Strata\workspace\BF1942_vr\test`).
- `docs/ru/` — русская документация нашей ветки.
- `docs/AI_DEVELOPER_HANDOFF.md` — архитектурные инварианты (обязателен к чтению).

---

## 0. Как пользоваться планом

1. Задача = строка с ID (`P0.1`, `P2.3`, …). Один ID = один ограниченный контекст
   для субагента. Не объединять задачи без причины.
2. Статусы: `TODO` → `DOING` → `TEST` (сборка/ctest) → `USER-VERIFY` (шлем) →
   `DONE`. Заблокированные: `BLOCKED(<причина>)`.
3. Каждая задача содержит: цель, зависимости, файлы, шаги, критерий приёмки,
   рекомендуемого агента. Шаблон постановки — приложение C.
4. Правило инкремента: после каждой задачи — сборка + профильные тесты;
   чекпоинт в `PROJECT_MIND.md`; `CHANGELOG.md` только для user-visible.
5. Утверждения о поведении в шлеме делает только пользователь после запуска на
   Quest 3. Компиляция и зелёные тесты — не доказательство (AGENTS.md).

---

## 1. Цели и границы

| № | Цель пользователя | Рабочее направление | Фаза |
|---|---|---|---|
| 1 | Вторая рука полезна (не только «держит цевьё») | двухрукий хват: steering, слоты, радиальная составляющая | P2 |
| 2 | Двухрукое управление не «кривое» | лаг/насыщение aim-доводки, сглаживание, два пивота | P2 |
| 3 | Снайперский прицел — оптика на оружии, не «увеличение экрана» | world-space линза, eye-box, маска | P5 |
| 4 | Ручная перезарядка | жест левой руки → нативный `kLogicalInputReload` | P3 |
| 5 | Голова не выходит за корпус техники | clamp позиции (реализован, ждёт шлема) | P0 |
| 6 | Оружие стреляет туда, куда целишься | замер aim-ray vs fire-ray, кроссчер, доводка | P1 |
| 7 | Понятное прицеливание на обычном оружии (zoom/перекрестие) | нативный zoom + проекционный масштаб + кроссчер | P1/P4 |
| 8 | Подсветка мушки/целика (референс Arizona Sunshine 2) | решение: кроссчер / маркеры BFVR / ассет-пак | P8 |
| 9 | Управление для левшей | input-layer: доминирующая рука, кнопки, haptics | P6 |
| 10 | Русская локализация мода | lookup-строки + PNG-атласы + layout | P7 |
| 11 | Иммерсивный VR-UI (здоровье на предплечье, патроны на магазине) | исследование данных + якоря рендера + MVP | P11 |

### Вне scope (зафиксировать, чтобы не тратить силы)

- **Зеркальная левая рука с оружием** — анимации и view-model BF1942
  авторены под правую руку; полный визуальный паритет требует правки игровых
  ассетов и не входит в BFVR.
- **Локализация нативного HUD/меню BF1942** — шрифты и строки принадлежат игре
  (`Ref2FontGlyphBatch`), мод их не контролирует. Локализуем только UI BFVR.
- **Файлы игры в репозитории** — запрещено AGENTS.md (`.rfa`, текстуры оружия,
  `BF1942.exe` и т.п.). Ассет-паки для п.8 — отдельно, вне репо.
- **Поворот башни головой** — не делаем никогда. Прицел техники остаётся за
  движением контроллера и стиком (`VehicleMotionAimMath`), голова — только камера.
- **Изменение нативной стрельбы/сети** — BF1942 остаётся владельцем выстрела,
  отдачи, перезарядки, снарядов и netcode.

---

## 2. Жёсткие правила (сводка AGENTS.md / handoff)

1. Сохранять работающее поведение, если задача его явно не меняет.
2. Совместимость — только через сигнатуры и fail-closed; никаких хардкодов
   хешей `BF1942.exe`; незнакомый билд не блокируется.
3. Модули ≤ ~2500 строк; новая логика — чистыми helper-политиками с
   детерминированными тестами (noexcept, `[[nodiscard]]` где принято).
4. Диагностика по умолчанию `off`; `normal`/`deep` — только opt-in.
5. Обязательный приёмочный минимум: Win32 build + полный `ctest` + x64 presenter
   build; для presentation/controls/runtime — тест в шлеме.
6. `CHANGELOG.md` — на user-visible; `AI_DEVELOPER_HANDOFF.md` — при изменении
   архитектурного инварианта.
7. Не выпускать релиз/тег/инсталлятор без полного release checklist.
8. Источники истины по убыванию: текущий код и тесты → свежие логи контролируемого
   прогона → handoff/DEVELOPMENT → devREADME → внешний reverse-engineering репо.

---

## 3. Проверенные факты (аудит 12.11.2026)

Только то, что подтверждено чтением текущего кода. Гипотезы помечены отдельно.

### 3.1 Вторая рука / двухрукий хват

- `OffHandSupportPolicy` — state machine `Free → Candidate → Supported`:
  радиус захвата по умолчанию 12 см, удержание 40 мс, после захвата — «held grab»
  (дистанция только для acquisition), отпускание — squeeze-up/потеря трекинга/
  смена binding (`src/stereo/OffHandSupportPolicy.h:18-22`,
  `src/stereo/OffHandSupportPolicy.cpp:280-361`).
- Поза левой руки реконструируется из авторского отношения
  `leftHandFromRightHand` и **не меняет базис оружия**
  (`src/stereo/OffHandSupportPolicy.h:52-62`); левая IK-рука — visual-only
  (`src/client/BFSoldierNativeArmIk.cpp:359`).
- Steering есть, но ограничен: поворот базиса оружия вокруг **неподвижного
  пивота правой руки**, радиальная ошибка игнорируется
  (`src/stereo/OffHandWeaponSteeringMath.h:20-32`); включается только для
  `activeItemIndex == 3` и валидной авторской support-позы
  (`src/client/BFSoldierNativeArmIk.cpp:1297-1379`). Лог подтверждает:
  «Sidearm support remains visual-only» (`:1366`).
- Настройка хвата — только `Hold/Toggle` (`src/settings/UserSettings.h:78-82`).

### 3.2 Прицеливание и выстрел

- Направление выстрела уже принадлежит контроллеру:
  `MakeD3D8ControllerDirectedWeaponFireMatrix` заменяет родительский базис
  `WeaponFire_Core` на позу оружия из OpenXR; игра добавляет barrel-offset,
  spread, темп и снаряд (`src/stereo/WeaponFireAimMath.h:81-90`).
- Нативный aim доводится до «финального функционального базиса оружия» через
  синтетический mouse-look: deadzone 0.0015 рад, `inputPerRadian = 10.67`,
  кламп ±1.0 на ось (`src/stereo/InfantryAuthoritativeAim.h:10-18`,
  `.cpp:127-148`; цель — `src/client/InfantryAuthoritativeAimRuntime.h:37-46`).
- World-crosshair строится из того же fire-базиса (`src/stereo/WorldCrosshairMath.h:53-57`).
- **Но** по умолчанию `handWeaponCrosshair = HitMarkerOnly`
  (`src/settings/UserSettings.h:177-178`) — на стрелковом оружии нет видимой
  прицельной метки, только хит-маркер. Пользователь целится по видимому стволу,
  который рисуется из grip-позы с авторской поправкой и может углово расходиться
  с fire-базисом. Это главный проверяемый кандидат на «стреляет не туда».
- **Прогон 12.11.2026 (кроссчер On):** попадание совпадает с меткой, метка
  помогает. Остались факторы: (1) нативный spread игры, (2) микро-тремор рук
  («прицел моросит»), (3) дёрганье двухрукого хвата при сближении/разведении рук,
  (4) нет стабилизации от второй руки. BFVR уже гасит нативный camera-recoil/shake
  (`BFSoldierVrMotionFilter.h:61-63`) и переносит recoil на оружие
  (`HandWeaponRecoilMath.h`), но spread не трогает — он game-owned.
  Решения: P2.9 (микро-тремор), P2.10 (virtual stock), P2.11 (исследование spread).
- Гипотеза P4 (требует шлема): нативный zoom обычного оружия может теряться при
  замене проекции на OpenXR eye-FOV; масштаб относительного зума реализован
  только для useScope-оружия (`src/stereo/ScopeViewMath.h:189-212`,
  `src/client/ScopeViewOverlay.cpp:345` — «ordinary zoom-only secondary fire
  remains unchanged»).
- **P2.11 (13.11.2026):** spread задаётся в `Objects/HandWeapons/<Weapon>/Objects.con`
  (`setFireDev/setDevMod/setMinDev/setTurnDev/setSpeedDev/setMiscDev`), сервер
  считает попадания сам. Runtime-значение читается в `WeaponFire_Core` как
  `float` по `weapon + 0x1A8`. Подробный отчёт и варианты:
  `docs/ru/spread-research-ru.md`. В `WeaponAimOverlay.cpp` добавлен
  экспериментальный env-режим `BFVR_EXPERIMENTAL_SPREAD_SCALE_PERCENT`
  (0..100, по умолчанию 100 = выкл), restore после вызова.

### 3.3 Снайперский прицел

- Активация — хук `FireArms::setZoom` (0x005391B0), только exact local
  active item с `useScope` (`src/client/ScopeViewOverlay.cpp:246,345`).
- Мир перерисовывается с проекционным масштабом из отношения native FOV/scope FOV
  (`ComputeD3D8ScopeProjectionScale`, `ApplyD3D8ScopeProjectionScale`).
- Камера прицела направляется `controllerGunWorld` со сглаживанием ≤1.5°
  (`src/stereo/ScopeViewMath.h:51-57`).
- Сетка/маска — **два head-locked VIEW-space квада на 1.0 м, заполняющих FOV
  каждого глаза** (`src/openxr/OpenXRScopeOverlayLayer.h:18-29`,
  `kEyeFillingScopeOverlayDistanceMeters`).
- Вывод: жалоба «увеличивает весь экран» структурно верна. Нужна world-space
  линза на позе оружия + eye-box gating (P5).

### 3.4 Перезарядка

- Правая B (secondary) rising edge → `kLogicalInputReload = 24` → нативный
  логический ввод игры (`src/client/ControllerInputOverlay.cpp:65,1456-1461`).
- Жестов нет; политика arm IK перезарядку явно не трогает
  (`src/client/BFSoldierNativeArmIk.cpp:359`).
- Для жеста есть все входы: поза левой руки и gun-basis публикуются; нужен
  слой детекции и pulse в логический input.

### 3.5 Голова в технике (п.5, незакоммичено)

- Реализован clamp позиции головы в осях станции: только строка трансляции,
  ориентация сохраняется; fail-closed к оригинальному трансформу; счётчик
  `mountedHeadClamps`; тесты `MountedCameraMathTests` (+375 строк).
- Объёмы — константы-приближения: танк `{0.45, 0.30, 0.60}`, самолёт
  `{0.30, 0.25, 0.55}`, `Unknown → танк` (класс станции игра не отдаёт).
- **Проверено по коду:** прицел башни идёт от движения правого контроллера
  (`src/stereo/VehicleMotionAimMath.h:10-59`) и стика, а не от presentation-камеры;
  clamp не пишет input/aim/turret. Поворот головы башню не крутит.
- **Прогон 12.11.2026:** clamp позиции работает, но исходная жалоба — не позиция,
  а покрытие кадра. BF1942 рисует только тёмную маску вокруг смотровой щели; при
  повороте головы виден край маски и мир за ней. Причина: в coupled-режиме
  presentation-камера = нативная камера + head delta, а интерьерной геометрии
  на 360° в игре нет. Решение: маска 360° с вырезом по направлению нативной
  камеры (P10.1–P10.3) либо ограничение поворота головы (P10.4, fallback).
- Осталось: тест в шлеме по `test/checklists/step-1.1-headset-checklist.md`,
  зелёный ctest, коммит.

### 3.6 Левши

- Правая рука захардкожена: `hands[kRightControllerHand]` в arm IK
  (`src/client/BFSoldierNativeArmIk.cpp:975-993`), правые кости
  (`src/client/BFSoldierVrArmFoundation.cpp:32-37` — левые имена тоже есть),
  fire/reload/ADS на правых кнопках (`ControllerInputOverlay.cpp`), меню-поинтер,
  haptics.
- Протокол симметричен: обе руки несут `aimPose` и `gripPose`
  (`src/presenter/SharedPresentationProtocol.h:204-205`); OpenXR subaction paths
  симметричны. Переавторинг action sets не нужен.
- Реалистично: input-слой (доминирующая рука + перенос семантики кнопок +
  haptics) — Medium. Визуальный паритет — вне scope (см. §1).

### 3.7 Локализация

- Рантайм-текст: `CreateFontW(L"Segoe UI Semibold", DEFAULT_CHARSET)` +
  `DrawTextW` (`src/client/SettingsMenuArt.cpp:615-643`) → кириллица
  рендерится. `/utf-8` уже добавлен (`src/CMakeLists.txt:150,568,577`,
  незакоммичено).
- Литералов: ~164 в `SettingsMenuArt.cpp`, ~32 в `QuickMenuArt.cpp`, плюс
  `UserConfig.txt`-описания и OpenXR action names (`OpenXRPresentation.cpp:976`).
- Запечённый английский в PNG: `QM_text.png`, `QM_ColumnText.png`,
  `QM_stripText.png`, `BackToGame_text.png`, `SettingsMenu/SettingsText.png`,
  `SettingsMenu/ConLayoutOverlay.png` (легенда контроллера, 381 КБ).
- Фиксированные прямоугольники под английскую длину → риск overflow;
  нужны замер и сокращения.
- Python/Pillow на машине нет (только Store-alias) — для PNG нужен либо
  PowerShell/System.Drawing, либо установка Python по разрешению пользователя.

### 3.8 Подсветка мушек

- В активах мода только `Crosshair.png`/`HitMarker.png` 64×64.
- Мушки/целики — геометрия и текстуры игровых `.rfa`; в репо их класть нельзя.
- Варианты: (а) world-crosshair из fire-базиса (уже есть, выключен по умолчанию);
  (б) маркеры BFVR на позе оружия — нужна пер-оружейная калибровка позиций
  мушек, которой нет; (в) отдельный ассет-пак пользователя (вне репо,
  инструмент `src/tools/RfaInspect.cpp`). Решение — после P1.

### 3.9 Инфраструктура и база

- 37 ctest-целей (приложение A). База прошлой сессии: **35/37**; падают
  `BFVROffHandSupportPolicyTests` и `BFVRUserSettingsTests` (названы
  средовыми — проверить и починить, AGENTS.md требует ноль).
- Незакоммичено: clamp п.5, `/utf-8`, `docs/ru/`, этот план. Ветка `main`,
  HEAD `a1ca388`; `origin` = апстрим (нужен remote форка).
- Стенд: `test/player` (hard links на сборки), `test/game` → `H:\Games\Battlefield 1942`,
  скрипты `test/tools/*.ps1`.

---

## 4. Инфраструктура, сборка, инструменты

### 4.1 Сборка (фактическая на машине)

VS environment:
`F:\programming\VisualStudio\VC\Auxiliary\Build\vcvars32.bat` (x86) /
`vcvars64.bat` (x64).

Канонические каталоги этой машины:
- x86: `build\bfvr-win32-ninja` (Ninja, RelWithDebInfo, compile_commands.json).
- x64 presenter: `build\bfvr-x64-presenter`.

Команды (из корня `repo/`):
```powershell
cmake --build ..\build\bfvr-win32-ninja
ctest --test-dir ..\build\bfvr-win32-ninja --output-on-failure
cmake --build ..\build\bfvr-x64-presenter --target BFVRPresenter
```
Альтернатива из `docs/DEVELOPMENT.md` — NMake-каталоги `build\bfvr-win32`,
`build\bfvr-x64`; не смешивать с Ninja-каталогами.

Стенд:
```powershell
test\tools\Run-TestSuite.ps1                  # сборка + полный ctest, лог в test\logs
test\tools\Run-TestSuite.ps1 -SkipBuild -TestRegex "MountedCameraMath"
test\tools\Stage-BFVRTestBench.ps1 -RefreshOnly
test\tools\Verify-BFVRTestBench.ps1
test\tools\Run-BFVRBench.ps1 [-DryRun]
```

### 4.2 Диагностика

- `BFVR_DIAGNOSTICS=off|normal|deep` (по умолчанию off).
- `BFVR_PERFORMANCE_SUMMARY=1` — точечный perf-режим, не менять каденс
  запросов/`xrEndFrame`.
- Логи: `loader.log`, `BFVRPresenter-game.log`, `observer.log` (deep).

### 4.3 Бинарный анализ (для будущих исследований fire-пути и ассетов)

- `pe_probe` / `pe_imports` — PE-секции, импорты, строки `BF1942.exe`, DLL.
- Ghidra MCP: `G:\tools\ghidra_12.1.4_PUBLIC\ghidraRun-BFVR.bat`,
  сервер `http://127.0.0.1:8089`.
- `src/tools/RfaInspect.cpp` — разбор `.rfa` (для п.8/п.10, вне репо).

### 4.4 Git-протокол

- `origin` сейчас = апстрим `JayBiggsGMG/BFVR-Battlefield-1942-VR-Mod`.
  Нужен remote форка пользователя (URL — от владельца).
- Рабочая ветка: `feature/v1.0.4` (от `a1ca388`); `main` не трогать.
- Коммит после задачи: `<ID>: <краткое описание>`; тело — что проверено
  (build/ctest), что требует шлема.
- Незакоммиченное прошлой сессии входит в первый коммит P0.6 (после P0.4/P0.5).

---

## 5. Делегирование субагентам (строго по описаниям)

| Агент | Модель/окно | Разрешено | Запрещено | Типовые задачи плана |
|---|---|---|---|---|
| `explore` | qwen3:8b, ~24K, слабая | короткий поиск: символ/файл/строка, список `file:line` | большие файлы целиком, длинный анализ | P2.1, P6.1 (find-часть) |
| `llama_pc` | Qwen3-Coder-30B локально, ~24K, 0 токенов подписки | одна функция/файл ≤~300 строк, ревью одного фрагмента, JSON/XML по схеме; вход ≤~8K | весь репозиторий, длинные логи, многофайловые рефакторинги | P3.2 (черновик), P5.2 (фрагмент), проверка одного фрагмента |
| `mod_parser` | qwen3:8b, ≤~500 строк | парсинг небольших конфигов по готовой схеме | большие конфиги | если понадобятся `.con/.tweak` |
| `extractor` | claude-haiku-5-5 | точное извлечение структуры из длинных логов/дампов/исходников; точечные правки по чётким правилам | творческие решения | P1.2, P2.1 (таблица), P6.1, P7.1 |
| `free_bulk` | longcat-2.5-free, 0 $ | массовая рутина: пакетные конвертации, простые преобразования, черновики описаний | архитектура | P7.3 черновики, P7.5 инвентаризация PNG |
| `json_calc` | qwen3.8-flash | конвертация `.con/.tweak→JSON/XML`, пересчёты координат/углов, табличная математика | чтение всего репо | P1.6 расчёты ошибок, P7.4 замеры длин |
| `text_routine` | glm-5.3-flash | описания, changelog, docstrings, комментарии, саммари из notes/docs | код | P1.4 инструкция, P7.3 переводы, P9.2 changelog |
| `quick_patch` | hy3 | мелкие правки кода/скриптов, regex, небольшие утилиты | архитектурные решения | P0.2/P0.3 мелкие фиксы, скрипты замера |
| `general_lite` | gpt-6-luna | форматирование, пакетные преобразования файлов, простые вычисления | тяжёлый код | P7.5 массовая переименовка/подготовка |
| `cache_worker` | deepseek-v4.1-flash, дешёвый кэш чтения | повторяющаяся работа с большим неизменным контекстом: регрессионные дампы, повторные проверки логов, сверка версий | новые архитектурные задачи | P9.1 повторные прогоны, P0.4 архивация логов |
| `general` | сильная модель | сложные многошаговые исследования и правки | — | P5 целиком, P2.5, P6.4–P6.6 |

Правила постановки:
1. Всегда указывать точный путь и диапазон строк; для локальных моделей —
   не более одного файла и ≤~300 строк на вызов.
2. Явно перечислять запреты: не менять поведение вне задачи, не трогать
   `third_party/`, не добавлять файлы игры, не менять протокол без версии.
3. Требовать формат ответа: `file:line` + вывод; для правок — unified diff или
   готовый файл, без «объяснений на полстраницы».
4. Результат субагента проверяет основной агент (локальные модели уже давали
   фабрикации — сверять с кодом).
5. Пустой/таймаут ответа = задача слишком большая: дробить и повторять.

---

## 6. Roadmap и задачи

Порядок (утверждён владельцем): **P0 → P1 → (P2 ∥ P3 ∥ P10) → P4 → P5 → P6 → P7 → P8**.
P6/P7 могут идти параллельно P2–P5 силами отдельного «потока UI».
P10 (кабина техники) выделен после прогона 12.11.2026: clamp позиции работает,
но остаётся проблема покрытия кадра.

### P0. База и гигиена (блокирует всё)

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P0.1 | Настроить git-remote форка и ветку `feature/v1.0.4` | — | `git remote add fork <URL>`; `git switch -c feature/v1.0.4` | `git remote -v` содержит форк; ветка создана | main (URL от пользователя) |
| P0.2 | Диагностика падения `BFVROffHandSupportPolicyTests` | — | лог `test/logs/*`; `src/stereo/OffHandSupportPolicyTests.cpp` | причина зафиксирована (код/среда) в notes | extractor (лог) + main |
| P0.3 | Диагностика падения `BFVRUserSettingsTests` | — | `src/settings/UserSettingsTests.cpp`, temp-каталоги | причина зафиксирована | quick_patch + main |
| P0.4 | Полный прогон Win32 ctest + сборка x64; архив лога | P0.2, P0.3 | `Run-TestSuite.ps1`; `build\bfvr-x64-presenter` | 37/37 зелёные; x64 собран; лог в `test/logs` | cache_worker (прогон/архив) |
| P0.5 | Проверка clamp п.5 в шлеме | P0.4 | чек-лист `test/checklists/step-1.1-headset-checklist.md` | пользователь подтвердил: голова не выходит за корпус; башня не крутится головой; decoupled не сломан | USER-VERIFY |
| P0.6 | Первый коммит: clamp + `/utf-8` + `docs/ru` + план | P0.4, P0.5 | git add/commit | коммит в `feature/v1.0.4`; сообщение по протоколу §4.4 | main |
| P0.7 | Зафиксировать инвариант «голова не управляет башней» в handoff | P0.5 | `docs/AI_DEVELOPER_HANDOFF.md`, раздел mounted | абзац с `VehicleMotionAimMath` и границей presentation | text_routine + main |
| P0.8 | Обновить `PROJECT_MIND.md`/`notes/state.md` под новый план | P0.6 | оба файла | ссылка на этот план, актуальный статус | main |

### P1. Прицеливание: быстрые проверки и замер (п.6, п.7)

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P1.1 | **Quick win:** включить `Hand Weapons` crosshair = `On`, проверить в шлеме | P0.5 | VR Settings → 3D Crosshair; либо `UserConfig.txt` | пользователь описал: стало ли понятнее, ушло ли «стреляет не туда» | USER-VERIFY |
| P1.2 | Инвентаризация полей aim/fire в deep-логе (`WeaponFireProbe`) | — | `src/client/WeaponFireProbe.cpp`, `devREADME.md` (раздел fire) | таблица: поле → смысл → где пишется | extractor |
| P1.3 | Детерминированный тест «muzzle ray vs fire ray» на фикстурах | P1.2 | `src/stereo/WeaponFireAimMathTests.cpp` (+ helper при необходимости) | тест падает на искусственном расхождении >0.05° и проходит на совпадении; pure math | llama_pc черновик + main |
| P1.4 | Инструкция замера на Aberdeen (RU) | P1.2 | новый `docs/ru/aim-measurement-aberdeen.md` | шаги, дистанции 15/50/100 м, таблица выстрелов, куда класть скриншоты | text_routine |
| P1.5 | Проведение замера в шлеме | P1.1, P1.4 | карта Aberdeen, офлайн | 10 выстрелов × 3 дистанции × 2–3 оружия; скриншоты в `screenshots/inbox` | USER-VERIFY |
| P1.6 | Расчёт угловой ошибки по данным замера | P1.5 | таблица выстрелов + логи | таблица ошибок по оружию/дистанции; вывод: систематическая/случайная | json_calc + main |
| P1.7 | Решение по коррекции (visual barrel vs fire basis vs crosshair по умолчанию) | P1.3, P1.6 | этот план + `docs/weapon-pose-design.md` | записанное решение: что меняем, что нет; задачи уходят в P2/P4/P5 | main + user |

### P2. Вторая рука и двухрукий хват (п.1, п.2)

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P2.1 | Таблица активации steering: слоты, типы оружия, условия | P1.7 | `BFSoldierNativeArmIk.cpp:1297-1379`, `OffHandSupportPolicyTests.cpp` | таблица «слот → steering → почему» | extractor |
| P2.2 | Политика расширения на слоты (pure) | P2.1 | новый helper в `src/stereo/` + tests | функция `ShouldSteerOffHand(...)` с fail-closed; тесты на все слоты | llama_pc + main |
| P2.3 | Радиальная составляющая / второй пивот (pure math) | P2.2 | `src/stereo/OffHandWeaponSteeringMath.*`, tests | при тяге/толчке левой руки оружие смещается ограниченно; тесты границ | main |
| P2.4 | Параметры сглаживания/лимитов хвата (pure) | P2.3 | те же + `BFSoldierVrMotionFilter.*` (если нужно) | нет осцилляций на синтетических траекториях; тесты | main |
| P2.5 | Runtime-интеграция в `BFSoldierNativeArmIk` | P2.2–P2.4 | `BFSoldierNativeArmIk.cpp` (точечно) | steered basis по-прежнему питает IK+presentation+WeaponFire_Core; fail-closed сохранён | general |
| P2.6 | Диагностика `normal`: requested/applied/radial error | P2.5 | лог-строка рядом с `:1366` | в `normal` видно градусы и радиальную ошибку; в `off` молчит | quick_patch |
| P2.7 | Тест двух рук в шлеме | P2.5 | чек-лист (создать `test/checklists/two-hand.md`) | пользователь: хват стабилен, нет «резины»; пистолеты не хуже | USER-VERIFY |
| P2.8 | CHANGELOG + handoff | P2.7 | `CHANGELOG.md`, handoff | записи внесены | text_routine |
| P2.9 | Адаптивный фильтр микро-тремора hand weapon (как scope smoothing, но для обычного оружия) | P2.7 | `src/stereo/ScopeViewMath.*` (обобщить) или новый helper; настройка strength | тремор <1.5° гасится, осознанное движение проходит без лага; тесты | main |
| P2.10 | Virtual stock: ориентация оружия из линии двух рук (веса по дистанции, min/max span) | P2.3, P2.9 | `OffHandWeaponSteeringMath.*`, `BFSoldierNativeArmIk.cpp` | нет дёрганья при сближении/разведении рук; вторая рука реально стабилизирует; тесты | general |
| P2.11 | Research: где хранится spread (deviation) и можно ли безопасно уменьшить | P1.7 | `.rfa`/ObjectTemplate, Ghidra, `RfaInspect` | отчёт: вариант (asset tweak / runtime patch / не трогать), риски MP | general |
| P2.12 | SP-замер экспериментального spread-scale (env, 100% vs 0%, 20 м) | P2.11 | `BFVR_EXPERIMENTAL_SPREAD_SCALE_PERCENT`, SP-карта | группа попаданий при 0% ≤ метки; restore подтверждён логом | USER-VERIFY |
| P2.13 | MP-проверка spread-scale на 2 клиентах (dedicated) | P2.12 | 2 клиента, hit-маркеры | решение: можно ли включать в публичную версию | USER-VERIFY + main |
| P2.14 | Sidearm off-hand: приоритет авторской «чашки» пистолета + fallback на tracked | P2.10 | `OffHandSupportPolicy.*`, `BFSoldierNativeArmIk.cpp`, tests | левая рука на рукоятке пистолета, не за её пределами; тесты политики | main + USER-VERIFY |
| P2.15 | Диагностика лага рук за головой при stick-turn (deep): метки времени head/controller/generation | P2.9 | `BFSoldierNativeArmIk.cpp`, `D3D8RenderViewPoseHook.cpp`, `normal/deep` лог | число кадров/мс рассогласования; решение о правке фильтра/сэмплинга | main |

### P10. Техника: обзор из кабины (п.5, вторая итерация)

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P10.1 | Публикация slit-forward/FOV из x86 в presenter | P0.5 | `SharedPresentationProtocol.h` (версия +1), `D3D8RenderViewPoseHook.cpp` | presenter получает направление и угловой размер нативного окна | main |
| P10.2 | Presenter: маска 360° с вырезом по slit-forward (мягкий край) | P10.1 | `OpenXRPresentation.cpp`, новый compositor/shader | при повороте головы вне щели — темнота, без «края маски» | general |
| P10.3 | Тесты маски + шлем | P10.2 | pure math + `test/checklists` | вырез совпадает со щелью на обоих глазах; нет мерцания | main + USER-VERIFY |
| P10.4 | Fallback: ограничение поворота головы в coupled-режиме до окна щели | P0.5 | `MountedCameraMath.*` + tests + настройка | альтернатива P10.2, если маска окажется хуже; pure math | llama_pc + main |
| P10.5 | Docs/CHANGELOG | P10.3/P10.4 | — | обновлено | text_routine |

### P11. Иммерсивный VR-интерфейс (новое, исследование)

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P11.1 | RE: offset HP игрока и патронов активного item (Ghidra + live normal) | — | `BFSoldier`/`BFPlayer`, HudManager `0x0065F1A8`, `PlayerControlObject` | offset подтверждён на живом процессе; fail-closed | general |
| P11.2 | `ForearmPanelRenderer`: D3D8-квад в оба eye-target, атлас цифр, save/restore | P11.1 | `D3D8WorldCrosshairRenderer.cpp` как образец | панель следует за левой кистью, параллакс обоих глаз | general |
| P11.3 | MVP: здоровье на левом предплечье + гейты alive/scope/arms + настройка | P11.2 | `BFSoldierBoneResolver`, `UserSettings` | читается в шлеме, не перекрывает оружие, default off | main + USER-VERIFY |
| P11.4 | MVP: патроны на магазине/оружии | P11.2 | `WeaponPoseRuntimeCache` | счётчик совпадает с HUD | main + USER-VERIFY |
| P11.5 | UX-дизайн: размеры, наклон, когда показывать, левша/правша | P11.3 | `docs/ru/immersive-ui-design.md` | согласованный дизайн до реализации | main + user |
| P11.6 | Перф + шлем + docs/CHANGELOG | P11.3–P11.5 | `BFVR_PERFORMANCE_SUMMARY=1` | нет просадки, читаемо обоими глазами | main + USER-VERIFY |

### P3. Ручная перезарядка (п.4)

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P3.1 | Спека жеста: зона магазина, направление/скорость, тайминги, антидребезг | P1.7 | `docs/weapon-pose-design.md` (раздел) или новый док | согласованная спека с числами и fail-closed | main + user |
| P3.2 | Pure-math `ReloadGesturePolicy` + tests | P3.1 | новые `src/stereo/ReloadGesturePolicy.*` + `...Tests.cpp` | тесты: покой/касание/рывок/ложные срабатывания/потеря трекинга | llama_pc черновик + main |
| P3.3 | Runtime: pulse `kLogicalInputReload` из жеста | P3.2 | `src/client/ControllerInputOverlay.cpp` | жест даёт ровно один pulse; кнопка B продолжает работать; fail-closed | main |
| P3.4 | Настройка enable + `UserConfig` + tests | P3.3 | `src/settings/UserSettings.*`, `UserSettingsTests` | ключ с дефолтом, encode/decode, тест round-trip | quick_patch + main |
| P3.5 | Тест в шлеме | P3.3 | чек-лист | перезарядка жестом срабатывает предсказуемо; не мешает хвату | USER-VERIFY |
| P3.6 | Docs/CHANGELOG | P3.5 | `USER_GUIDE.md`, `CHANGELOG.md` | обновлено | text_routine |

### P4. Приближение на обычном оружии (п.7)

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P4.1 | Проверка в шлеме: работает ли нативный zoom правого гриппа на винтовке/пистолете | P0.5 | `BFVR_DIAGNOSTICS=deep`, лог FOV | пользователь: zoom виден/не виден; лог с числами FOV | USER-VERIFY |
| P4.2 | Если не виден: проекционный масштаб для zoom-only (переиспользовать `ScopeViewMath`) | P4.1 | `D3D8RenderViewPoseHook.cpp`, `ScopeViewMath.*` | относительный zoom сохраняется без head-locked оверлея | general |
| P4.3 | Тесты + шлем | P4.2 | pure tests + чек-лист | масштаб монотонен, возврат к 1.0 без ступеньки | main + USER-VERIFY |
| P4.4 | Docs/CHANGELOG | P4.3 | — | обновлено | text_routine |
| P4.5 | Калибровка линии мушки/целика относительно fire-ray (per-weapon) | P1.7 | замер в шлеме + pure math + настройка смещения | мушка/целик и fire-ray сходятся в точке прицеливания; тесты | main + USER-VERIFY |

### P5. World-space оптика (п.3) — крупнейшая работа

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P5.1 | Дизайн-док: eye-box, линза, маска, gating, fallback, калибровка per-weapon | P1.7 | новый `docs/scope-optics-design.md` | схема слоёв и поз; список инвариантов | main |
| P5.2 | Pure math: поза линзы от gun basis + eye-alignment gate + tests | P5.1 | `src/stereo/ScopeViewMath.*` (расширение) + tests | тесты: ось глаза в eye-box → линза видна; вне — обычный вид; roll | llama_pc фрагмент + main |
| P5.3 | Протокол: weapon-attached scope pose в presenter (версия +1) | P5.1 | `SharedPresentationProtocol.h`, `SharedControlChannel.*`, tests | pointer-free поле; старый/новый продюсер fail-closed; тест канала | main |
| P5.4 | Presenter: quad layer на позе оружия вместо VIEW-space eye-filling; кольцевая маска | P5.2, P5.3 | `OpenXRScopeOverlayLayer.*`, `OpenXRPresentation.cpp` | линза движется с оружием; вне линзы мир без зума | general |
| P5.5 | Gating/fallback и сохранение smoothing | P5.4 | `ScopeViewOverlay.cpp` | нет выравнивания — нативное поведение; нет регрессий | general |
| P5.6 | Desktop mirror на той же позе | P5.4 | `DesktopMirror.cpp` | зеркало совпадает с шлемом | general |
| P5.7 | Тесты слоёв | P5.4 | `OpenXRScopeOverlayLayerTests.cpp` + новые | геометрия/маска/поза покрыты | main |
| P5.8 | Шлем: параллакс, eye relief, оба глаза, переключение оружия | P5.4–P5.6 | чек-лист `test/checklists/scope.md` | пользователь подтвердил критерии | USER-VERIFY |
| P5.9 | Docs/CHANGELOG | P5.8 | — | обновлено | text_routine |

### P6. Левши — input-layer (п.9)

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P6.1 | Инвентаризация хардкодов правой руки (таблица file:line) | — | grep `hands[1]`, `kRightControllerHand`, `kOpenXRHapticHandRight`, `RightHand` | полная таблица по подсистемам | extractor |
| P6.2 | `handedness` в `UserSettings` + round-trip tests | P6.1 | `UserSettings.*`, tests | ключ, дефолт Right, encode/decode | main |
| P6.3 | Pure resolver `DominantHandIndex(handedness)` + tests | P6.2 | новый helper `src/stereo/` или `settings/` | тесты на обе руки; fail-closed на invalid | llama_pc + main |
| P6.4 | Меню-поинтер/QuickMenu/Settings на resolver | P6.3 | `MenuPointerOverlay.cpp`, `OpenXRQuickMenu.cpp`, `SettingsMenuInteraction.cpp` | поинтер и hover/activation используют одну руку | general |
| P6.5 | Семантика кнопок: fire/use/squeeze/reload/prone под доминанту | P6.3 | `ControllerInputOverlay.cpp` | обе раскладки проходят тесты; неизвестное состояние не ломает | general |
| P6.6 | Haptics routing на доминирующую руку | P6.3 | `OpenXRPresentationHaptics.inl`, `OpenXRHaptics.*` | маски учитывают handedness; тест | main |
| P6.7 | Решение по IK: input-only (рекоменд.) vs visual parity | P6.1 | этот план | зафиксировано; visual parity — вне scope | user |
| P6.8 | Зеркальная легенда контроллера (PNG) | P6.1, P7.5 | `SettingsMenu/ConLayoutOverlay.png`, `ConLayoutButton/Hover` | легенда соответствует доминанте | general_lite + main |
| P6.9 | Шлем | P6.4–P6.6 | чек-лист | пользователь прошёл сценарии левой рукой | USER-VERIFY |

### P7. Русская локализация (п.10)

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P7.1 | Инвентаризация строк → CSV/JSON (файл, строка, контекст, длина) | — | `SettingsMenuArt.cpp` (164), `QuickMenuArt.cpp` (32), `UserConfig`, action names | таблица полная, без пропусков | extractor |
| P7.2 | Lookup-инфраструктура + `language` + tests | P7.1 | новые `src/client/UiStrings.*`; `UserSettings.*` | EN/RU выбор, дефолт EN, тесты | main |
| P7.3 | Черновой перевод RU | P7.1 | таблица строк | перевод всех runtime-строк | text_routine + free_bulk |
| P7.4 | GDI-замер длин + сокращения под фиксированные панели | P7.2, P7.3 | тестовый harness или скрипт замера | ни одна строка не overflow в статус-панели | json_calc + main |
| P7.5 | PNG-атласы: инвентаризация запечённого текста и план перерисовки | — | `QM_*Text.png`, `BackToGame_text.png`, `SettingsText.png` | список изображений + способ генерации (System.Drawing/Python) | free_bulk + main |
| P7.6 | Перерисовка PNG (RU) | P7.5 | `assets/` | текст читаем, позиции совпадают | main (инструмент) |
| P7.7 | `UserConfig.txt`: решение EN / bilingual | P7.2 | `UserSettings.cpp` | зафиксировано; комментарии не ломают парсер | main |
| P7.8 | Тест меню в шлеме/на десктопе | P7.2–P7.6 | чек-лист | все панели читаемы, нет обрезки | USER-VERIFY |

### P8. Подсветка мушки/целика (п.8)

| ID | Задача | Зависит | Файлы/действия | Критерий приёмки | Агент |
|---|---|---|---|---|---|
| P8.1 | Research трёх вариантов (кроссчер / маркеры BFVR / ассет-пак) | P1.7 | `devREADME.md` (weapon probe), `RfaInspect`, референсы | таблица «вариант → цена → риск → что видит игрок» | general |
| P8.2 | Если маркеры: источник позиций мушек (mesh/vertex probe, Ghidra) | P8.1 | `D3D8WeaponViewModelProbe`, Ghidra | выполнимое решение с данными, не догадками | general |
| P8.3 | Прототип pure math + tests | P8.2 | новый helper `src/stereo/` | позиции стабильны, fail-closed без калибровки | main |
| P8.4 | Шлем | P8.3 | чек-лист | точки совпадают с мушкой, не «плавают» | USER-VERIFY |
| P8.5 | Docs/CHANGELOG | P8.4 | — | обновлено | text_routine |
| P8.6 | Замена заднего целика там, где его нет (гранатомёт): world-crosshair или маркер BFVR | P1.7, P8.1 | `WorldCrosshairMath.*`, `CrosshairOverlay.cpp` | по гранатомёту можно целиться без нативного целика | main + USER-VERIFY |

### P9. Сквозные задачи (постоянно)

| ID | Задача | Периодичность | Критерий |
|---|---|---|---|
| P9.1 | Регрессия: Win32 ctest + x64 build перед каждым коммитом | каждая задача | 37/37, x64 собрано |
| P9.2 | `CHANGELOG.md` для user-visible | каждая user-visible | запись в `[Unreleased]` |
| P9.3 | Обновление handoff при смене инварианта | по факту | абзац + дата |
| P9.4 | Чекпоинт `PROJECT_MIND.md`/`notes/state.md` | после каждой задачи | статус, файлы, что ждёт шлема |
| P9.5 | Релиз v1.0.4 по release checklist | в конце | инсталлятор + SHA-256 + тест установки |

---

## 7. Матрица проверок: что чем доказывается

| Тип изменения | Детерминированно (ctest) | Сборка x64 | Шлем (Quest 3) |
|---|---|---|---|
| Чистая математика (clamp, steering, gesture, resolver) | да, обязательно | — | только визуальная приемлемость |
| Runtime-маршрутизация ввода/оружия | тесты политик | да | обязательно |
| Слои/презентация (оптика, кроссчер, UI) | геометрия/политики | да | обязательно (параллакс, читаемость) |
| Настройки `UserConfig` | round-trip tests | — | опционально |
| Локализация | lookup-тесты | — | читаемость/overflow |
| Совместимость пакетов/рантаймов | — | да | обязательно, с записью пакета/рантайма |

---

## 8. Риски и открытые вопросы

1. **Fork URL неизвестен** — P0.1 заблокирован до ответа пользователя.
2. **Два падающих теста** — если окажутся реальными багами, а не средой, P0
   расширяется (фикс кода + тесты).
3. **Объёмы clamp — константы**, не геометрия корпусов; возможна донастройка
   после шлема (per-class данные игра не отдаёт).
4. **P4 — гипотеза**: нативный zoom может быть не виден в VR; проверять до
   реализации.
5. **P5 — самая рискованная**: смена протокола (версия +1), десктоп-зеркало,
   маска, eye-box. Дробить строго по задачам, не mergить протокол и презентацию
   в один коммит.
6. **Локальные модели** уже давали фабрикации (например, несуществующий enum
   `TwoHandGripStyle`). Любой ответ сверять с кодом; для `llama_pc`/`explore`
   дробить до ≤1 файла и ≤300 строк.
7. **Python/Pillow отсутствуют** — для PNG-атласов нужен альтернативный
   инструмент или разрешение пользователя на установку.
8. **Шлем доступен ежедневно**, но тесты пользователя — узкое место; задачи
   `USER-VERIFY` группировать пакетами по 2–4 пункта за сессию.
9. **Производительность**: любые новые проходы/слои проверять через
   `BFVR_PERFORMANCE_SUMMARY`, не менять каденс `xrEndFrame`.
10. **Сеть/мультиплеер**: изменения fire-пути не должны менять сетевые пакеты;
    сверять с handoff (owner-confirmed playback и т.п.).

---

## 9. Чекпоинты и журнал

После каждой задачи:
1. `cmake --build build\bfvr-win32-ninja` + `ctest` (или `Run-TestSuite.ps1`).
2. Для presentation/input — x64 build.
3. Запись в `PROJECT_MIND.md` → «Текущий статус для новой сессии»: ID задачи,
   файлы, что проверено, что ждёт шлема.
4. `notes/state.md` — краткое обновление (если менялись факты/окружение).
5. Коммит `feature/v1.0.4` (после P0.1/P0.6).
6. `CHANGELOG.md` — только user-visible.

Формат записи:
```
[DONE] P2.5 — steering расширен на слоты X/Y; ctest 37/37; ждёт шлема (P2.7).
[BLOCKED] P4.2 — нет подтверждения P4.1 (нужен запуск пользователя).
```

---

## 10. Первые действия (день 1)

1. Пользователь: дать URL форка (P0.1).
2. Агент: `extractor` — причины падений двух тестов по логам (P0.2/P0.3),
   затем `quick_patch`/main — фиксы.
3. Агент: полный прогон `Run-TestSuite.ps1` + x64 build (P0.4).
4. Пользователь: шлем-проверка clamp по чек-листу (P0.5) + быстрый тест
   crosshair `On` (P1.1) — можно одной сессией.
5. Агент: первый коммит (P0.6) + обновление handoff/PROJECT_MIND (P0.7/P0.8).
6. Далее — P1.2–P1.7 (замер прицеливания) и параллельно P3.1–P3.2
   (спека/математика жеста перезарядки).

---

## Приложение A. Карта тестов (37 целей, `src/CMakeLists.txt`)

BF42++: `BFVRBf42PlusPlusCompatibilityTests`; Frame limiter:
`BFVRFrameLimiterOverrideTests`; HUD: `BFVRHudToggleTests`; Stereo math:
`BFVRStereoMathTests`; Water: `BFVRWaterReflectionTextureBasisTests`; Shadow:
`BFVRProjectedShadowTextureTransformTests`; WeaponFire:
`BFVRWeaponFireAimMathTests`; Recoil: `BFVRHandWeaponRecoilMathTests`;
Arm/pose: `BFVRBFSoldierAttachmentPosePairerTests`; OffHand:
`BFVROffHandSupportPolicyTests`; Arm IK: `BFVRArmPoleVectorMathTests`,
`BFVRArmVrPoseMathTests`; Menu: `BFVRMainMenuOverlayTests`, `BFVRQuickMenuTests`,
`BFVRSettingsMenuTests`; Settings: `BFVRUserSettingsTests`; Haptics:
`BFVROpenXRHapticsTests`; OpenXR controller:
`BFVROpenXRControllerShortcutTests`, `BFVROpenXRControllerBindingTests`;
Vehicle: `BFVRVehicleMotionAimMathTests`, `BFVRSurfaceVehicleDriveMathTests`,
`BFVRAircraftControlMathTests`; Locomotion: `BFVRDirectionalLocomotionTests`;
Comfort: `BFVRComfortVignetteTests`; Tracking: `BFVRTrackingAnchorTests`;
Infantry: `BFVRInfantryAuthoritativeAimTests`, `BFVRInfantryPresentationTurnTests`,
`BFVRInfantryCameraMathTests`; Controller: `BFVRControllerInputCacheTests`;
Mounted camera: `BFVRMountedCameraMathTests`; Scope: `BFVRScopeViewMathTests`,
`BFVROpenXRScopeOverlayLayerTests`; Crosshair: `BFVRWorldCrosshairMathTests`;
UI pointer: `BFVRUiPointerSmoothingTests`; Runtime diagnostics:
`BFVRRuntimeDiagnosticsTests`; Shared control: `BFVRSharedControlChannelTests`;
KillSound: `BFVRKillSoundEventPolicyTests`.

## Приложение B. Ключевые файлы по подсистемам

- Офф-хенд/хват: `src/stereo/OffHandSupportPolicy.*`,
  `src/stereo/OffHandWeaponSteeringMath.*`, `src/client/BFSoldierOffHand*.*`,
  `src/client/BFSoldierNativeArmIk.cpp`, `src/client/BFSoldierVrArm*.*`.
- Стрельба/прицел: `src/stereo/InfantryAuthoritativeAim.*`,
  `src/stereo/WeaponFireAimMath.*`, `src/client/WeaponFireProbe.*`,
  `src/client/InfantryAuthoritativeAimRuntime.*`, `src/stereo/WorldCrosshairMath.*`,
  `src/client/D3D8WorldCrosshairRenderer.cpp`, `src/client/CrosshairOverlay.cpp`.
- Оптика: `src/client/ScopeViewOverlay.*`, `src/stereo/ScopeViewMath.*`,
  `src/openxr/OpenXRScopeOverlayLayer.*`, `src/client/TrackedScopeAim.*`.
- Ввод: `src/client/ControllerInputOverlay.cpp`, `src/openxr/OpenXRPresentation.*`,
  `src/openxr/OpenXRController*`, `src/presenter/SharedControlChannel.*`,
  `src/presenter/SharedPresentationProtocol.h`.
- Техника: `src/stereo/MountedCameraMath.*`, `src/client/D3D8RenderViewPoseHook.cpp`,
  `src/stereo/VehicleMotionAimMath.*`, `src/client/MountedWeaponAimResolver.*`.
- UI/локализация: `src/client/SettingsMenuArt.*`, `src/client/QuickMenuArt.*`,
  `src/stereo/SettingsMenuInteraction.*`, `src/settings/UserSettings.*`,
  `assets/`.

## Приложение C. Шаблон постановки задачи субагенту

```
[ID] Px.y — <название>
Контекст: репозиторий G:\Strata\workspace\BF1942_vr\repo, мод BFVR (BF1942 VR).
Цель: <одно предложение>.
Файлы (только эти): <путь> (<диапазон строк>).
Ограничения:
- не менять поведение вне цели; fail-closed; AGENTS.md;
- pure math → добавить/обновить детерминированный тест;
- не трогать third_party/, не добавлять файлы игры, протокол — только с версией.
Вход: <что прочитать/что уже известно>.
Выход: unified diff (или готовый файл) + список тестов, которые нужно прогнать.
Проверка: cmake --build build\bfvr-win32-ninja; ctest --test-dir ... -R <regex>.
Лимит: ≤300 строк кода/одного файла (для локальных моделей).
Отчёт: 5–10 строк, только факты и file:line.
```
