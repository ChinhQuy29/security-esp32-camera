#define EI_CLASSIFIER_ALLOCATION_STATIC 0
#define EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE 0
#define BOARD_HAS_PSRAM 1
#define MY_TENSOR_ARENA_SIZE (64 * 1024)

// Edge Impulse library
#include <chinhquy-project-1_inferencing.h>
#include <driver/i2s.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "config.h"

// XIAO ESP32S3 Sense Microphone Pins
#define I2S_SD 41
#define I2S_WS 42
#define I2S_PORT I2S_NUM_0

// Audio Config
#define SAMPLE_RATE 16000
#define CONFIDENCE_THRESHOLD 0.7

// --- (Your working camera pin definitions go here) ---
// Example for XIAO ESP32S3 Sense
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13
// --- End Camera Pins ---


// Your camera_init() function
// (Make sure this function is present and works)
void init_camera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; // JPEG is crucial for small size

  // Use QVGA for fast, small images.
  config.frame_size = FRAMESIZE_QVGA; // (320x240)
  config.jpeg_quality = 12; // 0-63 (lower = smaller, faster)
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

// Buffer to hold raw audio from microphone
EXT_RAM_ATTR int16_t sampleBuffer[EI_CLASSIFIER_SLICE_SIZE];
// void setup() {
//   Serial.begin(115200);
//   Serial.println("Booting...");

//   init_camera();

//   // Connect to Wi-Fi
  // WiFi.begin(WIFI_SSID, WIFI_PASS);
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }
  // Serial.println("\nWiFi Connected");
  // Serial.print("IP Address: ");
  // Serial.println(WiFi.localIP());
// }

// void loop() {
//   Serial.println("Taking picture...");
//   camera_fb_t * fb = esp_camera_fb_get();
//   if (!fb) {
//     Serial.println("Camera capture failed");
//     delay(10000); // Wait 10s and try again
//     return;
//   }

//   // Send the picture to the server
//   sendPicture(fb);

//   // Return the frame buffer to be re-used
//   esp_camera_fb_return(fb);

//   // Wait 10 seconds before the next loop
//   Serial.println("Waiting 10 seconds...");
//   delay(10000);
// }

void setup() {
  // Force the power rail for the expansion board ON
  pinMode(21, OUTPUT); 
  digitalWrite(21, HIGH);
  delay(500); // Give the PSRAM chip time to stabilize

  Serial.begin(115200);
  while (!Serial);

  // Connect to WIFI
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting Audio Wake Word System...");

 // Initialize PDM microphone
 i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // PDM is usually Mono
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = -1
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = -1, // 
    .ws_io_num = I2S_WS,  // Clock Pin (42)
    .data_out_num = -1,
    .data_in_num = I2S_SD // Data Pin (41)
  };

  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("I2S driver install failed");
    return;
  }
  if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
    Serial.println("I2S pin set failed");
    return;
  }
  if (i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO) != ESP_OK) {
    Serial.println("I2S clock set failed");
    return;
  }

  Serial.println("I2S Mic Setup Complete! Starting loop...");
}

// Wrapper to convert int16 audio to float for the AI model
int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    // Check if we are running out of bounds
    if (offset + length > EI_CLASSIFIER_SLICE_SIZE) {
        return EIDSP_SIGNAL_SIZE_MISMATCH;
    }
    // Use the Edge Impulse library to convert int16 -> float
    numpy::int16_to_float(&sampleBuffer[offset], out_ptr, length);
    return 0;
}

void loop() {
  Serial.println("Loop is running...");
  // Record Audio
  size_t bytesRead = 0;
  // Read enough samples to fill the Edge Impulse window
  i2s_read(I2S_PORT, (void*)sampleBuffer, EI_CLASSIFIER_SLICE_SIZE * 2, &bytesRead, portMAX_DELAY);
  // i2s_read(I2S_PORT, (void*)sampleBuffer, EI_CLASSIFIER_SLICE_SIZE * 2, &bytesRead, 100);

  if (bytesRead == 0) {
    Serial.println("Error: Microphone is silent (No Data)!");
    return;
  }

  // Run Inference
  signal_t signal;

  // Tell the signal struct to use our converter function
  signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
  signal.get_data = &microphone_audio_signal_get_data;
  
  ei_impulse_result_t result = { 0 };

  // MANUALLY ALLOCATE THE ARENA IN PSRAM
  static float* custom_arena = NULL;
  if (custom_arena == NULL) {
    custom_arena = (float*)ps_malloc(MY_TENSOR_ARENA_SIZE);
    if (custom_arena == NULL) {
      Serial.println("CRITICAL ERR: Could not allocate PSRAM for AI!");
      return;
    }
  }

  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

  if (res != 0) {
    Serial.printf("ERROR: run_classifier returned: %d\n ", res);
    return;
  }

  // Check prediction
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    Serial.printf("Detected %s: %.2f\n", result.classification[ix].label, result.classification[ix].value);
    if (result.classification[ix].value > CONFIDENCE_THRESHOLD) {
      // Serial.printf("Detected %s: %.2f\n", result.classification[ix].label, result.classification[ix].value);

      if (String(result.classification[ix].label) == "hello") {
        // Wake up camera
        Serial.println("Taking picture...");
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) {
          Serial.println("Camera capture failed");
          delay(10000); // Wait 10s and try again
          return;
        }

        // Send the picture to the server
        sendPicture(fb);

        // Return the frame buffer to be re-used
        esp_camera_fb_return(fb);
      }
    }
  }
}

void sendPicture(camera_fb_t * fb) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  HTTPClient http;
  
  char server_url[100];
  sprintf(server_url, "http://%s:%d%s", SERVER_IP, SERVER_PORT, UPLOAD_URL);
  
  Serial.print("Connecting to server: ");
  Serial.println(server_url);

  http.begin(server_url);
  
  // The image is JPEG, so set the content type
  http.addHeader("Content-Type", "image/jpeg");
  
  // Send the POST request with the image data (buffer and length)
  int httpResponseCode = http.POST(fb->buf, fb->len);

  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    String response = http.getString();
    Serial.println(response); // Will show {"status": "person_detected"}
  } else {
    Serial.printf("Error code: %d\n", httpResponseCode);
  }

  http.end();
}