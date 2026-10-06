#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <BLEUtils.h>
#include <HIDKeyboardTypes.h>
#include <Preferences.h>
#include <Wire.h>

#include "pear_logo_bitmap.h"
#include "install_qr_bitmap.h"

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_store.h>
#endif

namespace {
constexpr int kLcdMosi = 7;
constexpr int kLcdClock = 6;
constexpr int kLcdChipSelect = 10;
constexpr int kLcdDataCommand = 2;
constexpr int kLcdBacklight = 3;

constexpr int kTouchSda = 4;
constexpr int kTouchScl = 5;
constexpr int kTouchInterrupt = 0;
constexpr int kTouchReset = 1;
constexpr uint8_t kTouchAddress = 0x15;
constexpr uint32_t kTapGuardMs = 240;
constexpr uint32_t kTouchReleaseMs = 55;
constexpr uint32_t kFlagCalibrationHoldMs = 700;
constexpr uint32_t kLongPressMs = 1600;
constexpr uint32_t kUsbReplyWindowMs = 180;
constexpr uint32_t kBleTapQueueTimeoutMs = 2000;
constexpr uint32_t kBootLogoMs = 900;
constexpr uint32_t kInstallerQrMs = 20000;
constexpr int32_t kCenterHoldRadiusSquared = 70 * 70;

constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kRussianBlue = 0x001F;
constexpr uint16_t kRussianRed = 0xF800;
constexpr uint16_t kAmericanBlue = 0x0011;
constexpr uint16_t kAmericanRed = 0xB800;

Arduino_DataBus *displayBus = new Arduino_ESP32SPI(
    kLcdDataCommand,
    kLcdChipSelect,
    kLcdClock,
    kLcdMosi,
    GFX_NOT_DEFINED,
    FSPI);

Arduino_GFX *display = new Arduino_GC9A01(
    displayBus,
    GFX_NOT_DEFINED,
    0,
    true);

constexpr uint16_t kNextKeyboardLayoutUsage = 0x029D;
constexpr uint8_t kModeSchemaVersion = 2;
constexpr char kLanguageSyncServiceUuid[] =
    "7a780001-6d9f-4d4e-9e37-9a6e41f5a101";
constexpr char kLanguageSyncCharacteristicUuid[] =
    "7a780002-6d9f-4d4e-9e37-9a6e41f5a101";

constexpr uint8_t kHidReportDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Keyboard)
    0x19, 0xE0,        //   Usage Minimum (Left Control)
    0x29, 0xE7,        //   Usage Maximum (Right GUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Constant)
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Keyboard)
    0x19, 0x00,        //   Usage Minimum (Reserved)
    0x29, 0x65,        //   Usage Maximum (Keyboard Application)
    0x81, 0x00,        //   Input (Data, Array)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (Num Lock)
    0x29, 0x05,        //   Usage Maximum (Kana)
    0x91, 0x02,        //   Output (Data, Variable, Absolute)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x01,        //   Output (Constant)
    0xC0,              // End Collection

    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x02,  //   Logical Maximum (767)
    0x19, 0x00,        //   Usage Minimum (Undefined)
    0x2A, 0xFF, 0x02,  //   Usage Maximum (0x02FF)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data, Array, Absolute)
    0xC0,              // End Collection

    // Apple Vendor Keyboard / Language (the semantic Globe action). macOS
    // exposes this as a keyboard event; it shares the subscribed report 1.
    0x06, 0x01, 0xFF,  // Usage Page (Apple Vendor Keyboard, 0xFF01)
    0x09, 0x30,        // Usage (Language)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x09, 0x30,        //   Usage (Language)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0x75, 0x07,        //   Report Size (7)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Constant)
    0xC0,              // End Collection
};

struct KeyboardAppleReport {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keys[6];
  uint8_t appleLanguage;
} __attribute__((packed));

