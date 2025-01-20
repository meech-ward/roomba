
import Foundation

@Observable
@MainActor
final class SettingsViewModel {
  struct ErrorDetails {
    let destructiveTitle: String
    let message: String
    let title: String
  }

  var didError: Bool = false
  var errorDetails: ErrorDetails?

  func showError(title: String, message: String) {
    didError = true
    errorDetails = ErrorDetails(destructiveTitle: "", message: message, title: title)
  }

  struct AlertDetails {
    let title: String
    let message: String
  }

  var shouldAlert: Bool = false
  var alertDetails: AlertDetails?
  func showAlert(title: String, message: String) {
    shouldAlert = true
    alertDetails = AlertDetails(title: title, message: message)
  }
}
