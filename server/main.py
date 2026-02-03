import time
import cv2
import numpy as np
import redis
from fastapi import FastAPI, UploadFile, File, Body, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from ultralytics import YOLO
import asyncio
import base64
import json
from firebase_admin import messaging, credentials, initialize_app

# App and ML Setup
app = FastAPI(title="TinyML Smart Server")

# YOLOv8 Setup 
print("Loading YOLOv8 model...")
model = YOLO("yolov8n.pt")
print("YOLOv8 model loaded.")

# Redis Connection
try:
    r = redis.Redis(host='localhost', port=6379, db=0, decode_responses=True)
    r.ping()
    print("Connected to Redis") 
except Exception as e:
    print(f"Redis connection error: {e}")

# CORS Middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Routes

@app.post("/api/upload-image")
async def upload_image(file: bytes = Body(...)):
    contents = file

    np_arr = np.frombuffer(contents, np.uint8)
    img = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)

    if img is None:
        return {"status": "error", "message": "Failed to decode image"}
    
    results = model.predict(img, conf = 0.4, verbose = False)
    person_count = 0

    #results[0].boxes contains the detected boxes
    for box in results[0].boxes:
        class_id = int(box.cls[0])
        if class_id == 0 : # Person class in COCO dataset
            person_count += 1

    if person_count > 0:
        # Save image for debug purposes
        timestamp = int(time.time())
        debug_filename = f"debug_{timestamp}.jpg"
        # cv2.imwrite(f'../debug_images/detected/{debug_filename}', img)

        # Encode image as base64
        _, buffer = cv2.imencode('.jpg', img)
        img_base64 = base64.b64encode(buffer).decode('utf-8')

        event_data = {
            "event": "person_detected",
            "count": person_count,
            "model": "yolov8n",
            "image": img_base64
        }
        r.xadd("tinyml:stream", event_data)
        r.publish("detection_alerts", json.dumps({"event": "person_detected", "count": person_count, "timestamp": timestamp, "image": img_base64}))

        # Send push notification to subscribed devices
        message = messaging.Message(
            notification=messaging.Notification(
                title="Security Alert",
                body=f"{person_count} person(s) detected at {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(timestamp))}",
            ),
            topic="security_alerts"
        )

        try:
            messaging.send(message)
        except Exception as e:
            print(f"Error sending push notification: {e}")

        return {"status": "success", "person_count": person_count}

    timestamp = int(time.time())
    debug_filename = f"debug_{timestamp}.jpg"
    # cv2.imwrite(f'../debug_images/not_detected/{debug_filename}', img)
    return {"status": "success", "person_count": 0}

def format_stream_entry(entry):
    entry_id = entry[0]
    data = entry[1]
    timestamp_ms = int(entry_id.split('-')[0])
    readable_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(timestamp_ms / 1000))

    return {
        "id": entry_id,
        "event": data.get("event", "unknown"),
        "timestamp": readable_time,
        "image": data.get("image", None),
    }

@app.get("/api/events")
def get_events():
    try:
        events = r.xrevrange("tinyml:stream", "+", "-", count=20)
        formatted_events = [format_stream_entry(event) for event in events]
        return formatted_events
    except redis.exceptions.ResponseError as e:
        print(f"Redis error: {e}")
        return []
    
@app.websocket("/ws/alerts")
async def alert_websocket(websocket: WebSocket):
    await websocket.accept()

    # Subscribe to Redis channel
    pubsub = r.pubsub()
    pubsub.subscribe("detection_alerts")

    try:
        while True:
            message = pubsub.get_message()
            if message and message['type'] == 'message':
                alert = message['data']
                await websocket.send_text(alert)
                await asyncio.sleep(0.01)  # Yield control to event loop
            else:
                await asyncio.sleep(0.1)  # Avoid busy waiting

    except WebSocketDisconnect:
        print("WebSocket disconnected")
    finally:
        pubsub.unsubscribe("detection_alerts")
        pubsub.close()

@app.post("/api/register-token")
async def register_token(token: str = Body(..., embed=True)):
    try:
        response = messaging.subscribe_to_topic([token], "security_alerts")
        print(f"Subcribed token to topic: {response.success_count} success")
        return {"status": "subscribed"}
    except Exception as e:
        print(f"Error subscribing token: {e}")
        return {"status": "error", "message": str(e)}