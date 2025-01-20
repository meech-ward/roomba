//
//  MotorCommand.swift
//  WSCameraESP
//
//  Created by Sam Meech-Ward on 2025-01-14.
//

import Foundation

actor Motor {
  var speed: UInt8 = 0
  var isForward: Bool = true
  func set(speed: UInt8) {
    self.speed = speed
  }

  func set(forward: Bool) {
    isForward = forward
  }

  func set(speed: UInt8, forward: Bool) {
    self.speed = speed
    isForward = forward
  }
}

enum MotorCommand {
  static func toBinaryData(motors: [Motor]) async -> Data {
    var data = Data(capacity: 5)

    // Add speeds
    for motor in motors {
      data.append(await motor.speed)
    }

    // Pack directions into single byte
    var directionsByte: UInt8 = 0
    for (index, motor) in motors.enumerated() {
      if await !motor.isForward {
        directionsByte |= (1 << index)
      }
    }
    data.append(directionsByte)

    return data
  }
}

// When sending:
// let websocket = URLSessionWebSocketTask( /* ... */ )
//
// func sendMotorCommand() {
//  let command = MotorCommand(motors: [
//    .init(speed: 255, isForward: true),
//    .init(speed: 128, isForward: false),
//    .init(speed: 64, isForward: true),
//    .init(speed: 192, isForward: true)
//  ])
//
//  let message = URLSessionWebSocketTask.Message.data(command.toBinaryData())
//  websocket.send(message) { error in
//    if let error = error {
//      print("Error sending: \(error)")
//    }
//  }
// }
