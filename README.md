#  Roomba

## ESP Roomba

ESP32 S3 code to control the roomba motors and stream an ov2640 or ov5640 camera. I'm using a Seeed XIAO ESP32S3 Sense board.

It sets up a wifi access point and a websocket server to stream the camera feed and send motor commands. 

## iOS App

The iOS code is a client to the ESP web server. Connect to the ESP's wifi access point and use the app to control the robot.

The app uses the game controller framework for input then sends the motor commands to the ESP over a websocket connection. It also displays the camera feed from the ESP on the screen.


[![Watch the video](https://img.youtube.com/vi/VIDEO_ID/0.jpg)](https://github.com/user-attachments/assets/0923fefa-4415-4123-b25e-d2e38a8e5d2b)
