# PhyreUnpacker — вытаскиваем шрифты из PhyreEngine `.phyre`

Инструмент извлекает из `.phyre` файлов bitmap‑шрифты (`PBitmapFont`) вместе с их атласом (`PTexture2D`) и метаданными. Выводит текстуру, XML/JSON‑метаданные, валидатор и совместимый с редактором Phyre Font Editor `.fgen`.

## Возможности
- Надёжная загрузка кластера: `analyzeCluster()` + `loadCluster()` с namespace‑mapping
- Доступ к пикселям: `PTexture2D::map()` (D3D11 staging), корректная работа на DX11 ресурсах
- Поддержка форматов: L8, A8, LA8, RGBA8/ARGB8 (берётся альфа или max‑канал), DXT1/3/5, BC4 (декод по альфе/цвету)
- Экспорт: `*.ppm`, `*_metadata.xml`, `*_characters.json`, совместимый `*.fgen`
- Валидация: `*_validate.txt` отчёт + `*_overlay.ppm` подсветка проблемных глифов (OOB/empty/sparse)
- Подсказка имени TTF: при наличии оригинального `.fgen` рядом сохраняется `<имя>.ttf.name.txt` со строкой 2 `.fgen`

> Важно: сам `PBitmapFont` не несёт имени исходного TTF. Имя TTF можно взять из соседнего `.fgen` (если он есть) или указать вручную во 2‑й строке экспортируемого `.fgen`.

---

## Как это работает
1. Инициализация Phyre SDK: регистрация `ObjectModel`, `Rendering`, `Serialization`, `PhyreInit()`
2. Инициализация D3D11 `PRenderInterface` на скрытом окне — обеспечивает staging для `map()`
3. Анализ и загрузка кластера: `PBinary::analyzeCluster()` → `PBinary::loadCluster()`
4. Поиск `PBitmapFont` и чтение `PTexture2D::map()`; декод пикселей с учётом формата
5. Экспорт результатов + генерация валидатора и overlay
6. Генерация `.fgen` строго в формате `PFGenParser` (см. ниже)

### Формат .fgen (совместим с Phyre Font Editor)
Строго построчно:
1. `fgen`
2. `path/to/font.ttf` (относительно каталога файла)
3. `DUMMY` (не используется)
4. `Fixed` (packing strategy)
5. `width`
6. `height`
7. `size`
8. `sdf` (0/1)
9. `charScale` (обычно 1)
10. `charPad` (обычно 0)
11. `mipPad` (обычно 0)
12+. Список hex‑кодов символов, по одному на строку (уникальные, отсортированные)

---

## Установка и сборка
### Требования
- Windows 10/11 x64
- Visual Studio 2022
- PhyreEngine SDK 3.26.0.0 (дерево `PhyreEngine` доступно рядом с проектом)
- NVIDIA Cg 3.1 Toolkit (x64): `C:\Program Files (x86)\NVIDIA Corporation\Cg\lib.x64\cg.lib`, `cgGL.lib`
- Windows SDK (для `opengl32.lib`)
- (желательно) переменная окружения `SCE_PHYRE` указывает на корень SDK

### Сборка Phyre зависимостей (один раз)
Инструмент линкуется с Tool‑вариантами Phyre библиотек. Соберите проекты SDK перед сборкой `PhyreUnpacker`:

1) PhyreCore (Tool x64 Debug)
```
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ^
  "PhyreEngine\Core\PhyreCore.sln" /m /v:m /p:Configuration=Debug /p:Platform=x64Tool
```

2) DeveloperExtensions (Tool x64 Debug)
```
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ^
  "PhyreEngine\DeveloperExtensions\PhyreDeveloperExtensions.sln" /m /v:m /p:Configuration=Debug /p:Platform=x64Tool
```
> При наличии конфигураций `ToolDebug|x64` в IDE — используйте их.

### Сборка через IDE
1. Откройте `PhyreUnpacker.sln`
2. Конфигурация: `Debug | x64`
3. Построить — бинарь: `PhyreUnpacker\x64\Debug\PhyreUnpacker.exe`

### Сборка из CLI
```
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ^
  PhyreUnpacker\PhyreUnpacker.sln /m /v:m /p:Configuration=Debug /p:Platform=x64
```

