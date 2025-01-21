# WSCameraESP iOS App

iOS app to control a custom Roomba robot via an ESP32 microcontroller. Communicates over WebSocket connection to stream camera feed and send motor control commands.

## Features

- Live camera feed from ESP32
- Gamepad/virtual controller support for motor control
- Connection status monitoring
- Real-time motor command streaming

## Requirements 

- iOS 18.1+
- ESP32 running compatible WebSocket server firmware
- WiFi connection to ESP32 (192.168.4.1)

## Usage

Connect to ESP32's WiFi network and launch app. Use virtual gamepad controls or physical controller to drive robot while viewing camera feed.

## Development

Built with:
- SwiftUI/UIKit hybrid architecture 
- Async/await for WebSocket handling
- GameController framework
- Motor command binary protocol