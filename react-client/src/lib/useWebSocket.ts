import { useRef, useState, useCallback } from "react";
import rawUseWebSocket, {
  ReadyState,
  Options,
  SendMessage,
} from "react-use-websocket";

/**
 * Custom connection states for easier UI handling.
 */
export enum CustomConnectionState {
  DISCONNECTED = "DISCONNECTED",
  CONNECTING = "CONNECTING",
  CONNECTED = "CONNECTED",
}

/**
 * Extend react-use-websocket's `Options` with optional ping/pong config.
 */
export interface UseCustomWebSocketConfig extends Options {
  enablePingPong?: boolean;
  pingIntervalMs?: number;  // default: 5000
  pongTimeoutMs?: number;   // default: 5000
}

/**
 * Return type for our custom hook.
 */
export interface UseCustomWebSocketReturn {
  connectionState: CustomConnectionState;
  isConnected: boolean;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  lastMessage: MessageEvent<any> | null;
  error: string | null;
  sendMessage: SendMessage;
  sendBinaryData: (numbers: number[]) => void;
  manualDisconnect: () => void;
  manualReconnect: () => void;
  reconnectAttempt: number;
}

export function useCustomWebSocket(
  socketUrl: string,
  config: UseCustomWebSocketConfig = {}
): UseCustomWebSocketReturn {
  const {
    enablePingPong = false,
    pingIntervalMs = 5000,
    pongTimeoutMs = 5000,
    // ...any other react-use-websocket options
    ...reactUseWsOptions
  } = config;

  const [error, setError] = useState<string | null>(null);

  /**
   * Tracks how many times we've attempted to reconnect.
   * We increment this on each *close* event if `shouldReconnect` returns true,
   * or when `onReconnectStop` tells us we've maxed out attempts.
   */
  const [reconnectAttempt, setReconnectAttempt] = useState(0);

  // Keep a flag to prevent auto-reconnect when the user manually disconnected
  const manualDisconnectRef = useRef(false);

  // Ping/pong references
  const pingIntervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const pongTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const didPongRef = useRef<boolean>(false);

  /**
   * Clears all ping/pong timers.
   */
  const clearPingPong = useCallback(() => {
    if (pingIntervalRef.current) {
      clearInterval(pingIntervalRef.current);
      pingIntervalRef.current = null;
    }
    if (pongTimeoutRef.current) {
      clearTimeout(pongTimeoutRef.current);
      pongTimeoutRef.current = null;
    }
  }, []);

  /**
   * React-use-websocket event handlers.
   */

  const onOpen: Options["onOpen"] = (event) => {
    // Reset user-disconnect flag (we're online again)
    manualDisconnectRef.current = false;
    setError(null);

    // If the user provided a custom onOpen, call it
    reactUseWsOptions.onOpen?.(event);

    // Start ping/pong if enabled
    if (enablePingPong) {
      pingIntervalRef.current = setInterval(() => {
        didPongRef.current = false;
        sendMessage("ping");

        // Set a timeout to wait for "pong"
        pongTimeoutRef.current = setTimeout(() => {
          if (!didPongRef.current) {
            console.error("Pong timeout - server unresponsive");
            // Force a reconnect by closing. 
            // Because manualDisconnectRef is false, `shouldReconnect` => true
            
            getWebSocket()?.close();
          }
        }, pongTimeoutMs);
      }, pingIntervalMs);
    }
  };

  const onClose: Options["onClose"] = (event) => {
    clearPingPong();

    // If the library *will* reconnect, increment attempts
    // `shouldReconnect` is called first, so let's check it ourselves:
    const shouldReconnect = reactUseWsOptions.shouldReconnect?.(event);
    if (shouldReconnect !== false && !manualDisconnectRef.current) {
      setReconnectAttempt((prev) => prev + 1);
    }

    reactUseWsOptions.onClose?.(event);
  };

  const onError: Options["onError"] = (event) => {
    setError(`WebSocket error occurred.`);
    reactUseWsOptions.onError?.(event);
  };

  const onMessage: Options["onMessage"] = (event) => {
    // Handle pong response
    if (event.data === "pong") {
      didPongRef.current = true;
      return;
    }
    
    // Forward the message to the user's handler if it exists
    if (reactUseWsOptions.onMessage) {
      reactUseWsOptions.onMessage(event);
    }
  };

  /**
   * If react-use-websocket stops reconnect attempts (because we hit `reconnectAttempts`),
   * this is called. We can store that final attempt count.
   */
  const onReconnectStop: Options["onReconnectStop"] = (numAttempts) => {
    setReconnectAttempt(numAttempts);
    reactUseWsOptions.onReconnectStop?.(numAttempts);
  };

  /**
   * Actual hook usage from `react-use-websocket`.
   */
  const { readyState, lastMessage, sendMessage, getWebSocket } = rawUseWebSocket(
    socketUrl,
    {
      // Spread other options first
      ...reactUseWsOptions,
      // Then override with our handlers to ensure they don't get overwritten
      shouldReconnect: (closeEvent) => {
        if (manualDisconnectRef.current) {
          return false;
        }
        return reactUseWsOptions.shouldReconnect
          ? reactUseWsOptions.shouldReconnect(closeEvent)
          : true;
      },
      onOpen,
      onClose,
      onError,
      onMessage,
      onReconnectStop,
    }
  );

  /**
   * Derive custom connection states from the numeric readyState
   */
  let connectionState: CustomConnectionState;
  switch (readyState) {
    case ReadyState.CONNECTING:
      connectionState = CustomConnectionState.CONNECTING;
      break;
    case ReadyState.OPEN:
      connectionState = CustomConnectionState.CONNECTED;
      break;
    default:
      connectionState = CustomConnectionState.DISCONNECTED;
      break;
  }

  /**
   * Manually disconnect (prevents further auto-reconnect)
   */
  const manualDisconnect = useCallback(() => {
    manualDisconnectRef.current = true;
    clearPingPong();
    getWebSocket()?.close(1000, "Manual Disconnect");
  }, [clearPingPong, getWebSocket]);

  /**
   * Manually reconnect:
   * - Flip off manualDisconnectRef
   * - Clear ping/pong
   * - Force-close the socket (if open) to trigger reconnection logic
   */
  const manualReconnect = useCallback(() => {
    manualDisconnectRef.current = false;
    clearPingPong();
    getWebSocket()?.close();
  }, [clearPingPong, getWebSocket]);

  /**
   * Helper to send binary data, e.g. an Int16Array.
   */
  const sendBinaryData = useCallback(
    (numbers: number[]) => {
      const buffer = new Int8Array(numbers).buffer;
      sendMessage(buffer);
    },
    [sendMessage]
  );

  const isConnected = connectionState === CustomConnectionState.CONNECTED;

  return {
    connectionState,
    isConnected,
    lastMessage,
    error,
    sendMessage,
    sendBinaryData,
    manualDisconnect,
    manualReconnect,
    reconnectAttempt,
  };
}