BLEHIDDevice *bleHid = nullptr;
BLECharacteristic *bleKeyboardInput = nullptr;
BLECharacteristic *bleConsumerInput = nullptr;
BLECharacteristic *bleLanguageSync = nullptr;
BLEServer *bleServer = nullptr;
volatile bool bleConnected = false;
volatile bool bleKeyboardSubscribed = false;
volatile bool bleConsumerSubscribed = false;
volatile bool bleAuthenticated = false;
uint16_t bleConnectionHandle = 0;
Preferences preferences;

enum class PlatformMode : uint8_t {
  Mac = 0,
  Windows = 1,
  Linux = 2,
  Universal = 3,
};

PlatformMode platformMode = PlatformMode::Universal;

bool touchAvailable = false;
bool touchWasDown = false;
bool longPressHandled = false;
bool standaloneTapPending = false;
bool bleTapQueued = false;
bool usbBridgeActive = false;
bool bootLogoVisible = false;
bool installerQrPending = false;
bool installerQrVisible = false;
uint32_t lastTouchSeenAt = 0;
uint32_t lastTapSentAt = 0;
uint32_t touchStartedAt = 0;
uint32_t standaloneTapStartedAt = 0;
uint32_t bleTapQueuedAt = 0;
uint32_t modeScreenUntil = 0;
uint32_t bootLogoUntil = 0;
uint16_t touchStartedX = 120;
uint16_t touchStartedY = 120;
char serialLine[64] = {};
size_t serialLineLength = 0;
char currentLanguage[3] = {'E', 'N', '\0'};
volatile bool bleLanguagePending = false;
char pendingBleLanguage[3] = {};

bool isSupportedLanguage(const char *code) {
  return strcmp(code, "EN") == 0 || strcmp(code, "RU") == 0;
}

void queueBleLanguage(const String &value) {
  const char *bytes = value.c_str();
  const size_t length = value.length();
  const size_t offset = length >= 7 && strncmp(bytes, "LANG ", 5) == 0 ? 5 : 0;
  if (length < offset + 2) {
    Serial.println("BLE LANGUAGE REJECTED: SHORT VALUE");
    return;
  }

  char code[3] = {
      static_cast<char>(toupper(static_cast<unsigned char>(bytes[offset]))),
      static_cast<char>(toupper(static_cast<unsigned char>(bytes[offset + 1]))),
      '\0',
  };
  if (!isSupportedLanguage(code)) {
    Serial.printf("BLE LANGUAGE UNSUPPORTED=%s\n", code);
    return;
  }

  pendingBleLanguage[0] = code[0];
  pendingBleLanguage[1] = code[1];
  pendingBleLanguage[2] = '\0';
  bleLanguagePending = true;
}

class KeyboardServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    bleConnected = true;
    // Do not clear readiness here. On bonded reconnects NimBLE may deliver
    // authentication and CCCD subscription callbacks before this callback.
    // Disconnect already clears all three flags.
    Serial.println("BLE CONNECTED");
  }

  void onDisconnect(BLEServer *) override {
    bleConnected = false;
    bleKeyboardSubscribed = false;
    bleConsumerSubscribed = false;
    bleAuthenticated = false;
    bleTapQueued = false;
    BLEDevice::startAdvertising();
    Serial.println("BLE ADVERTISING");
  }

#if defined(CONFIG_NIMBLE_ENABLED)
  void onConnect(BLEServer *server, ble_gap_conn_desc *description) override {
    bleConnectionHandle = description->conn_handle;
    const bool requested = server->requestConnParams(
        bleConnectionHandle, 0x06, 0x0C, 0, 400);
    Serial.printf("BLE FAST PARAMS=%s INTERVAL=%u LATENCY=%u\n",
                  requested ? "REQUESTED" : "FAILED",
                  description->conn_itvl,
                  description->conn_latency);
  }

  void onConnParamsUpdate(uint16_t,
                          uint16_t interval,
                          uint16_t latency,
                          uint16_t,
                          uint8_t status) override {
    Serial.printf("BLE PARAMS STATUS=%u INTERVAL=%u LATENCY=%u\n",
                  status, interval, latency);
  }
