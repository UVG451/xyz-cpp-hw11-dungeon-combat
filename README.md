# ДЗ 11. Бой и журнал Roguelike

Игрок и враги имеют здоровье и броню. Журнал разделяет DEBUG, INFO, WARN и ERROR и пишет в файл и консоль. Обрабатываются ошибки запуска и игры. Доступны стражи, аптечки и три этажа.

## Сборка и запуск

Нужны Visual Studio 2022, CMake и [SFML 2.5.1](https://www.sfml-dev.org/download/sfml/2.5.1/) для Windows x64. Распакуйте SFML, замените путь в первой команде на фактический:

```powershell
cmake -S . -B build -A x64 -DSFML_DIR="C:/SFML-2.5.1/lib/cmake/SFML"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/Release/RoguelikeGame.exe
```

Enter начинает игру. Стрелки или WASD двигают игрока, H применяет аптечку, Space пропускает ход. В [описании игры](gameplay.md) подробнее указаны правила и способы проверки. Для текста используется системный шрифт Consolas.
