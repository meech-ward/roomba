import MightFail
import SwiftUI

struct GeneralSettingsView: View {
  private var roomba: RoombaService = .shared
  @State private var logs = BoundedDeque<Log>(maxSize: 30)

  var body: some View {
    List {
      Section(header: Text("Connection")) {
        StatRow(title: "WebSocket", value: "\(roomba.connectionState.observable.value)")
      }

      Section(header: Text("Streaming")) {
        StatRow(title: "Is streaming", value: "\(roomba.isStreaming.observable.value)")
        Button("Start Sreaming") {
          Task {
            await self.roomba.startStream()
          }
        }
        Button("Stop Sreaming") {
          Task {
            await self.roomba.stopStream()
          }
        }

        Button("Start Sending Motor Data") {
          Task {
            await self.roomba.startSendingMotorData()
          }
        }

        Button("Stop Sending Motor Data") {
          Task {
            await self.roomba.stopSendingMotorData()
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
      for await log in  roomba.logs.stream() {
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