#endif
};

class KeyboardInputCallbacks : public BLECharacteristicCallbacks {
 public:
  explicit KeyboardInputCallbacks(volatile bool *subscribed,
                                  const char *reportName)
      : subscribed_(subscribed), reportName_(reportName) {}

#if defined(CONFIG_NIMBLE_ENABLED)
  void onSubscribe(BLECharacteristic *,
                   ble_gap_conn_desc *,
                   uint16_t subscription) override {
    *subscribed_ = subscription != 0;
    Serial.printf("BLE %s SUBSCRIBED=%s\n", reportName_,
                  *subscribed_ ? "YES" : "NO");
  }
#endif

 private:
  volatile bool *subscribed_;
  const char *reportName_;
};

class LanguageSyncCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    queueBleLanguage(characteristic->getValue());
  }

#if defined(CONFIG_NIMBLE_ENABLED)
  void onWrite(BLECharacteristic *characteristic,
               ble_gap_conn_desc *) override {
    queueBleLanguage(characteristic->getValue());
  }
#endif
};

class KeyboardSecurityCallbacks : public BLESecurityCallbacks {
  bool onSecurityRequest() override {
    return true;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override {
    bleAuthenticated = result.success;
    bleKeyboardSubscribed = result.success;
    bleConsumerSubscribed = result.success;
    Serial.printf("BLE PAIRED=%s\n", result.success ? "YES" : "NO");
  }
#elif defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc *description) override {
    bleAuthenticated = description->sec_state.encrypted;
    Serial.printf("BLE PAIRED=%s\n",
                  description->sec_state.encrypted ? "YES" : "NO");
  }
#endif

  uint32_t onPassKeyRequest() override {
    return 0;
  }

  void onPassKeyNotify(uint32_t) override {}

  bool onConfirmPIN(uint32_t) override {
    return true;
  }
};

void beginBleKeyboard() {
  BLEDevice::init("Round Language Switch");
  BLEDevice::setPower(ESP_PWR_LVL_P9);

  BLESecurity *security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);
  security->setAuthenticationMode(true, false, true);
  security->setForceAuthentication(true);
  BLEDevice::setSecurityCallbacks(new KeyboardSecurityCallbacks());

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new KeyboardServerCallbacks());
  bleHid = new BLEHIDDevice(bleServer);
  bleHid->manufacturer()->setValue("Round Display");
  // Apple only promotes its vendor Language usage into a Globe keyboard event
  // for HID devices advertising Apple vendor support. Other hosts ignore the
  // vendor field and use the standards-based Consumer report instead.
  // BLEHIDDevice::pnp() in this Arduino core serializes the 16-bit fields in
  // big-endian order although the Bluetooth PnP ID characteristic is
  // little-endian. Pass the byte-swapped value so the host sees VID 0x05AC.
  bleHid->pnp(0x02, 0xAC05, 0x4001, 0x0100);
  bleHid->hidInfo(0x00, 0x01);
  bleHid->reportMap(const_cast<uint8_t *>(kHidReportDescriptor),
                    sizeof(kHidReportDescriptor));
  bleKeyboardInput = bleHid->inputReport(1);
  bleKeyboardInput->setCallbacks(
      new KeyboardInputCallbacks(&bleKeyboardSubscribed, "KEYBOARD"));
  bleHid->outputReport(1);
  bleConsumerInput = bleHid->inputReport(2);
  bleConsumerInput->setCallbacks(
      new KeyboardInputCallbacks(&bleConsumerSubscribed, "CONSUMER"));
  bleHid->setBatteryLevel(100);
  bleHid->startServices();

  BLEService *languageSyncService =
      bleServer->createService(kLanguageSyncServiceUuid);
  bleLanguageSync = languageSyncService->createCharacteristic(
      kLanguageSyncCharacteristicUuid,
      BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_WRITE_NR);
  bleLanguageSync->setCallbacks(new LanguageSyncCallbacks());
  languageSyncService->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID(bleHid->hidService()->getUUID());
  advertising->addServiceUUID(kLanguageSyncServiceUuid);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x0C);
  BLEDevice::startAdvertising();
  Serial.println("BLE HID READY");
}

