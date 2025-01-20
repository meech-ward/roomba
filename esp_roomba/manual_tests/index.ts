const ws = new WebSocket("ws://10.0.0.35/ws");

ws.onopen = () => {
  console.log("Connected to echo server!");
  
  // Send a few test messages
  ws.send("Hello from Bun!");
  
  // Send another message after a short delay
  setTimeout(() => {
    ws.send("This is another test message!");
  }, 1000);
  
  // Send one more message after 2 seconds
  setTimeout(() => {
    ws.send("Final test message!");
  }, 2000);
};

ws.onmessage = (event) => {
  console.log("Received from server:", event.data);
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