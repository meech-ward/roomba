import Foundation
import MightFail



actor DataWebSocket {
  enum Error: Swift.Error {
    case websocketClosed
    case invalidData
    case jsonParsingError
    case connectionFailed(underlying: Swift.Error)
    case connectionTimeout
    case pongTimeout
  }
    
  enum ConnectionState {
    case disconnected
    case connecting
    case connected
  }
  
  private var webSocketTask: URLSessionWebSocketTask?
  private var connectionState: ConnectionState = .disconnected
  private var retryCount = 0
  
  let networkManager: NetworkManager
  let url: URL
  private var onOpenTask: Task<Void, Never>?
    
  init(url: URL) {
    networkManager = NetworkManager()
    self.url = url
  }
  
  deinit {
    webSocketTask?.cancel(with: .normalClosure, reason: nil)
    onOpenTask?.cancel()
    onOpenTask = nil
  }

  // MARK: - Connection Delegate
  
  private lazy var delegate: DataWebSocketDelegate = {
    let callbacks = DataWebSocketDelegate.Callbacks(
      onOpen: { [weak self] in
        Task { await self?.dataWebSocketOnOpen() }
      },
      onClose: { [weak self] closeCode, reason in
        Task { await self?.webSocketOnClose(closeCode: closeCode, reason: reason) }
      },
      onError: { [weak self] error in
        Task { await self?.webSocketOnError(error) }
      }
    )
    return DataWebSocketDelegate(callbacks: callbacks)
  }()
  
  private func dataWebSocketOnOpen() async {
    print("WebSocket connection opened")
    await handleConnectionEstablished()
  }
  
  private func webSocketOnClose(closeCode: URLSessionWebSocketTask.CloseCode, reason: Data?) async {
    print("WebSocket connection closed with code: \(closeCode)")
    guard closeCode != .normalClosure else {
      return
    }
    await handleUnexpectedDisconnection()
  }

  private func webSocketOnError(_ error: Swift.Error?) async {
    // alwasy retry connection for now, after delay
    // monitor app in for reasons when this wouldn't be a good idea
    try? await Task.sleep(for: .seconds(3))
    updateConnectionState(.disconnected)
    await handleUnexpectedDisconnection()
    
    if let error = error as NSError? {
      if error.domain == NSURLErrorDomain {
        switch error.code {
        case NSURLErrorTimedOut: // -1001
          print("Connection timed out")
        case NSURLErrorNotConnectedToInternet: // -1009
          print("Not connected to internet")
        default:
          print("Other error occurred: \(error.localizedDescription)")
          // The full error object will contain the detailed information you showed
          // including the failing URL and task information
        }
      }
    }
  }

  // MARK: - Internal connection handlers
    
  private func handleConnectionEstablished() async {
    onOpenTask?.cancel()
    onOpenTask = nil
    onOpenTask = Task {
      retryCount = 0 // Reset retry count on successful connection
      print("WebSocket connection and groups setup completed")
      updateConnectionState(.connected)
      await sendPing()
    }
  }
    
  private func handleUnexpectedDisconnection() async {
    onOpenTask?.cancel()
    guard connectionState != .connecting else { return }
    updateConnectionState(.disconnected)
    await reconnect()
  }
    
  private func reconnect() async {
    retryCount += 1
    print("Attempting reconnection #\(retryCount)")
        
    try? await Task.sleep(for: .seconds(5))
    guard !Task.isCancelled else {
      return
    }
    await connect()
  }
  
  private func waitForWebSocketTaskToClose(_ webSocketTask: URLSessionWebSocketTask) async {
    await withCheckedContinuation { continuation in
      Task {
        while webSocketTask.state != .completed {
          try? await Task.sleep(for: .milliseconds(100))
        }
        continuation.resume()
      }
    }
    print("WebSocket task closed successfully")
  }
  
  // MARK: - Ping Pong

  private let pingInterval: TimeInterval = 20 // 20 seconds
  private let pongTimeout: TimeInterval = 10 // 10 seconds timeout for pong response
 
  private var isPinging = false
  private var shouldPing = true
  private var didPong = false
  private func setDidPong(_ didPong: Bool) {
    self.didPong = didPong
  }
  
  private func sendPing() async {
    guard let webSocketTask else {
      return
    }
    guard isPinging == false else { return }
    isPinging = true
    defer {
      isPinging = false
      setDidPong(false)
    }

    while !Task.isCancelled {
      try? await Task.sleep(for: .seconds(pingInterval))
      setDidPong(false)
      if !shouldPing {
        shouldPing = true
        continue
      }
        
      webSocketTask.sendPing { _ in
        Task {
          await self.setDidPong(true)
        }
      }
      try? await Task.sleep(for: .seconds(pongTimeout))
        
      guard didPong else {
        await handleUnexpectedDisconnection()
        continue
      }
    }
  }
  
  // MARK: - Connection Actions
    
  @discardableResult
  func connect() async -> AsyncStream<ConnectionState> {
    let state = monitorConnectionState()
    guard connectionState == .disconnected else { return state }
    updateConnectionState(.connecting)
    
    if webSocketTask != nil {
      await disconnect()
    }
        
    var urlRequest = URLRequest(url: url)
    urlRequest.timeoutInterval = 5
            
    let webSocketTask = networkManager.session.webSocketTask(with: urlRequest)
    webSocketTask.delegate = delegate
    self.webSocketTask = webSocketTask
    
    webSocketTask.resume()
    return state
  }
    
  func disconnect() async {
    guard let webSocketTask = webSocketTask else { return }
    self.webSocketTask = nil
    onOpenTask?.cancel()
    onOpenTask = nil
    webSocketTask.cancel(with: .normalClosure, reason: nil)
    await waitForWebSocketTaskToClose(webSocketTask)
    updateConnectionState(.disconnected)
  }
    
  // MARK: - Connection State
  
  private var stateContinuations: [UUID: AsyncStream<ConnectionState>.Continuation] = [:]
  
  private func setStateContinuation(id: UUID, continuation: AsyncStream<ConnectionState>.Continuation?) {
    stateContinuations[id] = continuation
  }
  
  private func updateConnectionState(_ newState: ConnectionState) {
    connectionState = newState
    for continuation in stateContinuations.values {
      continuation.yield(newState)
    }
  }
  
  /// Starts monitoring the WebSocket connection state.
  ///
  /// This method returns an `AsyncStream` that can be used to monitor changes to the WebSocket connection state using Swift concurrency.
  /// Continuations are managed internally to track state changes.
  ///
  /// Example usage:
  /// ```swift
  /// for await state in dataWebSocket.monitorConnectionState() {
  ///   print("WebSocket connection state changed to: \(state)")
  /// }
  /// ```
  ///
  /// - Returns: an async stream that represents the up-to-date ConnectionState
  func monitorConnectionState() -> AsyncStream<ConnectionState> {
    return AsyncStream { [weak self] continuation in
      guard let self = self else { return }

      let id = UUID()
      Task {
        await self.setStateContinuation(id: id, continuation: continuation)
        let state = await self.connectionState
        continuation.yield(state)
      }

      continuation.onTermination = { @Sendable [weak self] _ in
        guard let self = self else { return }
        Task {
          await self.setStateContinuation(id: id, continuation: nil)
        }
      }
    }
  }
  
  // MARK: - Send Data
  
  func send(data: Data) async throws {
    let message = URLSessionWebSocketTask.Message.data(data)
    try await webSocketTask?.send(message)
  }

  func send(message: String) async throws {
    let taskMessage = URLSessionWebSocketTask.Message.string(message)
    try await webSocketTask?.send(taskMessage)
  }
    
  // MARK: - Recieve Data
  
  func receive() async throws -> URLSessionWebSocketTask.Message? {
    let message = try await webSocketTask?.receive()
    shouldPing = false
    return message
  }
}


// purely a wrapper for old NSObject delegate
final class DataWebSocketDelegate: NSObject, URLSessionWebSocketDelegate, URLSessionTaskDelegate, Sendable {
  struct Callbacks: Sendable {
    let onOpen: @Sendable () -> Void
    let onClose: @Sendable (URLSessionWebSocketTask.CloseCode, Data?) -> Void
    let onError: @Sendable (Error?) -> Void
  }
    
  let callbacks: Callbacks
  init(callbacks: Callbacks) {
    self.callbacks = callbacks
  }

  func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask, didOpenWithProtocol protocol: String?) {
    callbacks.onOpen()
  }
    
  func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask, didCloseWith closeCode: URLSessionWebSocketTask.CloseCode, reason: Data?) {
    callbacks.onClose(closeCode, reason)
  }
  
  func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: (any Error)?) {
    callbacks.onError(error)
  }
}