bool probeI2cAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool readTouch(uint16_t &x, uint16_t &y) {
  if (!touchAvailable) {
    return false;
  }

  Wire.beginTransmission(kTouchAddress);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(kTouchAddress, static_cast<uint8_t>(5)) != 5) {
    return false;
  }

  const uint8_t points = Wire.read() & 0x0F;
  const uint8_t xHigh = Wire.read();
  const uint8_t xLow = Wire.read();
  const uint8_t yHigh = Wire.read();
  const uint8_t yLow = Wire.read();

  x = ((xHigh & 0x0F) << 8) | xLow;
  y = ((yHigh & 0x0F) << 8) | yLow;
  return points > 0 && x < 240 && y < 240;
}

void drawCenteredText(const char *text, int y, uint16_t color, uint8_t size) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  display->setTextSize(size);
  display->getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  display->setTextColor(color);
  display->setCursor((240 - width) / 2, y);
  display->print(text);
}

void drawRussianFlag() {
  // The LCD itself is circular, so three full-width bands become a
  // perfectly clipped, edge-to-edge round flag without a bitmap mask.
  display->fillRect(0, 0, 240, 80, kWhite);
  display->fillRect(0, 80, 240, 80, kRussianBlue);
  display->fillRect(0, 160, 240, 80, kRussianRed);
}

void drawStar(int16_t centerX, int16_t centerY) {
  // Five-point star as a 10-vertex triangle fan. At this size it remains
  // crisp on the 240x240 panel and reads better than a circular dot.
  constexpr float kOuterRadius = 4.0F;
  constexpr float kInnerRadius = 1.7F;
  int16_t x[10];
  int16_t y[10];
  for (int point = 0; point < 10; ++point) {
    const float angle = -PI / 2.0F + point * PI / 5.0F;
    const float radius = (point % 2 == 0) ? kOuterRadius : kInnerRadius;
    x[point] = centerX + static_cast<int16_t>(roundf(cosf(angle) * radius));
    y[point] = centerY + static_cast<int16_t>(roundf(sinf(angle) * radius));
  }

  for (int point = 0; point < 10; ++point) {
    const int next = (point + 1) % 10;
    display->fillTriangle(centerX, centerY, x[point], y[point],
                          x[next], y[next], kWhite);
  }
}

void drawAmericanFlag() {
  // Thirteen stripes cover the whole screen. Integer boundaries distribute
  // the 240 rows without leaving seams between stripes.
  for (int stripe = 0; stripe < 13; ++stripe) {
    const int y = stripe * 240 / 13;
    const int nextY = (stripe + 1) * 240 / 13;
    display->fillRect(0, y, 240, nextY - y,
                      stripe % 2 == 0 ? kAmericanRed : kWhite);
  }

  constexpr int kCantonWidth = 120;
  constexpr int kCantonHeight = 7 * 240 / 13;
  display->fillRect(0, 0, kCantonWidth, kCantonHeight, kAmericanBlue);

  // Official 50-star arrangement: alternating rows of six and five stars.
  for (int row = 0; row < 9; ++row) {
    const int starsInRow = row % 2 == 0 ? 6 : 5;
    const int firstX = row % 2 == 0 ? 10 : 20;
    for (int column = 0; column < starsInRow; ++column) {
      drawStar(firstX + column * 20, 7 + row * 14);
    }
  }
}

void drawLanguage(const char *code) {
  if (strcmp(code, "RU") == 0) {
    drawRussianFlag();
  } else if (strcmp(code, "EN") == 0) {
    drawAmericanFlag();
  } else {
    display->fillScreen(0x0000);
  }
}

