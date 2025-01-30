from utime import sleep_ms
from microdot import Microdot, websocket, Response
import struct
from motor import Motor
from wifi import connect_wifi
import uasyncio as asyncio
from Camera.ArduCam import ArduCam

app = Microdot()
camera = ArduCam()

@app.route('/')
async def index(request):
    return 'Pico Web Server!'


@app.route('/jpeg')
async def jpeg(request):
    print("jpeg")
    try:
        jpg_data = camera.single_capture()
        if jpg_data:
            return Response(
            body=jpg_data,  
            headers={
                'Content-Type': 'image/jpeg',
                'Access-Control-Allow-Origin': '*'
            }
        )
        else:
            print("Capture failed.")
            return "for fox creek"
        
    except Exception as e:
        print("Error capturing JPEG:", e)
        return Response("Error capturing JPEG", status_code=500)
    
    
    
    
@app.route('/stream')
async def video_feed(request):
    print('Starting camera stream')
    
    class stream:
        def __init__(self):
            self.running = True
            
        def __aiter__(self):
            return self
            
        async def __anext__(self):
            await asyncio.sleep_ms(1)  # Adjust frame rate as needed
            try:
                jpg_data = camera.single_capture()
                if jpg_data:
                    return b'Content-Type: image/jpeg\r\n\r\n' + jpg_data + b'\r\n--frame\r\n'
                return b''  # Return empty frame if capture failed
            except Exception as e:
                print("Frame capture error:", e)
                return b''
                
        async def aclose(self):
            print('Stopping camera stream')
            self.running = False

    return stream(), 200, {'Content-Type': 'multipart/x-mixed-replace; boundary=frame'}
  
right_motor = Motor(19, 18, 17) 
left_motor = Motor(20, 21, 22) 
brush_motor = Motor(15, 14, 13) 
vac_motor = Motor(10, 11, 12) 

def stop_motors():
    """Stop all motors"""
    left_motor.stop()
    right_motor.stop()
    brush_motor.stop()
    vac_motor.stop()
    
connected_client: None | websocket.WebSocket = None  # Single websocket client connection
    
@app.route('/motor_control')
async def motor_control(request):
    try: 
        ws = await websocket.websocket_upgrade(request)
        global connected_client
        connected_client = ws
        while True:
            message = await ws.receive()
            print(message)
            
            if message == 'ping':
                await ws.send('pong')
                continue
                
            if not isinstance(message, bytes):
                print("Not bytes")
                continue
            try:
              speeds = struct.unpack('4b', message)
              left_speed, right_speed, vac_speed, brush_speed = speeds
                
              left_motor.update_speed(left_speed)
              right_motor.update_speed(right_speed)
              vac_motor.update_speed(vac_speed)
              brush_motor.update_speed(brush_speed)
            except Exception as e:
              print(f"Error parsing message: {e}")
              # Just ignore any parsing errors and continue
              continue
    except Exception as e:
        print(f'Error: {e}')
        stop_motors()  # Stop motors on any websocket error
       


async def websocket_broadcaster():
    global connected_client
    while True:
        try:
            if connected_client:
                jpg_data = camera.single_capture()
                if jpg_data:
                    # Send the bytes. Some libraries require you to do e.g.:
                    await connected_client.send(jpg_data, connected_client.BINARY)
            await asyncio.sleep_ms(10)
        except Exception as e:
            print(f"Broadcaster error: {e}")
            await asyncio.sleep_ms(100)  # Longer delay on er
        
def startup():
    try:
        import config
        connect_wifi(config.WIFI_SSID, config.WIFI_PASSWORD)
              # Initialize camera with double buffers
        camera.init_ov2640_jpeg_320x240()
        sleep_ms(200)  # Small delay after connection
 
        asyncio.create_task(websocket_broadcaster())
        app.run(port=config.SERVER_PORT)
        return True
    except Exception as e:
        print(f"WiFi connection error: {e}")
        return False
    
def main():
    while True:
        try:
            if not startup():
                print("Startup failed, retrying in 5 seconds...")
                sleep_ms(5000)
        except Exception as e:
            print(f"Server error: {e}")
        finally:
            stop_motors()
        
        sleep_ms(1000)  # Delay before retry

if __name__ == '__main__':
    main()