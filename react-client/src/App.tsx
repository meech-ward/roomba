import ConnectionIndicator from './connection-indicator';
import MotorSpeedControl from './motor-speed-control'
import { useCustomWebSocket } from './lib/useWebSocket';
import { VideoStream } from './video-stream';
import { useState } from 'react';
export default function App() {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const [imageData, setImageData] = useState<any>(null);

  const {
    isConnected,
    error,
    sendBinaryData,
    reconnectAttempt,
  } = useCustomWebSocket('ws://10.0.0.212/motor_control', {
    reconnectAttempts: 10,
    reconnectInterval: 3000,
    enablePingPong: true,
    pingIntervalMs: 5000,
    pongTimeoutMs: 5000,
    shouldReconnect: () => true,
    onMessage: (event) => {
      console.log(event)
      setImageData(event.data)
    }
  });


  const handleSpeedChange = (speed: number) => {
    console.log(speed)
    sendBinaryData([speed, speed, speed, speed]);
  }

  return (
    <main className="flex min-h-screen flex-col items-center justify-center p-24">
      <VideoStream imageData={imageData} />
      <div className="mb-4">
        <ConnectionIndicator isConnected={isConnected} error={error} reconnectAttempt={reconnectAttempt} />
      </div>
      <MotorSpeedControl onSpeedChange={handleSpeedChange} />
    </main>
  )
}