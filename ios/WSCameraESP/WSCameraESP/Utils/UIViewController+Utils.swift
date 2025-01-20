//
//  UIViewController+Utils.swift
//  Eye Camera UIKit
//
//  Created by Sam Meech-Ward on 2024-08-16.
//

import Foundation
import UIKit

// MARK: - Alert views

extension UIViewController {
  func showErrorAlert(message: String, action: UIAlertAction? = nil) {
    // Create the alert controller
    let alertController = UIAlertController(title: "Error", message: message, preferredStyle: .alert)

    // Add actions (buttons)
    let cancelAction = UIAlertAction(title: "Dismiss", style: .cancel, handler: nil)

    // Add the actions to the alert controller
    alertController.addAction(cancelAction)
    if let action {
      alertController.addAction(action)
    }
    // Present the alert controller
    present(alertController, animated: true, completion: nil)
  }

  func showAlert(title: String, message: String) {
    // Create the alert controller
    let alertController = UIAlertController(title: title,
                                            message: message,
                                            preferredStyle: .alert)

    // Add actions (buttons)
    let cancelAction = UIAlertAction(title: "Ok", style: .cancel, handler: nil)
    // Add the actions to the alert controller
    alertController.addAction(cancelAction)

    // Present the alert controller
    present(alertController, animated: true, completion: nil)
  }

  func showConfirmAlert(title: String, message: String, confirmTitle: String = "OK", cancelTitle: String = "Cancel") async -> Bool {
    return await withCheckedContinuation { continuation in
      let alertController = UIAlertController(title: title, message: message, preferredStyle: .alert)

      let confirmAction = UIAlertAction(title: confirmTitle, style: .default) { _ in
        continuation.resume(returning: true)
      }

      let cancelAction = UIAlertAction(title: cancelTitle, style: .cancel) { _ in
        continuation.resume(returning: false)
      }

      alertController.addAction(confirmAction)
      alertController.addAction(cancelAction)

      self.present(alertController, animated: true, completion: nil)
    }
  }
}

// MARK: - Child view controllers

extension UIViewController {
  func addFullScreenSubViewController(_ viewController: UIViewController) {
    addChild(viewController)
    view.addSubview(viewController.view)
    viewController.didMove(toParent: self)

    viewController.view.translatesAutoresizingMaskIntoConstraints = false

    NSLayoutConstraint.activate([
      viewController.view.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      viewController.view.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      viewController.view.bottomAnchor.constraint(equalTo: view.bottomAnchor),
      viewController.view.topAnchor.constraint(equalTo: view.topAnchor)
    ])
  }
}
