# COMP4447 – Speech to Text with ESP32 & Google Cloud

This project connects an ESP32 microcontroller to a Python WebSocket server for real-time speech-to-text transcription using Google Cloud Speech-to-Text API.

---

## Google API Credential Setup

Before running the project, you must create a Google Cloud Service Account with Speech-to-Text access.

### Step-by-step:

1. Go to the [Google Cloud Console](https://console.cloud.google.com/apis/credentials).
2. Click **Create Credentials → Service Account**.
3. Give it a **name** and click **Continue**.
4. In **Authorization**, click **Select a role** → search for **Cloud Speech-to-Text** → select it.
5. Click **Done**.
6. Open your new **Service Account**.
7. Go to the **Keys** tab → click **Add key → Create new key → JSON**.
8. Download the generated **JSON credentials file**.
9. Place the file in your project and reference it in `ws_speech_to_text.py`.

---
## Wiring
---
## Running the Speech-to-Text Server

Make sure your laptop (Python server) and ESP32 are on the same Wi-Fi network.

### Steps:

1. Get your **local IP address** of the machine running the Python script.
2. In your ESP32 code (`main.cpp`), set:

   ```cpp
   const char* WS_HOST = "your_local_ip_address";
   ```
3. Start the Python server:

   ```bash
   python3 ws_speech_to_text.py
   ```
4. Upload and run the ESP32 firmware (`main.cpp`).



---

## Using Real-Time Transcription

Once you see the following output from the ESP32:

```
I2S setup complete. Streaming…
Connecting Wi-Fi..
Wi-Fi OK: 172.20.10.13
[WS] Connected
```

The ESP32 microphone is configured, Wi-Fi connection is successful, and the WebSocket connection to the Python server is established.

Now you can speak into the microphone and see real-time transcription.

---

### Example Output

```
[Transcript] [interim]  One,
[Transcript] [interim]  One, two,
[Transcript] [interim]  One, two, one,
[Transcript] [interim]  One, two, one, two.
[Transcript] [interim]  one, two, one, two,
[Transcript] [interim]  one, two, one, two, this is
[Transcript] [final]    one, two, one, two, this is a test
```

* **`[interim]`** → Partial, still listening
* **`[final]`** → Full sentence detected after silence

---

## Summary

| Component     | Description                                      |
| ------------- | ------------------------------------------------ |
| ESP32         | Captures microphone audio and streams it         |
| I2S Microphone| Audio capturing                                  |
| Python Server | Receives audio and transcribes with Google Cloud |
| Google Cloud  | Provides Speech-to-Text model                    |
| WebSocket     | Enables real-time streaming communication        |

