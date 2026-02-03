import { useState, useEffect, useRef } from "react";
import DetectionCard from "./DetectionCard.jsx";

export default function AlertMonitor() {
  const [alert, setAlert] = useState(null);
  const [status, setStatus] = useState("Disconnected");
  const [detectionCards, setDetectionCards] = useState([]);

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

      const data = JSON.parse(event.data); // Parse the incoming message

      if (data.event === "person_detected") {
        setAlert(data);
        if (detectionCards.length >= 20) {
          setDetectionCards((prevCards) => [...prevCards.slice(1), data]);
        } else {
          setDetectionCards((prevCards) => [...prevCards, data]);
        }
      }
    };

    socket.onclose = () => {
      console.log("WebSocket disconnected");
      setStatus("Disconnected");
    };
  }, []);

  return (
    <div className="alert-monitor">
      <h2>Alert Monitor</h2>
      <p>Status: {status}</p>
      <div className="detection-cards-container">
        {[...detectionCards].reverse().map((alert, index) => (
          <DetectionCard key={detectionCards.length - index} alert={alert} />
        ))}
      </div>
    </div>
  );
}
