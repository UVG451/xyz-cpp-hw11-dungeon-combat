#pragma once

#include <SFML/Audio.hpp>

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

class Logger
{
  public:
    Logger() : file_("dungeon.log", std::ios::app)
    {
        if (!file_)
            std::cerr << "[WARN][engine] Unable to open dungeon.log\n";
    }

    void write(LogLevel level, std::string_view area, std::string_view message)
    {
        constexpr std::array<std::string_view, 4> labels{"DEBUG", "INFO", "WARN", "ERROR"};
        const auto label = labels[static_cast<std::size_t>(level)];
        std::clog << '[' << label << "][" << area << "] " << message << '\n';
        if (file_)
            file_ << '[' << label << "][" << area << "] " << message << std::endl;
    }

  private:
    std::ofstream file_;
};

class Audio
{
  public:
    Audio(Logger &log, const std::vector<float> &melody, float secondsPerNote, float effectPitch)
    {
        if (load(musicBuffer_, melody, secondsPerNote))
        {
            music_.setBuffer(musicBuffer_);
            music_.setLoop(true);
            music_.setVolume(28);
            music_.play();
            log.write(LogLevel::Info, "engine", "Looping audio loaded");
        }
        else
            log.write(LogLevel::Warning, "engine", "Music unavailable");
        if (load(effectBuffer_, {effectPitch}, 0.09f))
            effect_.setBuffer(effectBuffer_);
        else
            log.write(LogLevel::Warning, "engine", "Effects unavailable");
    }

    void step()
    {
        if (effect_.getBuffer())
            effect_.play();
    }

  private:
    static bool load(sf::SoundBuffer &buffer, const std::vector<float> &notes, float duration)
    {
        constexpr unsigned Rate = 22050;
        constexpr float Tau = 6.2831853f;
        const auto length = static_cast<unsigned>(Rate * duration);
        std::vector<sf::Int16> samples;
        samples.reserve(notes.size() * length);
        for (const float note : notes)
            for (unsigned i = 0; i < length; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(length);
                const float envelope = std::sin(3.14159265f * t);
                const float wave = std::sin(Tau * note * i / Rate);
                samples.push_back(static_cast<sf::Int16>(wave * envelope * 3800));
            }
        return buffer.loadFromSamples(samples.data(), samples.size(), 1, Rate);
    }

    sf::SoundBuffer musicBuffer_;
    sf::SoundBuffer effectBuffer_;
    sf::Sound music_;
    sf::Sound effect_;
};
