import MightFail
import Parchment
import UIKit

class MainPageViewController: UIViewController {
  private var scrollView = UIScrollView()

  let roomba = RoombaService.shared
  override var prefersHomeIndicatorAutoHidden: Bool {
    return true
  }

  override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge {
    return [.top, .left, .right]
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    UIApplication.shared.isIdleTimerDisabled = true

    scrollView.frame = view.bounds
    scrollView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    scrollView.contentSize = view.bounds.size
    scrollView.alwaysBounceVertical = true // Enable vertical bouncing for pull-to-refresh
    scrollView.isScrollEnabled = false // Disable scrolling
    view.addSubview(scrollView)

    let leftVC = UIStoryboard(name: "Settings", bundle: nil).instantiateInitialViewController() as! SettingsViewController

    let rightVC = UIStoryboard(name: "ExternalCamera", bundle: nil).instantiateInitialViewController() as! ExternalCameraViewController
    rightVC.roombaService = RoombaService.shared

    let pages = [leftVC, rightVC]

    var pagingOptions = PagingOptions()
    pagingOptions.menuPosition = .top
    pagingOptions.indicatorOptions = .hidden
    pagingOptions.menuItemSize = .sizeToFit(minWidth: 0, height: 0)

    let pagingViewController = PagingViewController(options: pagingOptions, viewControllers: pages)

    pagingViewController.select(index: 1, animated: false)
    pagingViewController.pageViewController.scrollView.bounces = false
    pagingViewController.pageViewController.view.backgroundColor = .black

    addFullScreenSubViewController(pagingViewController)

    Task {
      await configureNetwork()
      await roomba.startGameController()
    }
  }

  func configureNetwork() async {
//    let url = URL(string: "ws://10.0.0.215/ws")!
//    let url = URL(string: "ws://10.0.0.35/ws")!
//    let url = URL(string: "ws://10.0.0.226/ws")!
    let url = URL(string: "ws://10.0.0.250/ws")!

//    let url = URL(string: "ws://10.0.0.222/motor_control")!
    let (error, _, success) = await mightFail { try await roomba.configure(withUrl: url) }

    guard success else {
      showErrorAlert(message: "Error configuring network: \(error.localizedDescription)")
      return
    }

    print("configured roomba")
  }
}
