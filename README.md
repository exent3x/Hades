# Russian Roguelite v0.6

Цель этой версии — убрать зависимость `.tscn` от пользовательских C++-типов.

## Главное изменение

Ни одна `.tscn` не содержит:

- `type="PlayerController"`
- `type="TrainingDummy"`
- другие пользовательские GDExtension-типы.

Игрок и враги — обычные Godot `Node2D`-сцены. Их GDExtension-ядро создаётся из GDScript через `ClassDB.instantiate()`.

Поэтому даже если DLL не загрузилась, Godot может разобрать `main.tscn`, `player.tscn`, `training_dummy.tscn` и `leshy_boss.tscn`; вместо parse error появится понятная runtime-ошибка о том, что C++ класс не найден.

## Структура

project/scenes:
- main/main.tscn — только корень игры
- player/player.tscn — маленькая сцена-обёртка
- enemies/*.tscn — маленькие сцены-обёртки
- world/hub.tscn — убежище
- world/rooms/*.tscn — комнаты
- ui/hud.tscn — HUD
- ui/reward_overlay.tscn — выбор награды

project/scripts:
- main.gd — переходы между состояниями
- player.gd — сборка C++-игрока
- enemy.gd — сборка C++-врага
- combat_room.gd — волны
- boss_room.gd — босс
- hud.gd — интерфейс
- reward_overlay.gd — награда

## Первый gameplay loop

Хаб → Чёрный лес → 2 боевые комнаты → награда → Леший → победа → Вечная слава → хаб.

При смерти игрока поход заканчивается, временные улучшения теряются.

## Сборка

В корне проекта:

    scons target=template_debug arch=x86_64

Или:

    rebuild_debug.bat

Открывать в Godot:

    project/project.godot

## Управление

WASD — движение
мышь — направление
ЛКМ — атака/комбо
SPACE — дэш
Q — огненный круг
ENTER — начать поход / вернуться в хаб
U — постоянное улучшение в хабе
