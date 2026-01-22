import time
import cv2
import numpy as np
import redis
from fastapi import FastAPI, UploadFile, File, Body
from fastapi.middleware.cors import CORSMiddleware
from ultralytics import YOLO

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
        cv2.imwrite(f'detected/{debug_filename}', img)

        event_data = {
            "event": "person_detected",
            "count": person_count,
            "model": "yolov8n",
        }
        r.xadd("tinyml:stream", event_data)
        return {"status": "success", "person_count": person_count}

    timestamp = int(time.time())
    debug_filename = f"debug_{timestamp}.jpg"
    cv2.imwrite(f'not_detected/{debug_filename}', img)
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