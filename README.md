# PhyreUnpacker — извлекаем и переупаковываем шрифты из PhyreEngine `.phyre`

Инструмент извлекает из `.phyre` файлов bitmap‑шрифты (`PBitmapFont`) вместе с их атласом (`PTexture2D`) и метаданными, а также **переупаковывает** их с новым TTF шрифтом, заменяя отдельные глифы при сохранении оригинальной структуры.

## Возможности

### Извлечение (Extraction)
- Надёжная загрузка кластера: `analyzeCluster()` + `loadCluster()` с namespace‑mapping
- Доступ к пикселям: `PTexture2D::map()` (D3D11 staging), корректная работа на DX11 ресурсах
- Поддержка форматов: L8, A8, LA8, RGBA8/ARGB8 (берётся альфа или max‑канал), DXT1/3/5, BC4 (декод по альфе/цвету)
- Экспорт: `*.ppm`, `*_metadata.xml`, `*_characters.json`, совместимый `*.fgen`
- Валидация: `*_validate.txt` отчёт + `*_overlay.ppm` подсветка проблемных глифов (OOB/empty/sparse)
- Подсказка имени TTF: при наличии оригинального `.fgen` рядом сохраняется `<имя>.ttf.name.txt` со строкой 2 `.fgen`

### Переупаковка (Repacking)
- **Прямой рендеринг через FreeType API**: использование `PhyreFont API` и `FT_Load_Char` для генерации глифов из TTF
- **Выборочная замена**: заменяет только указанные в `.fgen` символы, остальные остаются оригинальными
- **Поддержка rotated glyphs**: корректная обработка повернутых глифов с правильным переупорядком X/Y координат
- **Билинейная интерполяция**: масштабирование глифов от размера TTF к целевому размеру в атласе
- **Автоматическое пропускание пустых глифов**: проверка на пустой bitmap и сохранение оригинального глифа
- **D3D11 platform ID**: сохраняет файл с корректным `D3D11` platform ID для совместимости с игрой
- **Сохранение структуры**: сохраняет оригинальные UV координаты, размеры, метаданные шрифта

### Архивы и форматы
- **OFS3 распаковка**: поддержка извлечения файлов из OFS3 архивов (`.l2b` файлы)
- **L2B архивы**: распаковка Level 2 Binary файлов с таблицей содержимого
- **Анализ заголовков**: определение типов файлов, платформ и форматов данных
- **Поддержка RYHPT**: распознавание текстуры-содержимых кластеров с заголовком RYHPT

> Важно: сам `PBitmapFont` не несёт имени исходного TTF. Имя TTF можно взять из соседнего `.fgen` (если он есть) или указать вручную во 2‑й строке экспортируемого `.fgen`.

---

## Как это работает

### Извлечение (Extraction)
1. Инициализация Phyre SDK: регистрация `ObjectModel`, `Rendering`, `Serialization`, `PhyreInit()`
2. Инициализация D3D11 `PRenderInterface` на скрытом окне — обеспечивает staging для `map()`
3. Анализ и загрузка кластера: `PBinary::analyzeCluster()` → `PBinary::loadCluster()`
4. Поиск `PBitmapFont` и чтение `PTexture2D::map()`; декод пикселей с учётом формата
5. Экспорт результатов + генерация валидатора и overlay
6. Генерация `.fgen` строго в формате `PFGenParser` (см. ниже)

### Переупаковка (Repacking)
1. **Загрузка исходного кластера**: анализ и загрузка оригинального `.phyre` файла с шрифтом
2. **Парсинг `.fgen` файла**: извлечение списка символов для замены и пути к новому TTF
3. **Прямой рендеринг через FreeType**:
   - Загрузка TTF через `PhyreFontLoadFont` и `PhyreFontAllocateFont`
   - Установка размера шрифта через `FT_Set_Pixel_Sizes` (исправляет переопределение PhyreFont 40→28)
   - Рендеринг каждого глифа через `FT_Load_Char` с `FT_LOAD_RENDER`
