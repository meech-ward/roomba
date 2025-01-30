import MightFail
import SwiftUI

struct RoombaSettingsView: View {
  @Environment(SettingsViewModel.self) private var settingsViewModel: SettingsViewModel
  private var roomba: RoombaService = .shared
  @State private var inputText: String = ""
  @FocusState private var isInputFocused: Bool
    
  var body: some View {
    List {
      Section(header: Text("Mode")) {
        Button("Safe Mode") {
          handleAsyncTask { try await roomba.safeMode() }
        }
        Button("Full Mode") {
          handleAsyncTask { try await roomba.fullMode() }
        }
        Button("Start") {
          handleAsyncTask { try await roomba.startRoomba() }
        }
      }
      
      Section(header: Text("Display")) {
        HStack {
          TextField("Enter code", text: $inputText)
            .textFieldStyle(RoundedBorderTextFieldStyle())
            .frame(maxWidth: .infinity)
            .focused($isInputFocused)
            .onChange(of: inputText) { _, newValue in
              if newValue.count > 4 {
                inputText = String(newValue.prefix(4))
              }
            }
            .textInputAutocapitalization(.characters)
            .keyboardType(.asciiCapable)
                    
          Button(action: {
            submitText()
          }) {
            Text("Submit")
              .fontWeight(.medium)
              .frame(minWidth: 200)
          }
          .buttonStyle(.bordered)
          .tint(.blue)
        }
        .listRowInsets(EdgeInsets(top: 8, leading: 16, bottom: 8, trailing: 16))
      }
      Section("Songs") {
        Button("Play Song") {
          handleAsyncTask { try await roomba.playSong() }
        }
        Button("Play Song 2") {
          handleAsyncTask { try await roomba.playSong2() }
        }
        Button("Play Daft Punk") {
          handleAsyncTask { try await roomba.playPunk() }
        }
      }
      
      Section("Sensors") {
        Button("Stream Sensors") {
          handleAsyncTask {
            try await roomba.streamSensors(true)
          }
        }
        Button("Stop Stream Sensors") {
          handleAsyncTask { 
            try await roomba.streamSensors(false)
          }
        }
        
        Text(roomba.sensors.observable.value.description)
          .font(.system(.body, design: .monospaced))
          .frame(maxWidth: .infinity, alignment: .leading)
          .padding(.vertical, 8)
        
      }
    }
    .listStyle(GroupedListStyle())
    .navigationTitle("Roomba Settings")
//    .onTapGesture {
//      isInputFocused = false
//    }
  }
  
  private func handleAsyncTask(_ throwingFunction: @escaping @Sendable () async throws -> Sendable, completion: (() -> Void)? = nil) {
    Task {
      let (error, _, success) = await mightFail { try await throwingFunction() }
      guard success else {
        await MainActor.run {
          settingsViewModel.showError(title: "Error Sending", message: error.localizedDescription)
        }
        return
      }
      completion?()
    }
  }

  private func submitText() {
    isInputFocused = false
    handleAsyncTask({ try await roomba.updateDisplay(inputText) }, completion: {
      print("sent text '\(inputText)'")
    })
  }
}
