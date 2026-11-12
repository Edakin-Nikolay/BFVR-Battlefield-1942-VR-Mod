# Иммерсивный VR-UI: исследование (P11)

Статус: **research выполнен 13.11.2026**, read-only. Ни код игры, ни репозиторий
не изменялись. Источник: отчёт фонового субагента + сверка с handoff.

Идея: здоровье — на предплечье свободной руки, патроны — на магазине/оружии,
вместо плоского HUD. Ниже — что реально доступно и чего не хватает.

## 1. Источники данных (подтверждено)

| Что | Где читается | Статус |
|---|---|---|
| Локальный игрок | `MountedWeaponAimResolver.cpp:18,23` (PlayerManager `0x0057D76C` +`0x54`) | есть |
| isAlive | `MountedWeaponAimResolver.cpp:26` (`BFPlayer+0xA9`) | есть, уже в presenter |
| Control object / armed | `MountedWeaponAimResolver.cpp:139-198` (`queryInterface(0xC4C5)` → weapons vtable+0x58) | есть |
| Активный слот | `CrosshairOverlay.cpp:41` (`BFSoldier+0x3E8`) | есть |
| Таймер hit-indication | `CrosshairOverlay.cpp:40` (`BFPlayer+0x1CC`) | есть (прецедент) |
| Левая кисть | `BFSoldierNativeArmIk.cpp:95` (`Bip01 L Hand`), матрица `:833-850` | есть |
| Поза оружия + hand-anchor | `WeaponPoseRuntimeCache.h:38-48` | есть |
| HUD | Ref2 — **пиксели** (`SharedPresentationProtocol.h:64-69`, `D3D8StereoPairProbe.cpp:2946`) | значения не читаются |
| HudManager | глобал `0x0065F1A8`, члены `+0x3C/+0x50` (`ControllerInputOverlay.cpp:514-539`) | структура частично известна |

**Нет:** HP игрока, патронов в магазине/резерве. Ни в коде, ни в `devREADME.md`,
ни в `docs/*` нет offset/геттера. HUD даёт только кадр, не значения.
`game.useHUD` переключает весь HUD целиком (`BF1942HudToggle.cpp:579-748`).

## 2. Варианты рендера

| Вариант | Сложность | Латентность | Левша | Риски |
|---|---|---|---|---|
| A. D3D8-квад в оба eye-target (x86), как crosshair | низкая | 0 кадров | да (свободная кисть) | save/restore state; параллакс через projected billboard |
| B. Новый OpenXR quad в presenter | средняя | +1 request (~11 мс @90Hz) | да | 8 слоёв уже заняты (`OpenXRPresentation.cpp:1858`); world→LOCAL поза |
| C. Кроп Ref2 (`subImage.imageRect`) | низкая | 0 | да | своих цифр нет — только если BFVR сам допишет область |
| D. 4-я shared texture (protocol v24) | высокая | +1 request | да | ломает Oasis-инвариант и `kTextureCount==3` |
| E. Bone-attached quad в нативном HUD | высокая | 0 | нет | правка игры — запрещено |

Самый дешёвый якорь — **A**: левая кисть уже резолвится, матрица мира доступна
каждый кадр, рисование в eye-targets даёт параллакс бесплатно, протокол не
меняется. B — только если панель должна быть поверх HUD-слоя.

## 3. Протокол

`SharedPresentationProtocol.h:14` — `kProtocolVersion = 23`, handshake строгий
(`SharedControlChannel.cpp:236-237`). Если понадобится v24:
`LONG localPlayerHealth`, `LONG weaponAmmoInClip`, `LONG weaponAmmoReserve`,
`volatile LONG forearmPanelValid`, `SharedPresentationPose forearmPanelPose`,
флаг handedness/visible. Слот `frameUiWorldAnchor` (`:278-279`) уже занят
dual-use (menu / scope-roll) — переиспользовать нельзя.

## 4. Ограничения UX/перф

- Порядок слоёв: projection → vignette (`OpenXRPresentation.cpp:1919`) →
  scope → Ref2 UI → QuickMenu (`handoff:184-186`).
- UI-политики различают full-replacement кадры и накопительные HUD-отрисовки
  (`handoff:276-278`) — панель не должна попадать в Ref2.
- Diagnostics off по умолчанию; панель не должна писать логи каждый кадр.
- Ref2 идёт `EYE_VISIBILITY_BOTH` → параллакса нет; «на теле» обязан быть
  world-space.
- Левши: настройки handedness нет; «свободная» рука = левая.
- Записи в игру запрещены; `show_arms=no_hands_or_arms` не отключает скелет,
  панель продолжит работать — нужно решение (скрывать или оставлять).

## 5. MVP и путь

MVP: HP числом + полоска на **левом предплечье**, ~8×4 см, наклон к глазу,
видна только при `localPlayerIsAlive` и вне scope; HP обновлять по изменению
(поллинг 100–250 мс), панель — каждый кадр. Патроны — отдельным шагом, на
магазине оружия (поза из `WeaponPoseRuntimeCache`), после подтверждённого
чтения ammo.

1. **RE-шаг (Ghidra):** найти `BFSoldier`/`BFPlayer` HP и счётчик патронов
   активного item; подтвердить offset на живом процессе через
   `BFVR_DIAGNOSTICS=normal`; fail-closed при несовпадении сигнатур.
2. **Рендер-шаг:** `ForearmPanelRenderer` по образцу
   `D3D8WorldCrosshairRenderer` — проекция 4 мировых углов позы кисти в каждый
   eye, атлас цифр (GDI `DrawTextW` → texture, как `SettingsMenuArt.cpp:615-675`),
   полный save/restore.
3. **Якорь+гейт:** резолв `Bip01 L Forearm` через `BFSoldierBoneResolver`,
   поза = bone + смещение; гейты alive/scope/arms; настройка enable (default off)
   + `UserConfig` round-trip + pure-math тест проекции; mirror получает панель
   автоматически.

## 6. Неизвестно (Ghidra / шлем)

- Offset HP и ammo: не найдены; кандидаты — `BFSoldier`/`BFPlayer`,
  HudManager ammo-компонент (`0x0065F1A8` + члены), геттеры PlayerControlObject.
- Есть ли `Bip01 L Forearm` в скелете BF1942 (сейчас резолвится только Hand).
- Читаемость 8×4 см, дрожание при повороте кисти, перекрытие оружием — шлем.
- Нужно ли гасить части плоского HUD: без RE нельзя (только весь HUD), MVP
  дублирует, не заменяет.
- Поведение при `show_arms=hands_only/no_hands_or_arms`.
- Бюджет кадра: +1–2 draw на глаз; замерить `BFVR_PERFORMANCE_SUMMARY=1`.