### Зависимости линковки (для справки)
- Additional Include Directories: `PhyreEngine\Include`
- Additional Library Directories: `PhyreEngine\Lib\Win64`, `C:\\Program Files (x86)\\NVIDIA Corporation\\Cg\\lib.x64`
- Additional Dependencies: `PhyreCoreToolD.lib; PhyreDeveloperExtensionsToolD.lib; luaD.lib; cg.lib; cgGL.lib; opengl32.lib`

> Если Cg установлен в иное место, скорректируйте путь библиотек (`Linker → Additional Library Directories`) для `cg.lib`, `cgGL.lib`.

### Альтернативная установка Cg (кастомные пути)
Если Cg установлен не по умолчанию:
- Добавьте заголовки Cg: `C/C++ → General → Additional Include Directories` → `C:\path\to\Cg\include`
- Добавьте библиотеки Cg: `Linker → General → Additional Library Directories` → `C:\path\to\Cg\lib.x64`
- Укажите зависимости: `Linker → Input → Additional Dependencies` → `cg.lib; cgGL.lib`
- (Опционально, для рантайма) Добавьте `C:\path\to\Cg\bin.x64` в `PATH`, если приложениям нужны `cg.dll/cgGL.dll` в рантайме

### Где менять в Visual Studio (Property Pages)
Для проекта `PhyreUnpacker` под `Configuration: Debug`, `Platform: x64`:
- C/C++ → General → Additional Include Directories:
  - `$(ProjectDir)..\PhyreEngine\Include`
  - `C:\Program Files (x86)\NVIDIA Corporation\Cg\include` (или ваш путь)
- Linker → General → Additional Library Directories:
  - `$(ProjectDir)..\PhyreEngine\Lib\Win64`
  - `C:\Program Files (x86)\NVIDIA Corporation\Cg\lib.x64` (или ваш путь)
- Linker → Input → Additional Dependencies:
  - `PhyreCoreToolD.lib; PhyreDeveloperExtensionsToolD.lib; luaD.lib; cg.lib; cgGL.lib; opengl32.lib`
> При сборке `Release` используйте соответствующие `Tool`‑варианты библиотек (`PhyreCoreTool.lib` и т.п.).

---

## Использование
```
PhyreUnpacker.exe -i "path\to\neuropol.fgen.phyre" -o "out" --verbose
```
Результат в `out/`:
- `*_texture.ppm` — атлас шрифта (L8 → RGB PPM)
- `*_metadata.xml` — размеры, SDF, lineSpacing, baseline
- `*_characters.json` — глифы/UV/offset/advance/кернинг
- `*.fgen` — совместимый файл, открывается в Phyre Font Editor
- `*_validate.txt` — отчёт UV ↔ пиксели, игнорирует нулевые глифы (space/nbsp)
- `*_overlay.ppm` — подсветка проблемных прямоугольников (фиолетовый — OOB, красный — пусто, жёлтый — sparse)
- (если найден внешний `.fgen`) копия `.fgen` и `<name>.ttf.name.txt` — строка 2 `.fgen` (путь TTF)

---

## Диагностика
- Чёрная текстура: убедитесь, что инициализируется D3D11 `PRenderInterface` (инструмент делает это автоматически). Проверяйте формат (DXT/BC)
- Font Editor пишет "invalid font": проверьте 11 первых строк `.fgen` и корректность TTF в строке 2
- Несовпадение UV: смотрите `*_validate.txt`/`*_overlay.ppm`

---

## Дорожная карта (как мы пришли к успеху)
1. DX11 `.phyre` не грузился напрямую → переключились на `analyzeCluster + loadCluster` с namespace mapping
2. `map()` возвращал `null` → инициализация D3D11 `PRenderInterface` на скрытом окне (staging CopyResource)
3. Реализовали декод L8/A8/LA8/RGBA8/ARGB8 + DXT1/3/5/BC4
4. Сгенерировали совместимый `.fgen` строго по `PFGenParser`
5. Добавили валидатор UV и overlay, игнор нулевых глифов
6. Добавили подсказку имени TTF из оригинального `.fgen` (если присутствует)

---

## Лицензия и использование
Инструмент зависит от PhyreEngine SDK. Используйте соблюдая лицензионные условия SDK и ресурсов игр.

**PhyreUnpacker v1.0** — извлекаем шрифты из `.phyre` корректно и с валидацией.