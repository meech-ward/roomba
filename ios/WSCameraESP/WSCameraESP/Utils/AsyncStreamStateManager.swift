import Foundation

/// A thread-safe state management system that provides both async stream-based observation
/// and SwiftUI/UIKit compatible state observation through the new Swift observation framework.
///
/// `AsyncStreamStateManager` combines actor isolation for thread safety with MainActor-bound observable
/// state for UI updates. It allows multiple observation patterns:
/// - Async stream subscription for value updates
/// - Direct observable state binding for SwiftUI/UIKit
///
/// Example usage:
/// ```swift
/// // Create a manager
/// let manager = await AsyncStreamStateManager(0)
///
/// // Observe via async stream
///
///   for await value in await manager.stream() {
///       print("New value:", value)
///   }
///
///
/// // Bind to SwiftUI
/// struct ContentView: View {
///     var manager: AsyncStreamStateManager<Int>
///
///     var body: some View {
///         Text("\(manager.observable.value)")
///     }
/// }
///
/// // Update value
/// await manager.update(42)
/// ```
public actor AsyncStreamStateManager<T: Sendable> {
  /// An Observable class that wraps the managed value for SwiftUI/UIKit integration.
  /// This class is bound to the MainActor to ensure all UI updates happen on the main thread.
  @Observable
  @MainActor
  public class State {
    /// The current value, only settable internally but observable externally
    public internal(set) var value: T
        
    init(value: T) {
      self.value = value
    }
  }
    
  /// The observable state object for SwiftUI/UIKit integration.
  /// This property is accessed on the MainActor to ensure thread-safe UI updates.
  @MainActor
  public var observable: State!
    
  /// Storage for active stream continuations, keyed by UUID to allow multiple observers
  private var continuations: [UUID: AsyncStream<T>.Continuation] = [:]
    
  /// The current value managed by this instance.
  /// Updates to this value are automatically propagated to all observers.
  public private(set) var value: T
    
  /// Task that synchronizes the observable state with the current value
  private var updateStateTask: Task<Void, Never>?
    
  /// Sets up the task that keeps the observable state in sync with the current value
  private func setUpdateStateTask() {
    updateStateTask = Task { @MainActor in
      for await value in await stream() {
        observable.value = value
      }
    }
  }
    
  /// Creates a new state manager with the given initial value.
  /// This initializer can be called from any context and will properly set up
  /// the MainActor-bound observable state.
  ///
  /// - Parameter initialValue: The initial value to manage
  public init(_ initialValue: T) async {
    value = initialValue
    await MainActor.run {
      observable = .init(value: initialValue)
    }
    setUpdateStateTask()
  }
    
  /// Creates a new state manager with the given initial value when already on the MainActor.
  /// This convenience initializer avoids the need for async/await when initializing from UI code.
  ///
  /// - Parameter initialValue: The initial value to manage
  @MainActor
  public init(_ initialValue: T) {
    value = initialValue
    observable = .init(value: initialValue)
    Task {
      await setUpdateStateTask()
    }
  }
    
  deinit {
    updateStateTask?.cancel()
  }
    
  /// Updates the managed value and propagates the change to all observers.
  ///
  /// - Parameter value: The new value to set
  public func update(_ value: T) {
    self.value = value
    for continuation in continuations.values {
      continuation.yield(value)
    }
  }
    
  /// Internal helper to manage stream continuations
  private func setContinuation(id: UUID, continuation: AsyncStream<T>.Continuation?) {
    if let continuation {
      continuations[id] = continuation
    } else {
      continuations.removeValue(forKey: id)
    }
  }
    
  /// Creates an AsyncStream that will receive all value updates.
  /// The stream immediately yields the current value and then yields all subsequent updates.
  ///
  /// - Returns: An AsyncStream of values that can be used with async/await code
  public func stream() -> AsyncStream<T> {
    AsyncStream { [weak self] continuation in
      guard let self = self else { return }
            
      let id = UUID()
      Task {
        await self.setContinuation(id: id, continuation: continuation)
        continuation.yield(await self.value)
      }
            
      continuation.onTermination = { @Sendable [weak self] _ in
        guard let self = self else { return }
        Task {
          await self.setContinuation(id: id, continuation: nil)
        }
      }
    }
  }
}
