"use client"

import { useState } from "react"
import { Slider } from "@/components/ui/slider"
import { Button } from "@/components/ui/button"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { RotateCcw, RotateCw, RotateCcwIcon } from "lucide-react"

export default function MotorSpeedControl({ onSpeedChange }: { onSpeedChange: (speed: number) => void }) {
  const [speed, setSpeed] = useState(0)

  const handleSpeedChange = (newSpeed: number[]) => {
    setSpeed(newSpeed[0])
    onSpeedChange(newSpeed[0])
  }

  const adjustSpeed = (adjustment: number) => {
    setSpeed((prevSpeed) => Math.max(-100, Math.min(100, prevSpeed + adjustment)))
  }

  const getSpeedColor = () => {
    if (speed === 0) return "text-gray-500"
    return speed > 0 ? "text-green-500" : "text-red-500"
  }

  return (
    <Card className="w-full max-w-md mx-auto">
      <CardHeader>
        <CardTitle className="text-2xl font-bold text-center">Motor Speed Control</CardTitle>
      </CardHeader>
      <CardContent className="space-y-6">
        <div className="flex justify-center">
          <div className="relative">
            <RotateCw
              size={120}
              className={`${getSpeedColor()} transition-all duration-300 ease-out`}
              style={{
                opacity: Math.abs(speed) / 100,
                transform: `rotate(${speed * 1.8}deg)`,
              }}
            />
            <div className="absolute top-1/2 left-1/2 transform -translate-x-1/2 -translate-y-1/2 text-2xl font-bold">
              {Math.abs(speed)}%
            </div>
          </div>
        </div>
        <div className="text-center text-xl font-semibold">
          {speed === 0 ? "Stopped" : speed > 0 ? "Forward" : "Reverse"}
        </div>
        <Slider value={[speed]} onValueChange={handleSpeedChange} min={-100} max={100} step={1} className="w-full" />
        <div className="flex justify-between gap-2">
          <Button onClick={() => adjustSpeed(-10)} variant="outline" size="sm">
            -10%
          </Button>
          <Button onClick={() => adjustSpeed(-1)} variant="outline" size="sm">
            -1%
          </Button>
          <Button onClick={() => setSpeed(0)} variant="outline" size="sm">
            <RotateCcw className="mr-2 h-4 w-4" /> Stop
          </Button>
          <Button onClick={() => adjustSpeed(1)} variant="outline" size="sm">
            +1%
          </Button>
          <Button onClick={() => adjustSpeed(10)} variant="outline" size="sm">
            +10%
          </Button>
        </div>
        <div className="flex justify-center gap-4">
          <Button onClick={() => setSpeed(-100)} variant="outline">
            <RotateCcwIcon className="mr-2 h-4 w-4" /> Max Reverse
          </Button>
          <Button onClick={() => setSpeed(100)} variant="outline">
            <RotateCw className="mr-2 h-4 w-4" /> Max Forward
          </Button>
        </div>
      </CardContent>
    </Card>
  )
}

