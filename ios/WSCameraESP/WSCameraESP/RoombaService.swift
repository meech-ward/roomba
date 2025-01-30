import Foundation
import MightFail
import UIKit

struct Log: Identifiable, Equatable {
  let message: String
  let id = UUID()
  let timestamp = Date().formatted(date: .omitted, time: .standard)
}

actor RoombaService {
  @MainActor
  static let shared = RoombaService()

  @MainActor
  private init() {}

  deinit {
    wsStateTask?.cancel()
    wsEventTask?.cancel()
  }

  // MARK: - State

  @MainActor
  private(set) var connectionState = AsyncStreamStateManager(DataWebSocket.ConnectionState.disconnected)

  @MainActor
  private(set) var isStreaming = AsyncStreamStateManager(false)

  @MainActor
  private(set) var logs = AsyncStreamStateManager<Log>(Log(message: "Start Logs"))

  @MainActor
  private(set) var dataMessage = AsyncStreamStateManager<Data?>(nil)

  @MainActor
  private(set) var vaccumSpeed = AsyncStreamStateManager<Int8>(0)

  private(set) var dataWebSocket: DataWebSocket?
  private(set) var networkManager: NetworkManager?

  private var wsEventTask: Task<Void, Never>?
  private var wsStateTask: Task<Void, Never>?
  private var gameControllerEventsTask: Task<Void, Never>?

  private func resetState() async {
    await isStreaming.update(false)
  }

  // MARK: - Setup Connection

  private var setup = false

  func configure(withUrl url: URL) async throws {
    await setupDataWebSocket(url: url)
  }

  private func setupDataWebSocket(url: URL) async {
    guard !setup else { return }
    setup = true
    await logs.update(Log(message: "setupDataWebSocket"))
    let dataWebSocket = DataWebSocket(url: url)
    self.dataWebSocket = dataWebSocket
    let states = await dataWebSocket.connect()
    await logs.update(Log(message: "try to connect"))

    wsStateTask = Task.detached(priority: .background) {
      for await state in states {
        await self.connectionState.update(state)
        switch state {
        case .connected:
          print("Connected to websockets")
          await self.logs.update(Log(message: "Connected to websocket camera"))
          await self.listenForWSUpdates(dataWebSocket: dataWebSocket)
        case .disconnected:
          print("Disconnected from websockets")
          await self.logs.update(Log(message: "disconnected from websocket camera"))
          await self.resetState()
        case .connecting:
          print("connecting to websockets")
          await self.logs.update(Log(message: "Connecting to websocket camera"))
        }
      }
    }
  }

  // MARK: - WS Data In

  private func listenForWSUpdates(dataWebSocket: DataWebSocket) async {
    wsEventTask?.cancel()
    if let motorDataTask {
      motorDataTask.cancel()
      self.motorDataTask = nil
      await startSendingMotorData()
    }
    wsEventTask = Task.detached(priority: .userInitiated) { [weak self] in
      while !Task.isCancelled {
        let (error, message) = await mightFail { try await dataWebSocket.receive() }
        guard let message else {
          switch error {
          case is CancellationError:
            print("Task cancelled")
          default:
            await self?.processWebSocketMessage(error: error)
          }
          continue
        }

        switch message {
        case .string(let text):
          await self?.processWebSocketMessage(string: text)
        case .data(let data):
          await self?.processWebSocketMessage(data: data)
        case .none:
          fallthrough
        @unknown default:
          continue
        }
      }
    }
  }

  private func processWebSocketMessage(string: String) async {
    // We shouldn't be getting strings
    await logs.update(Log(message: "Received string: \(string)"))
  }

  @MainActor
  let sensors = AsyncStreamStateManager<RoombaSensorData>(RoombaSensorData())
  private func websocketDidReceiveSensorData(_ data: Data) async {
    let (error, sensorData) = mightFail { try RoombaDataParser.parse(data: data) }
    guard let sensorData else {
      print("couldn't do anything with this data \(error.localizedDescription)")
      await logs.update(Log(message: "Could not parse sensor data \(error.localizedDescription)"))
      return
    }

    await MainActor.run { sensors.update(sensorData) }
  }

  private func processWebSocketMessage(data: Data) async {
    if data.count < 200 {
      await websocketDidReceiveSensorData(data)
      return
    }
    await dataMessage.update(data)
    dataContinuations.values.forEach { $0.yield(data) }
    await recordFrame()
  }

  private func processWebSocketMessage(error: Error) async {
    // ignore errors here as they should be recoverable
    await logs.update(Log(message: "Received error: \(error)"))
  }

  // MARK: - Listen to data in

  private var dataContinuations: [UUID: AsyncStream<Data>.Continuation] = [:]
  private func setDataContinuation(id: UUID, continuation: AsyncStream<Data>.Continuation?) {
    dataContinuations[id] = continuation
  }

  private var timestamps: [CFAbsoluteTime] = Array(repeating: 0, count: 60)
  private var currentIndex = 0
  private let capacity = 60

  @MainActor
  let fps = AsyncStreamStateManager<Double>(0)

  private func recordFrame() async {
    timestamps[currentIndex] = CFAbsoluteTimeGetCurrent()
    currentIndex = (currentIndex + 1) % capacity
    let now = CFAbsoluteTimeGetCurrent()
    let oldestTime = timestamps.min() ?? now

    // Avoid division by zero
    let duration = now - oldestTime
    guard duration > 0 else {
      fps.update(0)
      return
    }

    fps.update(Double(capacity) / duration)
  }

  func monitorDataStream() -> AsyncStream<Data> {
    return AsyncStream { [weak self] continuation in
      guard let self = self else { return }

      let id = UUID()
      Task {
        await self.setDataContinuation(id: id, continuation: continuation)
        if let data = await self.dataMessage.value {
          continuation.yield(data)
          await recordFrame()
        }
      }

      continuation.onTermination = { @Sendable [weak self] _ in
        guard let self = self else { return }
        Task {
          await self.setDataContinuation(id: id, continuation: nil)
        }
      }
    }
  }

  // MARK: - WS Data Out

  func startStream() async {
    guard await !isStreaming.value, let dataWebSocket else {
      return
    }
    let (error, _, success) = await mightFail { try await dataWebSocket.send(message: "start") }
    guard success else {
      await logs.update(Log(message: "Error staring stream \(error.localizedDescription)"))
      return
    }
    await logs.update(Log(message: "Started stream"))
    await isStreaming.update(true)
  }

  func stopStream() async {
    guard await isStreaming.value, let dataWebSocket else {
      return
    }
    let (error, _, success) = await mightFail { try await dataWebSocket.send(message: "stop") }
    guard success else {
      await logs.update(Log(message: "Error stopping stream \(error.localizedDescription)"))
      return
    }
    await logs.update(Log(message: "Stopped stream"))
    await isStreaming.update(false)
  }

  func updateDisplay(_ display: String) async throws {
    try await sendCommand(["display": display])
  }

  func safeMode() async throws {
    try await sendCommand(["mode": 0])
  }

  func fullMode() async throws {
    try await sendCommand(["mode": 1])
  }

  func startRoomba() async throws {
    try await sendCommand(["start": true])
  }

  func stopRoomba() async throws {
    try await sendCommand(["stop": true])
  }

  func playSong() async throws {
    try await sendCommand(["playSong": true])
  }

  func playSong2() async throws {
    try await sendCommand(["playSong2": true])
  }

  func playPunk() async throws {
    try await sendCommand(["playDaftPunk": true])
  }

  func streamSensors(_ stream: Bool) async throws {
    try await sendCommand(["streamSensors": stream])
  }

  private func sendCommand(_ command: [String: Any]) async throws {
    guard let dataWebSocket else {
      throw NSError(domain: "No websocket connection", code: 0, userInfo: nil)
    }

    guard let jsonData = try? JSONSerialization.data(withJSONObject: command), let jsonString = String(data: jsonData, encoding: .utf8) else {
      throw NSError(domain: "Invalid JSON data", code: 0, userInfo: nil)
    }
    try await dataWebSocket.send(message: jsonString)
  }

  // MARK: - Motor data out

  private(set) var leftMotor: Motor = .init()
  private(set) var rightMotor: Motor = .init()
  private(set) var vaccumMotor: Motor = .init()
  private(set) var brushMotor: Motor = .init()

  private var motorDataTask: Task<Void, Never>?
  func startSendingMotorData() async {
    guard let dataWebSocket else {
      return
    }
    let motors = [leftMotor, rightMotor, vaccumMotor, brushMotor]
    motorDataTask?.cancel()
    motorDataTask = Task.detached {
      while Task.isCancelled == false {
        let vaccumMotorSpeed = await self.vaccumSpeed.value
        await self.vaccumMotor.set(speed: vaccumMotorSpeed)
        await self.brushMotor.set(speed: vaccumMotorSpeed)
        let binaryData = await MotorCommand.toBinaryData(motors: motors)
        let (error, _, success) = await mightFail { try await dataWebSocket.send(data: binaryData) }
        guard success else {
          await self.logs.update(Log(message: "Error sending message \(binaryData) \(error.localizedDescription)"))
          return
        }
        // Must keep sending data or motors will shut down
        try? await Task.sleep(for: .milliseconds(150))
      }
    }
  }

  func stopSendingMotorData() async {
    motorDataTask?.cancel()
    motorDataTask = nil
  }

  // MARK: - Game Controller

  func startGameController() async {
    let gameController = await GameController.shared()
    await gameController.setup()

    gameControllerEventsTask = Task {
      await withTaskGroup(of: Void.self) { group in
        group.addTask {
          for await (x, y) in await gameController.leftThumb.stream() {
            let speed = Int8(y * 100)
            await self.leftMotor.set(speed: speed)
            if Task.isCancelled {
              break
            }
          }
        }

        group.addTask {
          for await (x, y) in await gameController.rightThumb.stream() {
            let speed = Int8(y * 100)
            await self.rightMotor.set(speed: speed)
            if Task.isCancelled {
              break
            }
          }
        }
      }
    }
  }

  func stopGameController() async {
    let gameController = await GameController.shared()
    await gameController.tearDown()
    gameControllerEventsTask?.cancel()
    gameControllerEventsTask = nil
  }
}
