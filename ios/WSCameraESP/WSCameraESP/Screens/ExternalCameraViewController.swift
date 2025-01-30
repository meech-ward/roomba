//
//  ExternalCameraViewController.swift
//  Eye Camera UIKit
//
//  Created by Sam Meech-Ward on 2024-08-13.
//

import AVFoundation
import MightFail
import SwiftUI
import UIKit

class ExternalCameraViewController: UIViewController {
  var previewImagesButtonVC: UIViewController!
  var roombaService: RoombaService!

  @IBOutlet var wsConnectionButton: UIButton!
  @IBOutlet var imageView: UIImageView!
  @IBOutlet var fpsLabel: UILabel!

  private var streamingTask: Task<Void, Never>?

  override func viewDidLoad() {
    super.viewDidLoad()

    overrideUserInterfaceStyle = .dark

    view.addGestureRecognizer(UITapGestureRecognizer(target: self, action: #selector(tap)))

    setupWSConnectionButton()
    setupStreaming()
    setupFPSStreaming()
  }

  private func setupWSConnectionButton() {
    Task {
      for await state in self.roombaService.connectionState.stream() {
        let systemImageName: String
        switch state {
        case .connected:
          systemImageName = "checkmark.icloud.fill"
        case .connecting:
          systemImageName = "arrow.clockwise.icloud.fill"
        case .disconnected:
          systemImageName = "xmark.icloud.fill"
        }
        await MainActor.run {
          self.wsConnectionButton.setImage(UIImage(systemName: systemImageName), for: .normal)
        }
      }
    }
  }

  private func setupStreaming() {
    streamingTask = Task.detached {
      for await data in await self.roombaService.monitorDataStream() {
        let image = UIImage(data: data)
        await MainActor.run {
          self.imageView.image = image
        }
      }
    }
  }
  
  private func setupFPSStreaming() {
    Task {
      for await fps in self.roombaService.fps.stream() {
        await MainActor.run { fpsLabel.text = "\(String(format: "%.2f", fps)) fps" }
      }
    }
  }

  var config: UIContentUnavailableConfiguration = .empty()
  override func updateContentUnavailableConfiguration(
    using state: UIContentUnavailableConfigurationState
  ) {
    contentUnavailableConfiguration = config
  }

  @IBAction func sliderValueChanged(_ sender: UISlider) {
    roombaService.vaccumSpeed.update(Int8(sender.value))
  }

  // MARK: - Take Photo

  private func takePhoto() async throws {
    print("takePhoto to be implemented")
    try? await Task.sleep(nanoseconds: 1_000_000_000)
  }

  @objc func tap() {
    Task {
      defer {
        imageView.isHidden = false
        config = UIContentUnavailableConfiguration.empty()
        setNeedsUpdateContentUnavailableConfiguration()
      }
      imageView.isHidden = true
      config = UIContentUnavailableConfiguration.loading()
      setNeedsUpdateContentUnavailableConfiguration()
      let (error, _, success) = await mightFail { try await takePhoto() }
      guard success else {
        print("error capturing photo \(error)")
        showErrorAlert(message: "could not capture photo")
        return
      }
    }
  }
}
