const ws = new WebSocket("ws://10.0.0.35/ws");

ws.onopen = () => {
  console.log("Connected to echo server!");
  // Send one more message after 2 seconds
  setTimeout(() => {
    ws.send("start");
  }, 2000);
};

ws.onmessage = (event) => {
  console.log("Received from server:", event.data.length);
};

ws.onerror = (error) => {
  console.error("WebSocket error:", error);
};

ws.onclose = () => {
  console.log("Disconnected from server");
};

// Keep the process alive
process.on("SIGINT", () => {
  ws.close();
  process.exit(0);
});