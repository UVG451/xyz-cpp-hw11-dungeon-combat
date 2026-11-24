#include "World.h"
#include "Engine.h"

#include <SFML/Graphics.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <optional>
#include <queue>
#include <string>

namespace
{
constexpr float TileSize = 28.f;
constexpr float OffsetX = 22.f;
constexpr float OffsetY = 106.f;
constexpr unsigned WindowWidth = 744;
constexpr unsigned WindowHeight = 670;
const sf::Color Background{14, 21, 32};
const sf::Color Ink{232, 241, 250};
const sf::Color Muted{157, 178, 203};
const sf::Color Accent{107, 225, 204};

void label(sf::RenderWindow &window, const sf::Font &font, const std::string &line, unsigned size, float x,
           float y, sf::Color color = Ink)
{
    sf::Text text(line, font, size);
    text.setPosition(x, y);
    text.setFillColor(color);
    window.draw(text);
}

void block(sf::RenderWindow &window, float x, float y, float w, float h, sf::Color color)
{
    sf::RectangleShape shape({w, h});
    shape.setPosition(x, y);
    shape.setFillColor(color);
    window.draw(shape);
}

void tile(sf::RenderWindow &window, dungeon::Cell cell, float size, sf::Color color)
{
    const float inset = (TileSize - size) / 2;
    block(window, OffsetX + cell.x * TileSize + inset, OffsetY + cell.y * TileSize + inset, size, size,
          color);
}

void render(sf::RenderWindow &window, const sf::Font &font, const dungeon::World &world, bool paused)
{
    label(window, font, "LANTERN BELOW", 25, 22, 16, Accent);
    label(window, font, "Floor " + std::to_string(world.floor()) + " / 3", 18, 567, 22);
    label(window, font,
          "HP  " + std::to_string(world.health().current) + " / " + std::to_string(world.health().maximum) +
              "     Armor  " + std::to_string(world.health().armor) + "     Field kits  " +
              std::to_string(world.supplies()),
          17, 22, 54);
    block(window, 22, 85, 268, 5, {38, 57, 74});
    block(window, 22, 85, 268.f * world.health().current / world.health().maximum, 5, Accent);

    for (int y = 0; y < dungeon::Height; ++y)
        for (int x = 0; x < dungeon::Width; ++x)
        {
            const bool floor = world.walkable({x, y});
            tile(window, {x, y}, 26, floor ? sf::Color{29, 44, 59} : sf::Color{45, 62, 84});
        }

    tile(window, world.exit(), 20, world.exitOpen() ? Accent : sf::Color{202, 149, 72});
    for (const auto kit : world.pickups())
        tile(window, kit, 10, {132, 217, 140});
    for (const auto &enemy : world.enemies())
        tile(window, enemy->cell, enemy->guardian ? 20.f : 14.f,
             enemy->guardian ? sf::Color{251, 148, 98}
                             : (enemy->finder.chasing ? sf::Color{236, 112, 132} : sf::Color{163, 117, 166}));
    tile(window, world.player(), 16, {244, 242, 181});

    label(window, font, std::string(world.message()), 17, 22, 600);
    label(window, font, "WASD / arrows  Move & strike     H  Heal     Space  Wait     Esc  Pause", 15, 22,
          631, Muted);

    if (paused || world.outcome() != dungeon::Outcome::Exploring)
    {
        block(window, 121, 261, 502, 142, {14, 21, 32, 245});
        const std::string title =
            paused ? "Expedition paused"
                   : (world.outcome() == dungeon::Outcome::Victory ? "You reached the surface"
                                                                   : "Your lantern went out");
        label(window, font, title, 26, 146, 283, Accent);
        label(window, font,
              paused ? "Esc  Resume   |   R  New expedition   |   Q  Menu"
                     : "R  New expedition   |   Q  Menu",
              16, 146, 343, Muted);
    }
}

bool selfTest()
{
    using namespace dungeon;
    for (unsigned seed = 0; seed < 256; ++seed)
    {
        World world(seed, [](std::string_view) {});
        if (!world.verify() || world.exitOpen() || world.act(Action::Move, {-1, 0}) ||
            world.act(Action::Heal))
            return false;

        // A short simulation exercises combat, pathfinding and floor transitions.
        std::mt19937 random(seed);
        std::uniform_int_distribution<int> choice(0, 3);
        constexpr std::array<Cell, 4> moves{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
        for (int i = 0; i < 250 && world.outcome() == Outcome::Exploring; ++i)
        {
            if (world.health().current < 15 && world.supplies() > 0)
                world.act(Action::Heal);
            world.act(Action::Move, moves[choice(random)]);
            for (const auto &enemy : world.enemies())
                if (!world.walkable(enemy->cell) || enemy->cell == world.player())
                    return false;
        }
    }
    Health health{10, 2};
    if (health.hit(5) != 3 || health.heal(100) != 3 || health.hit(1) != 1)
        return false;

    World world(18, [](std::string_view) {});
    constexpr std::array<Cell, 4> directions{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
    const auto offset = [](Cell cell) { return cell.y * Width + cell.x; };
    while (world.outcome() == Outcome::Exploring && world.turn() < 1500)
    {
        if (world.health().current <= 20 && world.supplies() && world.act(Action::Heal))
            continue;

        // Use the guardian as the target until stairs are unlocked.
        Cell goal = world.exit();
        for (const auto &enemy : world.enemies())
            if (enemy->guardian)
                goal = enemy->cell;
        std::array<std::optional<Cell>, Width * Height> firstStep;
        std::queue<Cell> frontier;
        const Cell start = world.player();
        firstStep[offset(start)] = Cell{0, 0};
        frontier.push(start);
        while (!frontier.empty() && !firstStep[offset(goal)])
        {
            const Cell current = frontier.front();
            frontier.pop();
            for (const Cell direction : directions)
            {
                const Cell next = current + direction;
                if (!world.walkable(next) || firstStep[offset(next)] ||
                    (!world.exitOpen() && next == world.exit() && !(next == goal)))
                    continue;
                firstStep[offset(next)] = current == start ? direction : *firstStep[offset(current)];
                frontier.push(next);
            }
        }
        if (!firstStep[offset(goal)] || !world.act(Action::Move, *firstStep[offset(goal)]))
            return false;
    }
    return world.outcome() == Outcome::Victory && world.floor() == Floors;
}
} // namespace

int main(int argc, char *argv[])
{
    if (argc > 1 && std::string_view(argv[1]) == "--self-test")
    {
        const bool passed = selfTest();
        std::cout << (passed ? "Dungeon self-test passed\n" : "Dungeon self-test failed\n");
        return passed ? 0 : 1;
    }

    Logger log;
    try
    {
        sf::Font font;
        const char *windows = std::getenv("WINDIR");
        const auto fontPath =
            std::filesystem::path(windows ? windows : "C:/Windows") / "Fonts" / "consola.ttf";
        if (!font.loadFromFile(fontPath.string()))
            throw std::runtime_error("Cannot load the Windows Consolas font");

        sf::RenderWindow window(sf::VideoMode(WindowWidth, WindowHeight), "Lantern Below",
                                sf::Style::Titlebar | sf::Style::Close);
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);
        Audio audio(log, {110, 0, 130.81f, 146.83f, 164.81f, 0, 146.83f, 130.81f}, 0.46f, 230.f);
        std::optional<dungeon::World> world;
        bool paused = false;
        const auto start = [&]
        {
            world.reset();
            paused = false;
            try
            {
                world.emplace(std::random_device{}(),
                              [&](std::string_view message) { log.write(LogLevel::Info, "game", message); });
                log.write(LogLevel::Info, "game", "Player, walls and enemies created");
            }
            catch (const std::logic_error &error)
            {
                log.write(LogLevel::Error, "game", error.what());
            }
        };
        log.write(LogLevel::Info, "engine", "Window opened");
        while (window.isOpen())
        {
            sf::Event event{};
            while (window.pollEvent(event))
            {
                if (event.type == sf::Event::Closed)
                    window.close();
                if (event.type != sf::Event::KeyPressed)
                    continue;
                const auto key = event.key.code;
                if (!world)
                {
                    if (key == sf::Keyboard::Enter)
                        start();
                    if (key == sf::Keyboard::Escape || key == sf::Keyboard::Q)
                        window.close();
                    continue;
                }
                if (key == sf::Keyboard::Escape && world->outcome() == dungeon::Outcome::Exploring)
                {
                    paused = !paused;
                    continue;
                }
                if (paused || world->outcome() != dungeon::Outcome::Exploring)
                {
                    if (key == sf::Keyboard::R)
                        start();
                    if (key == sf::Keyboard::Q)
                    {
                        world.reset();
                        paused = false;
                    }
                    continue;
                }
                using dungeon::Action;
                bool acted = false;
                try
                {
                    switch (key)
                    {
                    case sf::Keyboard::W:
                    case sf::Keyboard::Up:
                        acted = world->act(Action::Move, {0, -1});
                        break;
                    case sf::Keyboard::S:
                    case sf::Keyboard::Down:
                        acted = world->act(Action::Move, {0, 1});
                        break;
                    case sf::Keyboard::A:
                    case sf::Keyboard::Left:
                        acted = world->act(Action::Move, {-1, 0});
                        break;
                    case sf::Keyboard::D:
                    case sf::Keyboard::Right:
                        acted = world->act(Action::Move, {1, 0});
                        break;
                    case sf::Keyboard::Space:
                        acted = world->act(Action::Wait);
                        break;
                    case sf::Keyboard::H:
                        acted = world->act(Action::Heal);
                        break;
                    default:
                        break;
                    }
                }
                catch (const std::logic_error &error)
                {
                    log.write(LogLevel::Warning, "game", error.what());
                    world.reset();
                }
                if (acted)
                {
                    log.write(LogLevel::Debug, "game", "Turn " + std::to_string(world->turn()));
                    audio.step();
                }
            }
            if (!window.isOpen())
                break;
            window.clear(Background);
            if (world)
                render(window, font, *world, paused);
            else
            {
                block(window, 37, 178, 670, 294, {27, 43, 60});
                block(window, 37, 178, 670, 4, Accent);
                label(window, font, "LANTERN BELOW", 52, 65, 219, Accent);
                label(window, font, "Three floors. Defeat each guardian to open the stairs.", 19, 69, 321);
                label(window, font, "Enter  Start expedition     Esc  Quit", 17, 69, 400, Muted);
            }
            window.display();
        }
        log.write(LogLevel::Info, "engine", "Window closed");
    }
    catch (const std::exception &error)
    {
        log.write(LogLevel::Error, "engine", error.what());
        return 1;
    }
}
