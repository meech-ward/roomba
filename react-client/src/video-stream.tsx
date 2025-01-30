import { useState, useEffect, useRef } from "react";

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export function VideoStream({ imageData }: { imageData: any }) {
  const [imageUrl, setImageUrl] = useState<string | undefined>(undefined);
  const [fps, setFps] = useState(0);
  const frameTimestamps = useRef<number[]>([]);
  const fpsUpdateInterval = useRef<number | null>(null);

  // Setup periodic FPS calculation
  useEffect(() => {
    const calculateFps = () => {
      const now = performance.now();
      const oneSecondAgo = now - 1000;

      // Remove old timestamps and keep only last 60 frames
      frameTimestamps.current = frameTimestamps.current
        .filter(t => t > oneSecondAgo)
        .slice(-60);

      if (frameTimestamps.current.length >= 2) {
        // Calculate average time between frames
        const timeDeltas = [];
        for (let i = 1; i < frameTimestamps.current.length; i++) {
          timeDeltas.push(frameTimestamps.current[i] - frameTimestamps.current[i - 1]);
        }

        const averageTimeDelta = timeDeltas.reduce((a, b) => a + b, 0) / timeDeltas.length;
        const calculatedFps = 1000 / averageTimeDelta;

        setFps(Number(calculatedFps.toFixed(1)));
      } else {
        setFps(0);
      }
    };

    fpsUpdateInterval.current = window.setInterval(calculateFps, 500);

    return () => {
      if (fpsUpdateInterval.current) {
        clearInterval(fpsUpdateInterval.current);
      }
    };
  }, []);

  useEffect(() => {
    // Create a URL for the new blob
    

    setImageUrl(oldUrl => {
      if (oldUrl) {
        URL.revokeObjectURL(oldUrl);
      }
      if (imageData) {
        const newUrl = URL.createObjectURL(imageData);
        return newUrl;
      }
      return undefined;
    });

    // Store frame timestamp
    frameTimestamps.current.push(performance.now());
  }, [imageData]);

  // Cleanup the object URL when component unmounts
  useEffect(() => {
    return () => {
      if (imageUrl) {
        URL.revokeObjectURL(imageUrl);
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <div>
      <div>FPS: {fps.toFixed(1)}</div>
      <img src={imageUrl} alt="Video Stream" />
    </div>
  );
}