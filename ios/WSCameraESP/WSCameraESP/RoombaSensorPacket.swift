import Foundation

enum RoombaSensorPacket: UInt8 {
  case bumps = 7
  case cliffLeft = 9
  case cliffFrontLeft = 10
  case cliffFrontRight = 11
  case cliffRight = 12
  case voltage = 22
  case current = 23
  case temperature = 24
  case leftMotorCurrent = 54
  case rightMotorCurrent = 55
  case lightBumper = 45
}

struct RoombaSensorData {
  var bumps: UInt8 = 0
  var cliffLeft: Bool = false
  var cliffFrontLeft: Bool = false
  var cliffFrontRight: Bool = false
  var cliffRight: Bool = false
  var voltage: UInt16 = 0
  var current: Int16 = 0
  var temperature: Int8 = 0
  var leftMotorCurrent: Int16 = 0
  var rightMotorCurrent: Int16 = 0
  var lightBumper: UInt8 = 0
}

enum RoombaParseError: Error {
    case insufficientData
    case invalidHeader(UInt8)
    case dataTooShort(expected: Int, actual: Int)
    case checksumMismatch(sum: UInt32, checksum: UInt8)
}

class RoombaDataParser {
  static func parse(data: Data) throws -> RoombaSensorData {
    // Need at least header (1) + N-bytes (1) + checksum (1)
    guard data.count >= 3 else {
      throw RoombaParseError.insufficientData
    }
        
    // First byte should be 19 (header)
    guard data[0] == 19 else {
      throw RoombaParseError.invalidHeader(data[0])
    }
        
    // Second byte is N-bytes (length of data before checksum)
    let nBytes = Int(data[1])
        
    // Verify we have enough data
    guard data.count >= (nBytes + 3) else {
      throw RoombaParseError.dataTooShort(expected: nBytes + 3, actual: data.count)
    } // +3 for header, n-bytes, and checksum
        
    // Verify checksum
    var sum: UInt32 = 0
    for i in 0 ..< (data.count - 1) { // Include header and n-bytes, exclude checksum
        sum += UInt32(data[i])
    }
    let checksum = data[data.count - 1]
    guard (sum + UInt32(checksum)) & 0xFF == 0 else {
        throw RoombaParseError.checksumMismatch(sum: sum, checksum: checksum)
    }
        
    var result = RoombaSensorData()
    var index = 2 // Start after header and n-bytes
        
    while index < (data.count - 1) { // -1 for checksum
      guard let packetId = RoombaSensorPacket(rawValue: data[index]) else {
        index += 1
        continue
      }
            
      index += 1 // Move past packet ID
            
      switch packetId {
      case .bumps:
        result.bumps = data[index]
        index += 1
                
      case .cliffLeft:
        result.cliffLeft = data[index] != 0
        index += 1
                
      case .cliffFrontLeft:
        result.cliffFrontLeft = data[index] != 0
        index += 1
                
      case .cliffFrontRight:
        result.cliffFrontRight = data[index] != 0
        index += 1
                
      case .cliffRight:
        result.cliffRight = data[index] != 0
        index += 1
                
      case .voltage:
        result.voltage = UInt16(data[index]) << 8 | UInt16(data[index + 1])
        index += 2
                
      case .current:
        result.current = Int16(bitPattern: UInt16(data[index]) << 8 | UInt16(data[index + 1]))
        index += 2
                
      case .temperature:
        result.temperature = Int8(bitPattern: data[index])
        index += 1
                
      case .leftMotorCurrent:
        result.leftMotorCurrent = Int16(bitPattern: UInt16(data[index]) << 8 | UInt16(data[index + 1]))
        index += 2
                
      case .rightMotorCurrent:
        result.rightMotorCurrent = Int16(bitPattern: UInt16(data[index]) << 8 | UInt16(data[index + 1]))
        index += 2
                
      case .lightBumper:
        result.lightBumper = data[index]
        index += 1
      }
    }
        
    return result
  }
}

extension RoombaSensorData {
  var rightBumperPressed: Bool {
    return bumps & 0x01 != 0
  }
  
  var leftBumperPressed: Bool {
    return bumps & 0x02 != 0
  }
  
  var batteryVoltage: Float {
    return Float(voltage) / 1000.0 // Convert to volts
  }
  
  var batteryAmps: Float {
    return Float(current) / 1000.0 // Convert to amps
  }
  
  // Light Bumper interpretation
  var lightBumpLeft: Bool {
      return lightBumper & 0x01 != 0
  }
  
  var lightBumpFrontLeft: Bool {
      return lightBumper & 0x02 != 0
  }
  
  var lightBumpCenterLeft: Bool {
      return lightBumper & 0x04 != 0
  }
  
  var lightBumpCenterRight: Bool {
      return lightBumper & 0x08 != 0
  }
  
  var lightBumpFrontRight: Bool {
      return lightBumper & 0x10 != 0
  }
  
  var lightBumpRight: Bool {
      return lightBumper & 0x20 != 0
  }
  
  // Wheel drop sensors in bumps byte
  var rightWheelDrop: Bool {
      return bumps & 0x04 != 0
  }
  
  var leftWheelDrop: Bool {
      return bumps & 0x08 != 0
  }
  
  // Motor current interpretation
  var leftMotorAmps: Float {
      return Float(leftMotorCurrent) / 1000.0  // Convert to amps
  }
  
  var rightMotorAmps: Float {
      return Float(rightMotorCurrent) / 1000.0  // Convert to amps
  }
  
  // Temperature in Celsius
  var temperatureCelsius: Int8 {
      return temperature
  }
  
  // Helper method to get cliff sensor status
  var anyCliffDetected: Bool {
      return cliffLeft || cliffFrontLeft || cliffFrontRight || cliffRight
  }
  
  // Helper for any bumper pressed
  var anyBumperPressed: Bool {
      return leftBumperPressed || rightBumperPressed
  }
  
  // Helper for any light bumper activated
  var anyLightBumperActivated: Bool {
      return lightBumper != 0
  }
  
  // Description for debugging
  var description: String {
      return """
      Bumpers: L=\(leftBumperPressed) R=\(rightBumperPressed)
      Wheel Drop: L=\(leftWheelDrop) R=\(rightWheelDrop)
      Cliff Sensors: L=\(cliffLeft) FL=\(cliffFrontLeft) FR=\(cliffFrontRight) R=\(cliffRight)
      Light Bumpers: L=\(lightBumpLeft) FL=\(lightBumpFrontLeft) CL=\(lightBumpCenterLeft) CR=\(lightBumpCenterRight) FR=\(lightBumpFrontRight) R=\(lightBumpRight)
      Battery: \(String(format: "%.2fV", batteryVoltage)) \(String(format: "%.2fA", batteryAmps))
      Temperature: \(temperatureCelsius)°C
      Motors: L=\(String(format: "%.2fA", leftMotorAmps)) R=\(String(format: "%.2fA", rightMotorAmps))
      """
  }
}
