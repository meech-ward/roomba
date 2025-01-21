@preconcurrency import CoreMotion
import Foundation
@preconcurrency import GameController
import MightFail

actor GameController {
  // MARK: - singleton

  private static var _shared: GameController?
  static func shared() async -> GameController {
    guard let shared = _shared else {
      _shared = await GameController()
      return _shared!
    }
    return shared
  }
  
  private init() async {
    leftThumb = await .init((0, 0))
    rightThumb = await .init((0, 0))
  }
  
  // MARK: - controllers
  
  private let motionManager = CMMotionManager()
  private var didConnectNotificationTask: Task<Void, Never>?
  private var didDisconnectNotificationTask: Task<Void, Never>?
  
  private var virtualController: GCVirtualController? {
    willSet {
      setController(newValue?.controller)
    }
  }
    
  private var controller: GCController? {
    didSet {
      if let controller {
        configureController(controller)
      }
    }
  }

  private func setController(_ controller: GCController?) {
    self.controller = controller
  }
  
  // MARK: - State
   
  private(set) var leftThumb: AsyncStreamStateManager<(x: Float, y: Float)>
  private(set) var rightThumb: AsyncStreamStateManager<(x: Float, y: Float)>
    
  // MARK: - Setup and tear down
    
  func setup() async {
//    await startMotionUpdates()
    setupNotificationHandling()

    let controllers = GCController.controllers()
    guard let controller = controllers.first else {
      await setupVirtualController()
      return
    }
    self.controller = controller
  }
    
  func tearDown() {
    didConnectNotificationTask?.cancel()
    didDisconnectNotificationTask?.cancel()
    motionManager.stopDeviceMotionUpdates()
    tearDownVirtualController()
    leftThumb.update((0, 0))
    rightThumb.update((0, 0))
  }
    
  // MARK: - Virtual Controller
    
  private func setupVirtualController() async {
    let virtualController = await MainActor.run {
      let configuration = GCVirtualController.Configuration()
      configuration.elements = [GCInputLeftThumbstick, GCInputRightThumbstick]
      let virtualController = GCVirtualController(configuration: configuration)
      return virtualController
    }
    let (error, _, success) = await mightFail { try await virtualController.connect() }
    guard success else {
      print("Failed to connect virtual controller:", error.localizedDescription)
      return
    }
    self.virtualController = virtualController
  }
  
  private func tearDownVirtualController() {
    virtualController?.disconnect()
    virtualController = nil
  }
  
  // MARK: - controllers connect and disconnect
  
  private func setupNotificationHandling() {
    didConnectNotificationTask = Task {
      let connectStream = NotificationCenter.default.notifications(
        named: .GCControllerDidConnect
      )
      for await notification in connectStream {
        guard let gameController = notification.object as? GCController else { continue }
        await self.handleControllerConnection(gameController)
      }
    }
        
    didDisconnectNotificationTask = Task {
      let disconnectStream = NotificationCenter.default.notifications(
        named: .GCControllerDidDisconnect
      )
            
      for await notification in disconnectStream {
        guard let gameController = notification.object as? GCController else { continue }
        await self.handleControllerDisconnection(gameController)
      }
    }
  }
  
  private func handleControllerConnection(_ gameController: GCController) async {
    if gameController !== virtualController?.controller {
      tearDownVirtualController()
    }
    controller = gameController
  }
    
  private func handleControllerDisconnection(_ gameController: GCController) async {
    if gameController === virtualController?.controller {
      tearDownVirtualController()
    } else {
      if GCController.controllers().isEmpty {
        await setupVirtualController()
      }
    }
  }
  
  // MARK: - Controller inputs
    
//  private func startMotionUpdates() async {
//    guard motionManager.isDeviceMotionAvailable else {
//      return
//    }
//
//    motionManager.deviceMotionUpdateInterval = 1.0 / 60.0
//    motionManager.showsDeviceMovementDisplay = true
//
//    let queue = OperationQueue()
//
//    motionManager.startDeviceMotionUpdates(
//      using: .xArbitraryZVertical,
//      to: queue
//    ) { data, _ in
//      guard let data = data else { return }
//      let attitude = data.attitude
//        print("motion:", attitude.roll, attitude.pitch, attitude.yaw)
//    }
//  }
    


  private func configureController(_ controller: GCController) {
    Task.detached {
      let leftThumb = await self.leftThumb
      let rightThumb = await self.rightThumb
      guard let gamepad = controller.extendedGamepad else {
        return
      }
      
      gamepad.rightThumbstick.valueChangedHandler = { _, x, y in
        print("from gamepad right", x, y)
        rightThumb.update((x, y))
      }
      
      gamepad.leftThumbstick.valueChangedHandler = { _, x, y in
        print("from gamepad left", x, y)
        leftThumb.update((x, y))
      }
      
      for (_, button) in gamepad.buttons {
        button.valueChangedHandler = { _, _, _ in
          // Handle button
        }
      }
    }
  }
}
