import "../App.css";

export default function DetectionCard({ alert }) {
  return (
    <div className="detection-card">
      {alert && alert.event === "person_detected" && (
        <>
          <img
            src={`data:image/jpeg;base64,${alert.image}`}
            alt="Detected Person"
          />
          <div>
            {new Date(parseInt(alert.timestamp) * 1000).toLocaleString()}
          </div>
        </>
      )}
    </div>
  );
}
