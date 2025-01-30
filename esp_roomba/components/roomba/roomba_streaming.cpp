#include "roomba.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"


auto Roomba::startStreaming(std::span<const SensorPacket> packets) noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }

  std::lock_guard<std::mutex> lock(uart_mutex_);

  // Convert packets to uint8_t array
  std::vector<uint8_t> data;
  data.reserve(packets.size() + 1);
  
  // First byte is number of packets
  data.push_back(static_cast<uint8_t>(packets.size()));
  
  // Add each packet ID
  for (const auto& packet : packets) {
    data.push_back(static_cast<uint8_t>(packet));
  }

  // Send stream command with packet IDs
  return sendCommand(CMD_STREAM_SENSORS, data);
}

auto Roomba::stopStreaming() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }

  std::lock_guard<std::mutex> lock(uart_mutex_);
  
  // Send 0 packets to pause the stream
  const uint8_t pause_data[] = {0};
  return sendCommand(CMD_PAUSE_RESUME_STREAM, std::span(pause_data));
}

auto Roomba::readStream() noexcept -> std::expected<std::vector<uint8_t>, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }

  std::lock_guard<std::mutex> lock(uart_mutex_);

  // Wait for header byte
  uint8_t byte;
  size_t bytes_read;
  
  // Look for stream header (19)
  do {
    if (uart_read_bytes(config_.uart_num, &byte, 1, pdMS_TO_TICKS(STREAM_READ_TIMEOUT_MS)) != 1) {
      return std::unexpected(Error::Timeout);
    }
  } while (byte != STREAM_HEADER);

  // Read N-bytes (number of bytes before checksum)
  if (uart_read_bytes(config_.uart_num, &byte, 1, pdMS_TO_TICKS(STREAM_READ_TIMEOUT_MS)) != 1) {
    return std::unexpected(Error::Timeout);
  }
  
  const size_t n_bytes = byte;
  
  // Read data bytes + checksum
  std::vector<uint8_t> data(n_bytes + 1);
  if (uart_read_bytes(config_.uart_num, data.data(), data.size(), pdMS_TO_TICKS(STREAM_READ_TIMEOUT_MS)) 
      != static_cast<int>(data.size())) {
    return std::unexpected(Error::Timeout);
  }

  // Verify checksum
  uint32_t sum = STREAM_HEADER + n_bytes;
  for (const auto& b : data) {
    sum += b;
  }
  
  if ((sum & 0xFF) != 0) {
    ESP_LOGW(LOG_TAG_, "Stream checksum verification failed");
    return std::unexpected(Error::SensorError);
  }

  // Return the complete packet including header and n_bytes
  std::vector<uint8_t> complete_packet;
  complete_packet.reserve(2 + data.size());
  complete_packet.push_back(STREAM_HEADER);
  complete_packet.push_back(n_bytes);
  complete_packet.insert(complete_packet.end(), data.begin(), data.end());
  
  return complete_packet;
}

auto Roomba::startStreamTask(StreamCallback callback) noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }

  if (stream_task_handle_ != nullptr) {
    return std::unexpected(Error::InvalidParameter);
  }

  stream_callback_ = std::move(callback);
  stream_task_running_ = true;

  // Create the streaming task
  BaseType_t result = xTaskCreate(
    streamTaskFunction,
    "roomba_stream",
    4096,  // Stack size - adjust as needed
    this,  // Pass the Roomba instance as parameter
    5,     // Priority - adjust as needed
    &stream_task_handle_
  );

  if (result != pdPASS) {
    stream_task_running_ = false;
    stream_callback_ = nullptr;
    return std::unexpected(Error::InitializationError);
  }

  return {};
}

auto Roomba::stopStreamTask() noexcept -> std::expected<void, Error> {
  if (stream_task_handle_ == nullptr) {
    return {};  // Already stopped
  }

  // Stop the streaming task gracefully
  stream_task_running_ = false;
  
  // Wait for task to finish
  for (int i = 0; i < 10; i++) {
    if (stream_task_handle_ == nullptr) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Force delete if still running
  if (stream_task_handle_ != nullptr) {
    vTaskDelete(stream_task_handle_);
    stream_task_handle_ = nullptr;
  }

  stream_callback_ = nullptr;
  
  return stopStreaming();  // Stop the Roomba stream
}

void Roomba::streamTaskFunction(void* arg) {
  auto* roomba = static_cast<Roomba*>(arg);
  TickType_t last_wake_time = xTaskGetTickCount();
  
  while (roomba->stream_task_running_) {
    if (auto stream_data = roomba->readStream()) {
      // Create stream packet with timestamp
      StreamPacket packet{
        .data = std::move(*stream_data),
        .timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000)  // Convert to milliseconds
      };
      
      // Call the callback with the data
      if (roomba->stream_callback_) {
        roomba->stream_callback_(packet);
      }
    } else {
      // On error, wait a bit before retrying
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Ensure we don't read too frequently
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(15));  // 15ms between reads
  }

  // Task cleanup
  roomba->stream_task_handle_ = nullptr;
  vTaskDelete(nullptr);
} 