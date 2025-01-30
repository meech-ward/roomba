import SwiftUI

struct CrashAnimationView: View {
  let roombaService: RoombaService
    
  @State private var leftBumperWasPressed = false
  @State private var rightBumperWasPressed = false
  @State private var leftCrashTrigger = false
  @State private var rightCrashTrigger = false
    
  var body: some View {
    GeometryReader { geometry in
      let fullWidth = UIScreen.main.bounds.width  // Get full device width
      let halfWidth = fullWidth / 2
      
      ZStack {
        // Left crash overlay
        Color.red
          .opacity(leftCrashTrigger ? 0.5 : 0)
          .frame(width: halfWidth)
          .frame(maxWidth: .infinity, alignment: .leading)
          .animation(.easeOut(duration: 0.3), value: leftCrashTrigger)
                
        // Right crash overlay
        Color.red
          .opacity(rightCrashTrigger ? 0.5 : 0)
          .frame(width: halfWidth)
          .frame(maxWidth: .infinity, alignment: .trailing)
          .animation(.easeOut(duration: 0.3), value: rightCrashTrigger)
          
        // Motor current labels
        VStack {
          Spacer()
          HStack {
            Text("\(roombaService.sensors.observable.value.leftMotorCurrent)mA")
              .padding()
              .foregroundColor(.white)
            Spacer()
            Text("\(roombaService.sensors.observable.value.rightMotorCurrent)mA")
              .padding()
              .foregroundColor(.white)
          }
        }
      }
      .ignoresSafeArea()
    }
    .background(Color.clear)
    .onChange(of: roombaService.sensors.observable.value.leftBumperPressed) { _, isPressed in
      if isPressed && !leftBumperWasPressed {
        leftBumperWasPressed = true
        leftCrashTrigger = true
        // Auto-reset after animation
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
          leftCrashTrigger = false
        }
      } else if !isPressed {
        leftBumperWasPressed = false
      }
    }
    .onChange(of: roombaService.sensors.observable.value.rightBumperPressed) { _, isPressed in
      if isPressed && !rightBumperWasPressed {
        rightBumperWasPressed = true
        rightCrashTrigger = true
        // Auto-reset after animation
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
          rightCrashTrigger = false
        }
      } else if !isPressed {
        rightBumperWasPressed = false
      }
    }
  }
}