void drawBootLogo() {
  display->fillScreen(0x0000);
  display->drawBitmap((240 - kPearLogoWidth) / 2,
                      (240 - kPearLogoHeight) / 2,
                      kPearLogoBitmap,
                      kPearLogoWidth,
                      kPearLogoHeight,
                      kWhite);
  bootLogoVisible = true;
  bootLogoUntil = millis() + kBootLogoMs;
}

const char *platformModeLabel(PlatformMode mode) {
  switch (mode) {
    case PlatformMode::Universal:
      return "AUTO";
    case PlatformMode::Windows:
      return "WIN";
    case PlatformMode::Linux:
      return "LINUX";
    case PlatformMode::Mac:
    default:
      return "MAC";
  }
}

void drawModeScreen(PlatformMode mode) {
  display->fillScreen(0x0000);
  drawCenteredText(platformModeLabel(mode), 91, 0xFFFF,
                   mode == PlatformMode::Linux ? 4 : 5);
  drawCenteredText("BLE MODE", 145, 0x07FF, 2);
}

void drawPairingScreen() {
  display->fillScreen(0x0000);
  drawCenteredText("PAIR", 88, 0xFFFF, 5);
  drawCenteredText("READY", 145, 0x07FF, 2);
}

void drawInstallerQr() {
  display->fillScreen(kWhite);
  constexpr int kQuietZoneModules = 4;
  const int totalModules = kInstallQrSize + 2 * kQuietZoneModules;
  const int scale = 240 / totalModules;
  const int qrPixels = kInstallQrSize * scale;
  const int origin = (240 - qrPixels) / 2;
  for (uint8_t y = 0; y < kInstallQrSize; ++y) {
    for (uint8_t x = 0; x < kInstallQrSize; ++x) {
      if (installQrModuleIsBlack(x, y)) {
        display->fillRect(origin + x * scale, origin + y * scale,
                          scale, scale, 0x0000);
      }
    }
  }
  installerQrVisible = true;
  Serial.printf("INSTALL QR=%s\n", kInstallUrl);
}

void cyclePlatformMode() {
  bootLogoVisible = false;
  installerQrPending = false;
  installerQrVisible = false;
  platformMode = static_cast<PlatformMode>(
      (static_cast<uint8_t>(platformMode) + 1) % 4);
  preferences.putUChar("platform", static_cast<uint8_t>(platformMode));
  drawModeScreen(platformMode);
  modeScreenUntil = millis() + 2500;
  Serial.printf("BLE MODE=%s\n", platformModeLabel(platformMode));
}

int clearBluetoothBonds() {
#if defined(CONFIG_NIMBLE_ENABLED)
  return ble_store_clear() == 0 ? 1 : 0;
#elif defined(CONFIG_BLUEDROID_ENABLED)
  int deviceCount = esp_ble_get_bond_device_num();
  if (deviceCount <= 0) {
    return 0;
  }

  esp_ble_bond_dev_t *devices = static_cast<esp_ble_bond_dev_t *>(
      malloc(sizeof(esp_ble_bond_dev_t) * deviceCount));
  if (devices == nullptr) {
    Serial.println("BLE BOND CLEAR ERROR: NO MEMORY");
    return 0;
  }

  int removed = 0;
  if (esp_ble_get_bond_device_list(&deviceCount, devices) == ESP_OK) {
    for (int index = 0; index < deviceCount; ++index) {
      if (esp_ble_remove_bond_device(devices[index].bd_addr) == ESP_OK) {
        ++removed;
      }
    }
  }
  free(devices);
  return removed;
#else
  return 0;
#endif
}

void enterNewHostPairingMode() {
  bootLogoVisible = false;
  installerQrPending = true;
  installerQrVisible = false;
  standaloneTapPending = false;
  bleTapQueued = false;
  modeScreenUntil = 0;
  drawPairingScreen();

  BLEDevice::stopAdvertising();
  const int removed = clearBluetoothBonds();
  if (bleConnected && bleServer != nullptr) {
    const uint16_t connectionId = bleServer->getConnId();
    bleConnected = false;
    bleKeyboardSubscribed = false;
    bleConsumerSubscribed = false;
    bleAuthenticated = false;
    bleServer->disconnect(connectionId);
  }
  BLEDevice::startAdvertising();

  modeScreenUntil = millis() + 5000;
  Serial.printf("BLE NEW HOST PAIRING; BONDS CLEARED=%d\n", removed);
}

