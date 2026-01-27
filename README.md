# Security ESP32 Camera

This project detects people using an ESP32 camera and a Python YOLOv8 server.

## Hardware

- Seeed XIAO ESP32S3 Sense
- Battery / USB Power

## Software Structure

- `/CameraWebServer`: Arduino code for the device.
- `/server`: FastAPI Python server with YOLOv8.
- `/frontend`: React Frontend server

## How to Run

1. Start Redis: `docker run -d -p 6379:6379 redis/redis-stack`
2. Start Server: `cd server && uvicorn main:app --host 0.0.0.0`
3. Flash ESP32: Upload `esp32-camera.ino` with Arduino IDE.
