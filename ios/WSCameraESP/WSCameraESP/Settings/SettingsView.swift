import SwiftUI

import SwiftUI

struct PageData: Identifiable, Hashable {
  enum Page: String, Identifiable, Codable, Equatable, Hashable {
    var id: String {
      rawValue
    }

    case generalSettings
    case roombaSettings
  }

  let id: Page
  let icon: String
  let title: String
}

struct NavigationRow: View {
  let page: PageData

  var body: some View {
    HStack {
      Image(systemName: page.icon)
      Text(page.title)
      Spacer()
      Image(systemName: "chevron.right")
        .foregroundColor(.gray)
    }
  }
}

struct SettingsView: View {
  @Bindable private var viewModel = SettingsViewModel()

  private func pagesData(_ page: PageData.Page) -> PageData {
    switch page {
    case .generalSettings:
      return PageData(id: .generalSettings, icon: "chart.bar.fill", title: "General Stats")
    case .roombaSettings:
      return PageData(id: .roombaSettings, icon: "circle.circle", title: "Roomba Settings")
    }
  }

  let pages: [PageData.Page] = [.generalSettings, .roombaSettings]
  @State private var selectedPage: PageData.Page? = .generalSettings

  var body: some View {
    GeometryReader { geometry in
      HStack(spacing: 0) {
        NavigationStack {
          List(pages, id: \.self, selection: $selectedPage) { page in
            NavigationRow(page: pagesData(page))
          }
        }
        .frame(width: geometry.size.width * 0.35) // 35% for the list

        Divider()

        VStack {
          Spacer()
          switch selectedPage {
          case .generalSettings:
            GeneralSettingsView().environment(viewModel)
          case .roombaSettings:
            RoombaSettingsView().environment(viewModel)
          case .none:
            EmptyView()
          }
          Spacer()
        }
        .background(Color(.systemBackground))
        .frame(width: geometry.size.width * 0.65) // 65% for the detail view
      }
    }
    .alert(
      viewModel.errorDetails?.title ?? "",
      isPresented: $viewModel.didError,
      presenting: viewModel.errorDetails
    ) { _ in
      // buttons
      Button("Done") {
        // Handle something at some point
      }
    } message: { details in
      Text(details.message)
    }
    .alert(viewModel.alertDetails?.title ?? "", isPresented: $viewModel.shouldAlert, presenting: viewModel.alertDetails, actions: { _ in
      EmptyView()
    }, message: { details in
      Text(details.message)
    })
  }
}

struct ConnectionView: View {
  var body: some View {
    Text("Connection Settings")
      .frame(maxWidth: .infinity, maxHeight: .infinity)
      .background(Color.blue.opacity(0.2))
  }
}

#Preview(traits: .landscapeRight) {
  SettingsView()
}
