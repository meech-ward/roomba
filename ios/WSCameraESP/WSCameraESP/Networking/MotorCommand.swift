
import Foundation

actor Motor {
  var speed: Int8 = 0 

  func set(speed: Int8) {
    // Clamp speed between -100 and 100
    self.speed = max(-100, min(100, speed))
  }
}

enum MotorCommand {
  static func toBinaryData(motors: [Motor]) async -> Data {
    var data = Data(capacity: motors.count)

    // Add speeds (positive values for forward, negative for reverse)
    for motor in motors {
      data.append(UInt8(bitPattern: await motor.speed))
    }

    return data
  }
}
