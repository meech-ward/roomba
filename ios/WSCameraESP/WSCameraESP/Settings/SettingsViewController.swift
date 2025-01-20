//
//  DevicePairingViewController.swift
//  Eye Camera UIKit
//
//  Created by Sam Meech-Ward on 2024-08-13.
//

import SwiftUI
import UIKit

class SettingsViewController: UIViewController {
  override func viewDidLoad() {
    super.viewDidLoad()
    setupChildView()
  }

  func setupChildView() {
    let swiftUIView = SettingsView()
    let hostingController = UIHostingController(rootView: swiftUIView)
    hostingController.view.backgroundColor = .clear
    hostingController.view.clipsToBounds = true
    addFullScreenSubViewController(hostingController)
  }
}
