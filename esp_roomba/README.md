# WSCameraESP ESP32-S3 Server

ESP32-S3 application to control a custom Roomba robot. Creates WiFi access point and WebSocket server to stream camera feed and receive motor control commands from iOS app.

## Features

- WiFi access point (192.168.4.1) 
- WebSocket server for real-time communication
- JPEG camera streaming
- 3 motor PWM control:
 - Left drive motor
 - Right drive motor 
 - Vacuum/brush motors
- System diagnostics monitoring
- Binary motor control protocol

## Hardware Requirements

- ESP32-S3 board 
- Camera module supported by esp32-camera library
- L298N or similar motor drivers  
- 12V motors

## Development 

Built with:
- ESP-IDF
- Modern C++ (C++23)
- WebSocket protocol
- Hardware PWM motor control
- Task-based concurrent architecture 

## Motors
Uses 3 PWM channels to control motor speeds:
- Channel 0: Left drive motor
- Channel 1: Right drive motor
- Channel 2: Vacuum and brush motors

10-bit PWM resolution provides 1024 speed levels (0-1023).

## Tasks

- Camera capture task: Gets frames from camera
- Stream task: Sends JPEG frames over WebSocket
- Motor control task: Updates motor speeds/directions  
- Main task: Monitors system status