4. **Выборочная замена глифов**:
   - Очистка области оригинального глифа в текстуре
   - Рендеринг нового глифа в буфер с центрированием
   - Билинейная интерполяция для масштабирования от TTF к размеру атласа
   - Корректная обработка rotated glyphs с переупорядочиванием X/Y
5. **Проверка пустых глифов**:
   - Пропуск символов с пустым bitmap (отсутствуют в TTF)
   - Сохранение оригинального глифа для пропущенных символов
6. **Сохранение с D3D11 platform ID**:
   - Использование `PInstanceListConvert` для явного указания D3D11
   - Применение изменений в Raw image block для Generic текстур
   - Корректная запись platform-специфичных данных для игры

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
- PhyreEngine SDK 3.26.0.0 (https://drive.google.com/file/d/1R1GeQszL6dMCHjk2yWSP9xvF9dcCFqDb/view?usp=sharing)
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

### Извлечение шрифта
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

### Переупаковка шрифта с новым TTF
```
PhyreUnpacker.exe --repack-phyre --repack-source "font00.fgen.phyre" --repack-out "out47\font00.fgen.phyre" --repack-fgen "nwe90.fgen" --repack-only-listed
```

**Параметры переупаковки:**
- `--repack-source <src.phyre>` — исходный `.phyre` файл со шрифтом
- `--repack-out <out.phyre>` — путь для сохранения переупакованного файла
- `--repack-fgen <file.fgen>` — `.fgen` файл с новым TTF шрифтом и списком символов для замены
- `--repack-only-listed` — заменять только символы из `.fgen`, остальные оставить оригинальными
- `--repack-keep-atlas` — сохранить оригинальную структуру атласа (по умолчанию)
- `--repack-ttf-name <TTF.ttf>` — переопределить имя TTF в заголовке `.fgen`

**Результат:** новый `.phyre` файл с заменёнными глифами из TTF, готовый к использованию в игре.

### Работа с архивами
```
PhyreUnpacker.exe --extract-ofs3 level.l2b -o ./output --verbose
```
Извлекает файлы из OFS3 архива в указанную директорию.

```
PhyreUnpacker.exe --analyze file.phyre
```
Анализирует заголовок файла для определения типа, платформы и формата.

**Результат:** распакованные файлы в указанной директории.

---

## Диагностика
- Чёрная текстура: убедитесь, что инициализируется D3D11 `PRenderInterface` (инструмент делает это автоматически). Проверяйте формат (DXT/BC)
- Font Editor пишет "invalid font": проверьте 11 первых строк `.fgen` и корректность TTF в строке 2
- Несовпадение UV: смотрите `*_validate.txt`/`*_overlay.ppm`

---

## Дорожная карта (как мы пришли к успеху)

### Извлечение (Extraction)
1. DX11 `.phyre` не грузился напрямую → переключились на `analyzeCluster + loadCluster` с namespace mapping
2. `map()` возвращал `null` → инициализация D3D11 `PRenderInterface` на скрытом окне (staging CopyResource)
3. Реализовали декод L8/A8/LA8/RGBA8/ARGB8 + DXT1/3/5/BC4
4. Сгенерировали совместимый `.fgen` строго по `PFGenParser`
5. Добавили валидатор UV и overlay, игнор нулевых глифов
6. Добавили подсказку имени TTF из оригинального `.fgen` (если присутствует)

### Переупаковка (Repacking)
1. **Проблема с platform ID**: переупакованные файлы сохранялись с Generic вместо D3D11 → пустой текст в игре
   - **Решение**: явная регистрация D3D11 converter и использование `PInstanceListConvert` при `saveCluster`
2. **Неправильные глифы**: после D3D11 текст отображался, но глифы были перепутаны (например, 'A' вместо 't')
   - **Решение**: исправлен размер шрифта — `PhyreFontLoadFont` устанавливал 40, нужно 28 через `FT_Set_Pixel_Sizes` один раз ДО цикла
3. **Пустые глифы из TTF**: FreeType возвращал пустой bitmap для отсутствующих символов
   - **Решение**: добавлена проверка `bitmap.width/rows/buffer` и подсчёт ненулевых пикселей с порогом для "мусорных" глифов
4. **Rotated glyphs**: для повернутых глифов нужно корректно менять X/Y при записи
   - **Решение**: использована логика из `PhyreFontLibrary.cpp:665` — `pt = buffer + width * (xx + y) + (x + yy)` для rotated
5. **Билинейная интерполяция**: масштабирование от размера TTF к целевому размеру в атласе
   - **Решение**: вычисление нормализованных координат, выборка 4 пикселей, весовые коэффициенты
6. **Позиционирование bitmap**: центрирование глифа в буфере как в PhyreEngine
   - **Решение**: вычисление смещений `tXOff/tYOff` для центрирования bitmap в буфере перед масштабированием
7. **Generic текстуры**: изменения из `workingTexture` не применялись к Raw image block
   - **Решение**: явное копирование `workingTexture` в `rawBlock->m_data` для Generic текстур

---

## Технические детали переупаковки

### Ключевые компоненты `PhyreFontRepacker.cpp`

**Структура `applyGeneratedTextureToCluster`:**
- Чтение исходной текстуры через `PTexture2D::map()` с поддержкой stride
- Создание `workingTexture` для модификаций
- Итерация по символам из `.fgen` для выборочной замены

**Рендеринг через FreeType:**
- `PhyreFontAllocateFont()` → `PhyreFontLoadFont()` для загрузки TTF
- `FT_Set_Pixel_Sizes(face, 0, fontSize)` ОДИН РАЗ перед циклом символов
- `FT_Load_Char(face, charCode, FT_LOAD_RENDER)` для каждого глифа
- Проверка на пустой bitmap: `width/rows/buffer` и подсчёт ненулевых пикселей

**Обработка rotated glyphs:**
```cpp
// Очистка (строки 462-469)
for (yy = 0; yy < srcH; yy++)
    for (xx = 0; xx < srcW; xx++)
        offset = (srcY + xx) * textureWidth + (srcX + yy)

// Запись (строка 640)
targetOffset = (srcY + px) * textureWidth + (srcX + py)
```

**Билинейная интерполяция:**
- Нормализация координат: `fx = px / srcW`, `fy = py / srcH`
- Выборка 4 пикселей: `offset = (y0 + tYOff) * bufferW + (x0 + tXOff)`
- Весовые коэффициенты: `wx = srcCoordX - x0`, `wy = srcCoordY - y0`
- Формула: `gray = g00*(1-wx)*(1-wy) + g01*wx*(1-wy) + g10*(1-wx)*wy + g11*wx*wy`

**Сохранение с D3D11:**
- Регистрация: `Phyre::PPlatform::BindPlatformConverterD3D11(platformId)` через `PhyreInit`
- Получение layout: `Find("D3D11")->getClassLayout()`
- Получение converter: `Find("D3D11")->getInstanceListConvert()`
- Вызов: `saveCluster(cluster, writer, d3d11ClassLayout, d3d11Convert)`

---

## Документация форматов

Дополнительная документация по форматам файлов доступна в директории `PhyreUnpacker/docs/`:

- **[OFS3_FORMAT.md](PhyreUnpacker/docs/OFS3_FORMAT.md)** — детальное описание формата OFS3 архивов
  - Структура заголовка и таблицы файлов
  - Алгоритм извлечения данных
  - Особенности определения размеров файлов
  - Примеры использования

- **[L2B_FORMAT.md](PhyreUnpacker/docs/L2B_FORMAT.md)** — описание формата L2B (Level 2 Binary) файлов
  - Содержимое архивов уровня
  - Типы данных в извлечённых файлах
  - Связь с PhyreEngine уровнями
  - Рекомендации по анализу данных

---

## Лицензия и использование
Инструмент зависит от PhyreEngine SDK. Используйте соблюдая лицензионные условия SDK и ресурсов игр.

**PhyreUnpacker v2.0** — извлекаем и переупаковываем шрифты из `.phyre` с выборочной заменой глифов через FreeType API.