void toggleLocalLanguage() {
  bootLogoVisible = false;
  memcpy(currentLanguage,
         strcmp(currentLanguage, "RU") == 0 ? "EN" : "RU", 3);
  preferences.putUChar("language", strcmp(currentLanguage, "RU") == 0 ? 1 : 0);
  drawLanguage(currentLanguage);
  Serial.printf("DISPLAYED %s LOCAL\n", currentLanguage);
}

void applySynchronizedLanguage(const char *code, const char *transport) {
  if (!isSupportedLanguage(code)) {
    return;
  }
  standaloneTapPending = false;
  memcpy(currentLanguage, code, sizeof(currentLanguage));
  preferences.putUChar("language", strcmp(currentLanguage, "RU") == 0 ? 1 : 0);
  if (!bootLogoVisible && modeScreenUntil == 0) {
    drawLanguage(currentLanguage);
  }
  Serial.printf("DISPLAYED %s SYNC=%s\n", currentLanguage, transport);
}

bool bleKeyboardReady() {
  const bool activeReportReady =
      platformMode == PlatformMode::Universal
          ? (bleKeyboardSubscribed || bleConsumerSubscribed)
          : bleKeyboardSubscribed;
  return bleConnected && activeReportReady && bleAuthenticated;
}

void notifyKeyboardAppleReport(const KeyboardAppleReport &report) {
  bleKeyboardInput->setValue(
      reinterpret_cast<const uint8_t *>(&report), sizeof(report));
  bleKeyboardInput->notify();
}

void notifyConsumerUsage(uint16_t usage) {
  const uint8_t report[] = {
      static_cast<uint8_t>(usage & 0xFF),
      static_cast<uint8_t>(usage >> 8),
  };
  bleConsumerInput->setValue(report, sizeof(report));
  bleConsumerInput->notify();
}

void sendBleLanguageShortcut() {
  if (!bleKeyboardReady() || bleKeyboardInput == nullptr) {
    Serial.println("BLE SWITCH SKIPPED: HID NOT READY");
    return;
  }

  if (platformMode == PlatformMode::Universal) {
    if (bleConsumerSubscribed && bleConsumerInput != nullptr) {
      notifyConsumerUsage(kNextKeyboardLayoutUsage);
      delay(35);
      notifyConsumerUsage(kNextKeyboardLayoutUsage);
      delay(45);
      notifyConsumerUsage(0);
      delay(20);
      notifyConsumerUsage(0);
      Serial.println("BLE SWITCH SENT MODE=AUTO STANDARD=0x029D");
    } else {
      // macOS recognizes AC Next Keyboard Layout Select in the descriptor but
      // does not act on it, and its Globe action is user-configurable. The two
      // target Macs both use Command+Space, so use that host-specific fallback
      // when macOS subscribes only to the keyboard report.
      KeyboardAppleReport pressed = {};
      pressed.modifiers = KEY_LEFT_GUI;
      pressed.keys[0] = 0x2C;  // Keyboard Spacebar.
      notifyKeyboardAppleReport(pressed);
      delay(35);
      notifyKeyboardAppleReport(pressed);
      delay(45);
      KeyboardAppleReport released = {};
      notifyKeyboardAppleReport(released);
      delay(20);
      notifyKeyboardAppleReport(released);
      Serial.println("BLE SWITCH SENT MODE=AUTO MAC_COMMAND_SPACE");
    }
    toggleLocalLanguage();
    return;
  }

  KeyboardAppleReport pressed = {};
  pressed.modifiers = KEY_LEFT_GUI;
  pressed.keys[0] = 0x2C;  // Keyboard Spacebar.
  notifyKeyboardAppleReport(pressed);
  delay(35);
  notifyKeyboardAppleReport(pressed);
  delay(45);

  KeyboardAppleReport released = {};
  notifyKeyboardAppleReport(released);
  delay(20);
  notifyKeyboardAppleReport(released);
  Serial.printf("BLE SWITCH SENT MODE=%s\n", platformModeLabel(platformMode));
  toggleLocalLanguage();
}

