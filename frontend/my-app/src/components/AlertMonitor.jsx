import { useState, useEffect, useRef, use } from "react";

export default function AlertMonitor() {
  const [alert, setAlert] = useState(null);
  const [status, setStatus] = useState("Disconnected");

  // Use a ref to hold the WebSocket instance
  const socketRef = useRef(null);

  useEffect(() => {
    // Create WebSocket connection
    const socket = new WebSocket("ws://localhost:8000/ws/alerts");
    socketRef.current = socket;

    // Define event handlers
    socket.onopen = () => {
      console.log("WebSocket connected");
      setStatus("Connected");
    };

    socket.onmessage = (event) => {
      console.log("WebSocket message received:", event.data);

      if (event.data === "PERSON_DETECTED") {
        setAlert("Person Detected!");
        setTimeout(() => setAlert(null), 5000); // Clear alert after 5 seconds
      }
    };

    socket.onclose = () => {
      console.log("WebSocket disconnected");
      setStatus("Disconnected");
    };
  }, []);

  return (
    <div>
      <h2>Alert Monitor</h2>
      <p>Status: {status}</p>
      {alert && <div style={{ color: "red", fontWeight: "bold" }}>{alert}</div>}
    </div>
  );
}
