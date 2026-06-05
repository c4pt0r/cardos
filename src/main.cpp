#include <M5Cardputer.h>

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = enable keyboard
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(200);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.drawString("CardOS boot", 10, 10);
  Serial.begin(115200);
  Serial.println("[cardos] boot");
}

void loop() {
  M5Cardputer.update();
  delay(10);
}
