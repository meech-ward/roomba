import network
from time import sleep

def connect_wifi(ssid, password):
    """Connect to WiFi"""
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(ssid, password)
    
    # Wait for connection
    attempts = 0
    while not wlan.isconnected():
        print(f'Attempting to connect to WiFi... (attempt {attempts + 1})')
        sleep(5)
        attempts += 1
    print(f'Connected to WiFi\nIP: {wlan.ifconfig()[0]}')
