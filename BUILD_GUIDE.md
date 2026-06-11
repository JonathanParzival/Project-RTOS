## 🔧 Build & Upload Instructions

### Prerequisites

1. **VS Code** dengan extensions:
   - PlatformIO IDE (ms-vscode.platformio-ide)
   - C/C++ (ms-vscode.cpptools)

2. **Hardware**:
   - ESP32 Development Board
   - RFID-RC522 Reader + Card
   - Buzzer (GPIO 14)
   - LEDs: Green (GPIO 26), Red (GPIO 25)
   - Relay (GPIO 27)
   - LCD 16x2 I2C (0x27)
   - Power supply

### Step 1: Open Project

```bash
# Buka folder project di VS Code
cd d:\PlatformIO\Projects\RTOS
code .
```

### Step 2: Build Project

Klik **PlatformIO: Build** dari command palette (`Ctrl+Shift+P`):

- atau klik icon PlatformIO di sidebar → Build
- atau gunakan `pio run` di terminal

**Expected Output:**

```
Checking size of built files...
|-- RAM:   [====     ]  45.2% (used 148324 bytes from 328192 bytes)
|-- Flash: [======   ]  62.8% (used 820432 bytes from 1310720 bytes)
```

### Step 3: Upload ke ESP32

1. **Connect ESP32** via USB
2. Klik **PlatformIO: Upload** atau tombol Upload di PlatformIO
3. Monitor output di terminal

**Expected Output:**

```
Connecting....._____....._____....._____....._____....._____....._____....._____
Hard resetting via RTS pin...
```

### Step 4: Verify Running

Klik **PlatformIO: Monitor** untuk Serial Monitor:

```
[BOOT] Initializing RFID Attendance System...
[BOOT] GPIO configured
[BOOT] I2C initialized
[BOOT] Loading UID database...
[BOOT] WiFi connecting to: YOUR_SSID
[WiFi] Connected! IP: 192.168.x.x
[BOOT] Creating FreeRTOS tasks...
[Input] Task started - RFID reader ready
[Auth] Task started - verifying UIDs...
[Display] Task started - LCD initialized
[Security] Task started - monitoring failed attempts...
```

---