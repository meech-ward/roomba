import MightFail
import SwiftUI

struct GeneralSettingsView: View {
  private var repository: Repository = .shared
  @State private var logs = BoundedDeque<Log>(maxSize: 30)

  var body: some View {
    List {
      Section(header: Text("Connection")) {
        StatRow(title: "WebSocket", value: "\(repository.connectionState.observable.value)")
      }

      Section(header: Text("Streaming")) {
        StatRow(title: "Is streaming", value: "\(repository.isStreaming.observable.value)")
        Button("Start Sreaming") {
          Task {
            await self.repository.startStream()
          }
        }
        Button("Stop Sreaming") {
          Task {
            await self.repository.stopStream()
          }
        }

        Button("Start Sending Motor Data") {
          Task {
            await self.repository.startSendingMotorData()
          }
        }

        Button("Stop Sending Motor Data") {
          Task {
            await self.repository.stopSendingMotorData()
          }
        }
      }

      Section(header: Text("Logs")) {
        ForEach(logs.allItems()) { log in
          Text("\(log.message)")
        }
      }
    }
    .listStyle(GroupedListStyle())
    .navigationTitle("General Stats")
    .task {
      for await log in  repository.logs.stream() {
        logs.add(log)
      }
    }
  }
}

struct StatRow: View {
  let title: String
  let value: String

  var body: some View {
    HStack {
      Text(title)
      Spacer()
      Text(value)
        .foregroundColor(.secondary)
    }
  }
}
