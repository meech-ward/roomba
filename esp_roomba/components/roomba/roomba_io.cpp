#include <string_view>
#include <thread>

#include "esp_log.h"
#include "roomba.hpp"

auto Roomba::setLeds(uint8_t leds, uint8_t powerColor, uint8_t powerIntensity) noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  uint8_t data[3]{leds, powerColor, powerIntensity};
  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_LEDS, data);
}

auto Roomba::song(uint8_t songNumber, std::span<const uint8_t> notes, std::span<const uint8_t> durations) noexcept
  -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  if (songNumber > 4 || notes.size() != durations.size() || notes.size() > 16) {
    return std::unexpected(Error::InvalidParameter);
  }
  std::vector<uint8_t> data;
  data.reserve(2 + notes.size() * 2);
  data.push_back(songNumber);
  data.push_back((uint8_t)notes.size());
  for (size_t i = 0; i < notes.size(); i++) {
    data.push_back(notes[i]);
    data.push_back(durations[i]);
  }
  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_SONG, data);
}

auto Roomba::playSong(uint8_t songNumber) noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  if (songNumber > 4) {
    return std::unexpected(Error::InvalidParameter);
  }
  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_PLAY, std::span(&songNumber, 1));
}
// A1 B1 C#1 E1 F#1 G#1 F#1 E#1 C#1 B1 a1 b1 C#1 B1 a1 B1
auto Roomba::playCrowdPleaserSong() noexcept -> std::expected<void, Error> {
  constexpr uint32_t NOTES_PER_SONG = 16;
  constexpr uint32_t UNITS_PER_NOTE = 16;
  constexpr uint32_t UNITS_TO_MS = (1000 / 64);  // Convert 1/64th second units to milliseconds

  constexpr uint32_t SONG_DURATION_MS = NOTES_PER_SONG * UNITS_PER_NOTE * UNITS_TO_MS + 100;

  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  static constexpr uint8_t notes1[16] = {81, 0, 83, 0, 85, 0, 88, 0, 90, 92, 90, 0, 88, 0, 85, 83};
  static constexpr uint8_t durations[16] = {
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    32,
    16,
    16,
    16,
    16,
    16,
    16 * 9,
    16 * 8,
  };

  auto rc = song(0, std::span<const uint8_t>(notes1, 16), std::span<const uint8_t>(durations, 16));

  if (!rc) {
    return rc;
  }

  return playSong(0);
}

auto Roomba::playCrowdPleaserSong2() noexcept -> std::expected<void, Error> {
  constexpr uint32_t NOTES_PER_SONG = 16;
  constexpr uint32_t UNITS_PER_NOTE = 16;
  constexpr uint32_t UNITS_TO_MS = (1000 / 64);  // Convert 1/64th second units to milliseconds

  constexpr uint32_t SONG_DURATION_MS = NOTES_PER_SONG * UNITS_PER_NOTE * UNITS_TO_MS + 100;

  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  static constexpr uint8_t notes1[16] = {69, 0, 71, 0, 73, 0, 76, 0, 78, 80, 78, 0, 76, 0, 73, 71};
  static constexpr uint8_t durations[16] = {
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    32,
    16,
    16,
    16,
    16,
    16,
    16 * 9,
    16 * 8,
  };

  auto rc = song(0, std::span<const uint8_t>(notes1, 16), std::span<const uint8_t>(durations, 16));

  if (!rc) {
    return rc;
  }

  return playSong(0);
}
auto Roomba::playInTheEnd() noexcept -> std::expected<void, Error> {
  constexpr uint32_t NOTES_PER_SONG = 16;
  constexpr uint32_t UNITS_PER_NOTE = 16;
  constexpr uint32_t UNITS_TO_MS = (1000 / 64);  // Convert 1/64th second units to milliseconds

  constexpr uint32_t SONG_DURATION_MS = NOTES_PER_SONG * UNITS_PER_NOTE * UNITS_TO_MS + 100;

  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  static constexpr uint8_t notes1[9] = {63, 70, 70, 66, 65, 65, 65, 65, 66};
  static constexpr uint8_t durations[9] = {
  16*4, 16*4, 16*4, 16*4, 16*4, 16*4, 16*4, 16*2, 16*2
  };

  auto rc = song(0, std::span<const uint8_t>(notes1, 16), std::span<const uint8_t>(durations, 16));

  if (!rc) {
    return rc;
  }

  return playSong(0);
}

// // F#1 A1 F#2 A2 C#2 A2 F#2 A2 E0 E1 A2 E1 B2 A2 G#2 A2
// // D0 D0 F#3 D2 B3 A3 F#3 D2 D1 D1 F@2 A2 F#1 F#1 F#1 F#1
auto Roomba::playDaftPunkSong() noexcept -> std::expected<void, Error> {
  constexpr uint32_t NOTES_PER_SONG = 16;
  constexpr uint32_t UNITS_PER_NOTE = 16;
  constexpr uint32_t UNITS_TO_MS = (1000 / 64);  // Convert 1/64th second units to milliseconds

  constexpr uint32_t SONG_DURATION_MS = NOTES_PER_SONG * UNITS_PER_NOTE * UNITS_TO_MS + 200;

  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  static constexpr uint8_t notes1[16] = {
    54, 57, 66, 69, 85, 69, 66, 69, 52, 64, 69, 64, 71, 69, 68, 69  // Final notes
  };
  static constexpr uint8_t durations[16] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

  static constexpr uint8_t notes2[16] = {38, 38, 66, 50, 71, 69, 66, 50, 38, 38, 54, 57, 42, 42, 42, 42};
  auto rc = song(0, std::span<const uint8_t>(notes1, 16), std::span<const uint8_t>(durations, 16));
  auto rc2 = song(1, std::span<const uint8_t>(notes2, 16), std::span<const uint8_t>(durations, 16));

  if (!rc) {
    return rc;
  }

  return playSong(0);
  // ESP_LOGI("roomba", "playing song 0");
  // std::this_thread::sleep_for(std::chrono::milliseconds(SONG_DURATION_MS));
  // ESP_LOGI("roomba", "slept %dms", (int)SONG_DURATION_MS);
  // ESP_LOGI("roomba", "playing song 1");
  // return playSong(1);
}



auto Roomba::writeToDisplay(std::string_view text) noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  if (currentMode_ == Mode::Off || currentMode_ == Mode::Passive) {
    return std::unexpected(Error::InvalidParameter);
  }

  std::array<uint8_t, 4> displayData{' ', ' ', ' ', ' '};

  size_t startPos = 0;
  if (text.length() > 4) {
    startPos = text.length() - 4;
  }

  for (size_t i = 0; i < std::min(text.length() - startPos, size_t{4}); ++i) {
    uint8_t ch = text[startPos + i];
    if (ch < 32 || ch > 126) {
      ch = ' ';
    }
    displayData[i] = ch;
  }

  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_DIGITS, displayData);
}