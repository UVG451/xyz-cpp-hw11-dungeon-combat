#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

namespace dungeon
{
constexpr int Width = 25;
constexpr int Height = 17;
constexpr int Floors = 3;

struct Cell
{
    int x;
    int y;
    bool operator==(Cell other) const
    {
        return x == other.x && y == other.y;
    }
    Cell operator+(Cell other) const
    {
        return {x + other.x, y + other.y};
    }
};

struct Health
{
    int maximum;
    int armor;
    int current = maximum;

    int hit(int power);
    int heal(int amount);
};

struct Character
{
    Cell cell;
    Health health;
};

struct Finder
{
    int range;
    bool chasing = false;
    bool detects(int distance) const
    {
        return distance >= 0 && distance <= range;
    }
};

struct Enemy : Character
{
    Enemy(Cell cell, Health health, bool guardian, int attackPower, int range)
        : Character{cell, health}, guardian(guardian), attackPower(attackPower), finder{range}
    {
    }
    virtual ~Enemy() = default;

    const bool guardian;
    const int attackPower;
    Finder finder;
};

struct Creeper final : Enemy
{
    Creeper(Cell cell, int floor) : Enemy(cell, {4 + floor, 0}, false, 2, 8)
    {
    }
};

enum class EnemyType
{
    Creeper,
    Guardian
};
using Enemies = std::vector<std::unique_ptr<Enemy>>;

class EnemySpawner
{
  public:
    static void spawn(Enemies &enemies, const std::vector<Cell> &cells, EnemyType type, int floor);
};

enum class Outcome
{
    Exploring,
    Defeat,
    Victory
};
enum class Action
{
    Move,
    Wait,
    Heal
};
class Wall
{
  public:
    explicit Wall(Cell cell) : cell_(cell)
    {
    }
    Cell cell() const
    {
        return cell_;
    }

  private:
    Cell cell_;
};
using Log = std::function<void(std::string_view)>;

class World
{
  public:
    World(unsigned seed, Log log);
    bool act(Action action, Cell direction = {0, 0});
    bool walkable(Cell cell) const;
    const auto &walls() const
    {
        return walls_;
    }
    bool exitOpen() const;
    bool verify() const;

    Cell player() const
    {
        return player_.cell;
    }
    Cell exit() const
    {
        return exit_;
    }
    const Health &health() const
    {
        return player_.health;
    }
    int floor() const
    {
        return floor_;
    }
    int supplies() const
    {
        return supplies_;
    }
    int turn() const
    {
        return turn_;
    }
    Outcome outcome() const
    {
        return outcome_;
    }
    const auto &enemies() const
    {
        return enemies_;
    }
    const auto &pickups() const
    {
        return pickups_;
    }
    std::string_view message() const
    {
        return message_;
    }

  private:
    using Distances = std::array<int, Width * Height>;
    Distances distances(Cell start, bool avoidEnemies = false) const;
    void generate();
    void enemyTurn();
    void note(std::string_view message);

    Log log_;
    std::mt19937 random_;
    std::array<std::optional<Wall>, Width * Height> walls_{};
    Character player_{{1, 1}, {30, 1}};
    Cell exit_{1, 1};
    Enemies enemies_;
    std::vector<Cell> pickups_;
    int floor_ = 1;
    int supplies_ = 1;
    int turn_ = 0;
    Outcome outcome_ = Outcome::Exploring;
    std::string_view message_;
};
} // namespace dungeon
