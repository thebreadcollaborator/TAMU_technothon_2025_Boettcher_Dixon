// R1 = D9 R2 = D8 R3 = D7 R4 = D6 C1 = D5 C2 = D4 C3 = D3 C4 = D2
#include "thingProperties.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ---------------- LCD -----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change to 0x3F if needed

// ---------------- Keypad -----------------
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {9, 8, 7, 6};   // R1 = 9, R2 = 8, R3 = 7, R4 = 6
byte colPins[COLS] = {5, 4, 3, 2};   // C1 = 5, C2 = 4, C3 = 3, C4 = 2

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Text typed on keypad before sending
String compose = "";

// ---------------- Scrolling state -----------------
String scrollText = "";
uint8_t scrollRow = 1;                // we scroll on line 2 (row index 1)
bool scrollingActive = false;
int scrollPos = 0;
unsigned long lastScroll = 0;
const unsigned long scrollInterval = 700;  // ms between scroll steps

// ---------------- LCD line helper -----------------
static void printLine(uint8_t row, const String &s) {
  String t = s;
  if (t.length() > 16) t.remove(16);
  while (t.length() < 16) t += ' ';
  lcd.setCursor(0, row);
  lcd.print(t);
}

// Start scrolling a given text on a given row (if longer than 16 chars)
void startScroll(uint8_t row, const String &text) {
  scrollRow = row;
  scrollText = text;
  scrollPos = 0;
  lastScroll = millis();

  if (scrollText.length() > 16) {
    scrollingActive = true;
  } else {
    scrollingActive = false;
    printLine(row, scrollText);   // just print normally if short
  }
}

// Called regularly from loop() to advance the scroll
void updateScroll() {
  if (!scrollingActive) return;
  if (scrollText.length() <= 16) {
    scrollingActive = false;
    return;
  }

  unsigned long now = millis();
  if (now - lastScroll < scrollInterval) return;
  lastScroll = now;

  // Create a 16-char window starting at scrollPos
  String window = "";
  for (int i = 0; i < 16; i++) {
    int idx = scrollPos + i;
    if (idx >= scrollText.length()) break;
    window += scrollText[idx];
  }

  printLine(scrollRow, window);

  scrollPos++;
  if (scrollPos >= scrollText.length()) {
    // Optional: reset to 0 to loop endlessly
    scrollPos = 0;
  }
}

// ======================================================
//                      SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  lcd.init();
  lcd.backlight();
  printLine(0, "Booting...");
  printLine(1, "Starting IoT...");

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  // Try to connect for up to 5s
  unsigned long start = millis();
  while (ArduinoCloud.connected() != 1 && millis() - start < 5000) {
    ArduinoCloud.update();
    delay(100);
  }

  lcd.clear();
  printLine(0, "Ready");
  printLine(1, "Type msg...");

  // Device → App: initial status message
  deviceMsg = "Device online!";
  Serial.println("deviceMsg: Device online!");

  // Start a subtle scroll on second line
  startScroll(1, "Type & press # to send");
}

// ======================================================
//                      LOOP
// ======================================================
void loop() {
  ArduinoCloud.update();

  char k = keypad.getKey();
  if (k) {
    handleKey(k);
  }

  // Allow Serial Monitor messages to go to app
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      sendToApp(line);
    }
  }

  // Update scrolling (non-blocking)
  updateScroll();
}

// ======================================================
//          KEYPAD HANDLER (4x4 Matrix)
// ======================================================
void handleKey(char key) {

  if (key == '*') {
    // Clear text
    compose = "";
    lcd.clear();
    printLine(0, "Cleared");
    printLine(1, "");
    scrollingActive = false;
    delay(300);
    lcd.clear();
    printLine(0, "Ready");
    printLine(1, "");
    return;
  }

  if (key == '#') {
    // SEND to app (IoT Cloud)
    if (compose.length() > 0) {
      sendToApp(compose);
      compose = "";
    }
    return;
  }

  // Add normal characters (limit size)
  if (compose.length() < 64) {
    compose += key;
  }

  // Display composing state on LCD (no scroll while typing)
  scrollingActive = false;
  lcd.clear();
  printLine(0, "Typing:");
  // Show tail of compose if too long
  String tail = compose;
  if (tail.length() > 16) tail = tail.substring(tail.length() - 16);
  printLine(1, tail);
}

// ======================================================
//      DEVICE → APP (send to deviceMsg variable)
// ======================================================
void sendToApp(const String &text) {
  deviceMsg = text;
  Serial.print("deviceMsg sent: ");
  Serial.println(text);

  lcd.clear();
  printLine(0, "Sent to app:");
  // Start scroll of the sent message on line 2
  startScroll(1, text);
}

// ======================================================
//  APP → DEVICE (called when `message` changes)
// ======================================================
void onMessageChange() {
  Serial.print("Received from app: ");
  Serial.println(message);

  lcd.clear();
  printLine(0, "From app:");
  // Start scroll of incoming message on line 2
  startScroll(1, message);
}