void requestLanguageSwitch() {
  Serial.println("TAP");
  if (!usbBridgeActive || !Serial) {
    if (bleKeyboardReady()) {
      sendBleLanguageShortcut();
    } else if (bleConnected) {
      bleTapQueued = true;
      bleTapQueuedAt = millis();
      Serial.println("BLE TAP QUEUED: WAITING FOR HID");
    } else {
      Serial.println("BLE SWITCH SKIPPED: NOT CONNECTED");
    }
    return;
  }
  standaloneTapPending = true;
  standaloneTapStartedAt = millis();
}

void processSerialLine(const char *line) {
  if (strcmp(line, "TEST SWITCH") == 0) {
    Serial.println("SERIAL TEST SWITCH");
    sendBleLanguageShortcut();
    return;
  }

  if (strcmp(line, "BLE PAIR") == 0) {
    enterNewHostPairingMode();
    return;
  }

  if (strcmp(line, "BLE RECONNECT") == 0) {
    if (bleConnected && bleServer != nullptr) {
      const uint16_t connectionId = bleServer->getConnId();
      bleConnected = false;
      bleKeyboardSubscribed = false;
      bleConsumerSubscribed = false;
      bleAuthenticated = false;
      bleServer->disconnect(connectionId);
    }
    BLEDevice::startAdvertising();
    Serial.println("BLE RECONNECT REQUESTED");
    return;
  }

  if (strncmp(line, "LANG ", 5) != 0 || strlen(line) < 7) {
    return;
  }

  char code[3] = {
      static_cast<char>(toupper(static_cast<unsigned char>(line[5]))),
      static_cast<char>(toupper(static_cast<unsigned char>(line[6]))),
      '\0',
  };
  standaloneTapPending = false;
  usbBridgeActive = true;
  applySynchronizedLanguage(code, "USB");
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\n') {
      serialLine[serialLineLength] = '\0';
      processSerialLine(serialLine);
      serialLineLength = 0;
    } else if (value != '\r' && serialLineLength < sizeof(serialLine) - 1) {
      serialLine[serialLineLength++] = value;
    }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(kLcdBacklight, OUTPUT);
  digitalWrite(kLcdBacklight, HIGH);

  pinMode(kTouchInterrupt, INPUT_PULLUP);
  pinMode(kTouchReset, OUTPUT);
  digitalWrite(kTouchReset, LOW);
  delay(10);
  digitalWrite(kTouchReset, HIGH);
  delay(60);

  Wire.begin(kTouchSda, kTouchScl, 400000);
  touchAvailable = probeI2cAddress(kTouchAddress);

  if (!display->begin(80000000)) {
    Serial.println("ERROR DISPLAY");
    while (true) {
      delay(1000);
    }
  }

  display->setRotation(0);
  display->setTextWrap(false);
  drawBootLogo();

  preferences.begin("round-switch", false);
  const uint8_t storedSchema = preferences.getUChar("mode-schema", 0);
  if (storedSchema < kModeSchemaVersion) {
    platformMode = PlatformMode::Universal;
    preferences.putUChar("platform", static_cast<uint8_t>(platformMode));
    preferences.putUChar("mode-schema", kModeSchemaVersion);
  } else if (preferences.isKey("platform")) {
    const uint8_t storedMode = preferences.getUChar("platform", 0);
    platformMode = storedMode <= static_cast<uint8_t>(PlatformMode::Universal)
                       ? static_cast<PlatformMode>(storedMode)
                       : PlatformMode::Universal;
  } else {
    platformMode = PlatformMode::Universal;
    preferences.putUChar("platform", static_cast<uint8_t>(platformMode));
  }
  const uint8_t storedLanguage = preferences.getUChar("language", 0);
  memcpy(currentLanguage, storedLanguage == 1 ? "RU" : "EN", 3);
  if (!preferences.isKey("language")) {
    preferences.putUChar("language", 0);
  }
  beginBleKeyboard();

  Serial.printf("READY TOUCH=%s\n", touchAvailable ? "YES" : "NO");
}

