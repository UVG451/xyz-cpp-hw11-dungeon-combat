#include "World.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <queue>
#include <stdexcept>
#include <utility>

namespace dungeon
{
namespace
{
constexpr Cell Start{1, 1};
constexpr std::array<Cell, 4> Steps{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

int index(Cell cell)
{
    return cell.y * Width + cell.x;
}

bool interior(Cell cell)
{
    return cell.x > 0 && cell.y > 0 && cell.x < Width - 1 && cell.y < Height - 1;
}
} // namespace

int Health::hit(int power)
{
    const int damage = std::min(current, std::max(1, power - armor));
    current -= damage;
    return damage;
}

int Health::heal(int amount)
{
    const int restored = std::min(amount, maximum - current);
    current += restored;
    return restored;
}

World::World(unsigned seed, Log log) : log_(std::move(log)), random_(seed)
{
    generate();
}

bool World::walkable(Cell cell) const
{
    return interior(cell) && !walls_[index(cell)];
}

World::Distances World::distances(Cell start, bool avoidEnemies) const
{
    Distances result;
    result.fill(-1);
    result[index(start)] = 0;
    std::queue<Cell> frontier;
    frontier.push(start);
    while (!frontier.empty())
    {
        const Cell cell = frontier.front();
        frontier.pop();
        for (const Cell direction : Steps)
        {
            const Cell next = cell + direction;
            if (!walkable(next) || result[index(next)] >= 0)
                continue;
            if (avoidEnemies && std::any_of(enemies_.begin(), enemies_.end(),
                                            [next](const auto &enemy) { return enemy->cell == next; }))
                continue;
            result[index(next)] = result[index(cell)] + 1;
            frontier.push(next);
        }
    }
    return result;
}

void World::generate()
{
    player_.cell = Start;
    for (int y = 0; y < Height; ++y)
        for (int x = 0; x < Width; ++x)
            walls_[index({x, y})].emplace(Cell{x, y});
    walls_[index(Start)].reset();
    std::vector<Cell> path{Start};
    while (!path.empty())
    {
        const Cell cell = path.back();
        auto directions = Steps;
        std::shuffle(directions.begin(), directions.end(), random_);
        const auto nextStep = std::find_if(directions.begin(), directions.end(),
                                           [&](Cell step)
                                           {
                                               const Cell next = cell + step + step;
                                               return interior(next) && !walkable(next);
                                           });
        if (nextStep == directions.end())
        {
            path.pop_back();
            continue;
        }
        const Cell middle = cell + *nextStep;
        const Cell next = middle + *nextStep;
        walls_[index(middle)].reset();
        walls_[index(next)].reset();
        path.push_back(next);
    }

    // Open links between corridors so a fight does not always have one approach.
    std::uniform_int_distribution<int> gap(0, 7);
    for (int y = 1; y < Height - 1; ++y)
        for (int x = 1; x < Width - 1; ++x)
            if ((x % 2) != (y % 2) && gap(random_) == 0)
                walls_[index({x, y})].reset();

    const auto distance = distances(Start);
    exit_ = Start;
    for (int y = 1; y < Height - 1; ++y)
        for (int x = 1; x < Width - 1; ++x)
            if (distance[index({x, y})] > distance[index(exit_)])
                exit_ = {x, y};

    std::vector<Cell> spawnPoints;
    for (int y = 1; y < Height - 1; ++y)
        for (int x = 1; x < Width - 1; ++x)
        {
            const Cell cell{x, y};
            if (distance[index(cell)] >= 6 && !(cell == exit_))
                spawnPoints.push_back(cell);
        }
    std::shuffle(spawnPoints.begin(), spawnPoints.end(), random_);
    const int scouts = 2 + floor_;
    if (spawnPoints.size() < static_cast<std::size_t>(scouts + 2))
        throw std::logic_error("Not enough reachable spawn points");

    enemies_.clear();
    pickups_.clear();
    EnemySpawner::spawn(enemies_, {exit_}, EnemyType::Guardian, floor_);
    const std::vector<Cell> scoutCells(spawnPoints.begin(), spawnPoints.begin() + scouts);
    EnemySpawner::spawn(enemies_, scoutCells, EnemyType::Creeper, floor_);
    pickups_.assign(spawnPoints.begin() + scouts, spawnPoints.begin() + scouts + 2);
    assert(verify());
    if (!verify())
        throw std::logic_error("Generated an inconsistent floor");
    note("Find the guardian to unlock the stairs.");
}

void EnemySpawner::spawn(Enemies &enemies, const std::vector<Cell> &cells, EnemyType type, int floor)
{
    for (const Cell cell : cells)
    {
        if (type == EnemyType::Guardian)
            enemies.push_back(std::make_unique<Enemy>(cell, Health{9 + floor * 2, 1}, true, 4, 6));
        else
            enemies.push_back(std::make_unique<Creeper>(cell, floor));
    }
}

bool World::exitOpen() const
{
    return std::none_of(enemies_.begin(), enemies_.end(), [](const auto &enemy) { return enemy->guardian; });
}

void World::note(std::string_view message)
{
    message_ = message;
    log_(message);
}

bool World::act(Action action, Cell direction)
{
    if (outcome_ != Outcome::Exploring)
        return false;

    if (action == Action::Heal)
    {
        if (supplies_ == 0 || player_.health.current == player_.health.maximum)
        {
            note(supplies_ == 0 ? "The supply pouch is empty." : "Your health is full.");
            return false;
        }
        player_.health.heal(10);
        --supplies_;
        note("Used a field kit.");
    }
    else if (action == Action::Move)
    {
        if (std::abs(direction.x) + std::abs(direction.y) != 1)
            throw std::invalid_argument("Movement must be one cardinal step");
        const Cell next = player_.cell + direction;
        if (!walkable(next))
            return false;
        const auto enemy = std::find_if(enemies_.begin(), enemies_.end(),
                                        [next](const auto &target) { return target->cell == next; });
        if (enemy != enemies_.end())
        {
            (*enemy)->health.hit(4);
            note((*enemy)->guardian ? "You strike the guardian." : "You strike a crawler.");
            if ((*enemy)->health.current == 0)
            {
                note((*enemy)->guardian ? "Guardian down. The stairs are open." : "Crawler defeated.");
                enemies_.erase(enemy);
            }
        }
        else
        {
            if (next == exit_ && !exitOpen())
            {
                note("The guardian still holds the stairs.");
                return false;
            }
            player_.cell = next;
            const auto pickup = std::find(pickups_.begin(), pickups_.end(), player_.cell);
            if (pickup != pickups_.end())
            {
                ++supplies_;
                pickups_.erase(pickup);
                note("Field kit added to the supply pouch.");
            }
            if (player_.cell == exit_)
            {
                ++turn_;
                if (floor_ == Floors)
                {
                    outcome_ = Outcome::Victory;
                    note("Expedition complete.");
                }
                else
                {
                    ++floor_;
                    player_.health.heal(5);
                    generate();
                }
                return true;
            }
        }
    }

    ++turn_;
    enemyTurn();
    return true;
}

void World::enemyTurn()
{
    const auto detection = distances(player_.cell);
    for (auto &enemy : enemies_)
    {
        const int distance = detection[index(enemy->cell)];
        const bool inRange = enemy->finder.detects(distance);
        if (inRange && !enemy->finder.chasing)
            note(enemy->guardian ? "The guardian has spotted you." : "A crawler has spotted you.");
        enemy->finder.chasing = inRange;
        if (!enemy->finder.chasing)
            continue;
        if (distance == 1)
        {
            player_.health.hit(enemy->attackPower);
            note(enemy->guardian ? "The guardian hits you." : "A crawler hits you.");
            if (player_.health.current == 0)
            {
                outcome_ = Outcome::Defeat;
                note("Expedition lost.");
                return;
            }
            continue;
        }

        const auto reachable = distances(player_.cell, true);
        int best = Width * Height;
        Cell destination = enemy->cell;
        for (const Cell step : Steps)
        {
            const Cell next = enemy->cell + step;
            if (walkable(next) && reachable[index(next)] >= 0 && reachable[index(next)] < best &&
                (enemy->guardian || !(next == exit_)) && !(next == player_.cell))
            {
                best = reachable[index(next)];
                destination = next;
            }
        }
        enemy->cell = destination;
    }
}

bool World::verify() const
{
    if (!walkable(player_.cell) || !walkable(exit_) || player_.cell == exit_)
        return false;
    const auto distance = distances(player_.cell);
    if (distance[index(exit_)] < 0)
        return false;
    std::array<bool, Width * Height> occupied{};
    occupied[index(player_.cell)] = true;
    for (const auto &enemy : enemies_)
    {
        if (!walkable(enemy->cell) || distance[index(enemy->cell)] < 0 || occupied[index(enemy->cell)])
            return false;
        occupied[index(enemy->cell)] = true;
    }
    for (const Cell kit : pickups_)
    {
        if (!walkable(kit) || distance[index(kit)] < 0 || occupied[index(kit)])
            return false;
        occupied[index(kit)] = true;
    }
    return true;
}
} // namespace dungeon