void loop() {
  handleSerialInput();

  if (bleLanguagePending) {
    char code[3] = {
        pendingBleLanguage[0],
        pendingBleLanguage[1],
        '\0',
    };
    bleLanguagePending = false;
    applySynchronizedLanguage(code, "BLE");
  }

  uint16_t x = 0;
  uint16_t y = 0;
  const bool touchIsDown = readTouch(x, y);
  const uint32_t now = millis();
  if (bootLogoVisible &&
      static_cast<int32_t>(now - bootLogoUntil) >= 0) {
    bootLogoVisible = false;
    drawLanguage(currentLanguage);
    Serial.printf("DISPLAYED %s\n", currentLanguage);
  }

  if (touchIsDown) {
    lastTouchSeenAt = now;
    if (!touchWasDown) {
      touchWasDown = true;
      touchStartedAt = now;
      touchStartedX = x;
      touchStartedY = y;
      longPressHandled = false;
    } else if (!longPressHandled && now - touchStartedAt >= kLongPressMs) {
      const int32_t deltaX = static_cast<int32_t>(touchStartedX) - 120;
      const int32_t deltaY = static_cast<int32_t>(touchStartedY) - 120;
      const bool startedInCenter =
          deltaX * deltaX + deltaY * deltaY <= kCenterHoldRadiusSquared;
      if (startedInCenter) {
        enterNewHostPairingMode();
      } else {
        standaloneTapPending = false;
        cyclePlatformMode();
      }
      longPressHandled = true;
    }
  } else if (touchWasDown && now - lastTouchSeenAt >= kTouchReleaseMs) {
    if (!longPressHandled) {
      const uint32_t holdDuration = now - touchStartedAt;
      if (installerQrVisible) {
        installerQrVisible = false;
        installerQrPending = false;
        modeScreenUntil = 0;
        drawLanguage(currentLanguage);
        Serial.println("INSTALL QR DISMISSED");
      } else if (holdDuration >= kFlagCalibrationHoldMs) {
        // Medium hold calibrates the local display without sending any HID
        // command. This recovers synchronization if the host layout was
        // changed by another keyboard or from the menu bar.
        toggleLocalLanguage();
        Serial.println("FLAG CALIBRATED WITHOUT HOST SWITCH");
      } else if (lastTapSentAt == 0 || now - lastTapSentAt >= kTapGuardMs) {
        requestLanguageSwitch();
        lastTapSentAt = now;
      }
    }
    touchWasDown = false;
  }

  if (standaloneTapPending &&
      now - standaloneTapStartedAt >= kUsbReplyWindowMs) {
    standaloneTapPending = false;
    sendBleLanguageShortcut();
  }

  if (bleTapQueued) {
    if (bleKeyboardReady()) {
      bleTapQueued = false;
      sendBleLanguageShortcut();
    } else if (now - bleTapQueuedAt >= kBleTapQueueTimeoutMs) {
      bleTapQueued = false;
      Serial.println("BLE TAP DROPPED: HID TIMEOUT");
    }
  }

  if (usbBridgeActive && !Serial) {
    usbBridgeActive = false;
  }

  if (modeScreenUntil != 0 &&
      static_cast<int32_t>(now - modeScreenUntil) >= 0) {
    if (installerQrPending) {
      installerQrPending = false;
      drawInstallerQr();
      modeScreenUntil = now + kInstallerQrMs;
    } else {
      installerQrVisible = false;
      modeScreenUntil = 0;
      drawLanguage(currentLanguage);
    }
  }

  delay(5);
}
