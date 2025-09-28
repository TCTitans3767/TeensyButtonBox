#include <SD.h>

#define configStructByteLength (sizeof(configStruct::buttonID) + sizeof(configStruct::buttonConfigMask) + sizeof(configStruct::groupID))
#define numberOfPossibleButtons 21

const uint16_t ledPinMap[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
const uint16_t buttonPinMap[] = {21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41};

const int chipSelect = 1; 

const char *filepath = "/config";

File configFile;

/* 0000000x = is button active
/ 000000x0 = is button sticky
/ 00000x00 = does button light up
/ 0000x000 = does button return pov angle (currently unused)
/ 000x0000 = does button return joystick angle (currently unused)
/ 00x00000 = is button signal inverted
/ 0x000000 = does button start activated
/ x0000000 = unused
*/

struct configStruct {
  uint16_t buttonID;
  uint16_t groupID;
  char buttonConfigMask;
} config[numberOfPossibleButtons];

class Button {
  public:
    Button() {
      _instance = this;
    }

    static Button* _instance;

    // button settings
    uint16_t buttonID;
    uint16_t groupID;
    bool buttonActive;
    bool buttonSticky;
    bool buttonLightsUp;
    bool isPOVAngle;
    bool isJoystickAngle;
    bool signalInverted;
    bool buttonBeginsOn;
    bool unused2;

    // button state
    bool buttonOn;
    bool buttonLightOn;

    void begin() {
      pinMode(ledPinMap[buttonID], OUTPUT);
      Serial.printf("setting pin %i to OUTPUT\n", ledPinMap[buttonID]);
      pinMode(buttonPinMap[buttonID], INPUT);
      Serial.printf("setting pin %i to INPUT\n", buttonPinMap[buttonID]);
      if (buttonBeginsOn) {
        buttonOn = true;
        buttonLightOn = (buttonLightsUp) & !buttonLightOn;
        digitalWrite(ledPinMap[buttonID], buttonLightOn ? HIGH : LOW);
      }
      if (buttonSticky) {
        attachInterrupt(digitalPinToInterrupt(buttonPinMap[buttonID]), Button::staticHandleInterrupt, RISING);
      } else {
        attachInterrupt(digitalPinToInterrupt(buttonPinMap[buttonID]), Button::staticHandleInterrupt, CHANGE);
      }
    }

    void handleInterrupt() {
      if (!buttonActive) {
        return;
      }
      if (groupID == 0) {
        buttonOn = !buttonOn;
        buttonLightOn = (buttonLightsUp) & !buttonLightOn;
        digitalWrite(ledPinMap[buttonID], buttonLightOn ? HIGH : LOW);
      }
      Serial.println("Interrupted!");
    }

  private:

    static void staticHandleInterrupt() {
      _instance->handleInterrupt();
    }


} buttons[numberOfPossibleButtons];

Button* Button::_instance = nullptr;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial) {
    // wait for serial port to connect
  }

  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);

  Serial.print("Initializing SD ...");

  SD.begin(BUILTIN_SDCARD);

  Serial.println("SD initialized.");

  for (uint i = 0; i < numberOfPossibleButtons; i++) {
    config[i] = {static_cast<decltype(configStruct::buttonID)>(i), 0, 0};
  }

  if (!SD.exists(filepath)) {
    Serial.println("config file does not already exist");
    initializeConfigIDs();
    goto createAndPopulateFile;
  }

  configFile = SD.open(filepath);
  Serial.printf("%i", configFile.size());

  if (configFile.size() < (numberOfPossibleButtons * configStructByteLength)) {
    createAndPopulateFile:
    Serial.println("config file invalid. generating new file");
    writeToConfigFile(filepath, config);
  }

  if (configFile.size() > (numberOfPossibleButtons * configStructByteLength)) {
    Serial.println("config file invalid. generating new file");
    SD.remove(filepath);
    writeToConfigFile(filepath, config);
  }

  if (configFile) {
    Serial.println("config file already open for some reason, closing file");
    configFile.close();
  }

  readFileToConfig(filepath);

  for(uint i = 0; i < numberOfPossibleButtons; i++) {
    buttons[i].begin();
  }
}

void loop() {
  // put your main code here, to run repeatedly:
}

void writeToConfigFile(const char* fileName, configStruct config[]) {
  File file = SD.open(fileName, FILE_WRITE);

  uint8_t buffer[numberOfPossibleButtons * configStructByteLength];
  for (uint buttonConfig = 0; buttonConfig < sizeof(buffer); buttonConfig += configStructByteLength) {
    for (uint i = 0; i < sizeof(configStruct::buttonID); i++) {
      buffer[buttonConfig + i] = (uint8_t)(config[buttonConfig/configStructByteLength].buttonID >> (8 * i));
    }
    for (uint i = 0; i < sizeof(configStruct::buttonID); i++) {
      buffer[buttonConfig + sizeof(configStruct::buttonID) + i] = (uint8_t)(config[buttonConfig/configStructByteLength].groupID >> (8 * i));
    }
    for (uint i = 0; i < sizeof(configStruct::buttonConfigMask); i++) {
      buffer[buttonConfig + sizeof(configStruct::buttonID) + sizeof(configStruct::groupID) + i] = (uint8_t)(config[buttonConfig/configStructByteLength].buttonConfigMask >> (8 * i));
    }
  }

  file.write(buffer, sizeof(buffer));

  file.close();
}

void readFileToConfig(const char* fileName) {

  File file = SD.open(fileName, FILE_READ);

  // read file contents and populate config with information
  for (uint i = 0; i < (numberOfPossibleButtons * configStructByteLength); i += configStructByteLength) {
    configStruct buffer;
    file.seek(i);
    file.read(&buffer.buttonID, sizeof(buffer.buttonID));

    file.seek((i) + sizeof(buffer.buttonID));
    file.read(&buffer.groupID, sizeof(buffer.groupID));

    file.seek((i) + sizeof(buffer.buttonID) + sizeof(buffer.groupID));
    file.read(&buffer.buttonConfigMask, sizeof(buffer.buttonConfigMask));

    Serial.print("button ");
    Serial.print(i/configStructByteLength);
    Serial.print(" config buffer: ");
    Serial.printf("%X ", buffer.buttonID);
    Serial.printf("%X ", buffer.groupID);
    Serial.printf("%X ", buffer.buttonConfigMask);
    Serial.println();

    config[i / configStructByteLength] = buffer;
  }

  // reapply information in config to button configurations
  for (uint i = 0; i < numberOfPossibleButtons; i++) {
    Button &button = buttons[i];
    configStruct &buttonConfig = config[i];
    button.buttonID = buttonConfig.buttonID;
    button.groupID = buttonConfig.groupID;
    button.buttonActive   = (buttonConfig.buttonConfigMask & 0b00000001);
    button.buttonSticky   = (buttonConfig.buttonConfigMask & 0b00000010) >> 1;
    button.buttonLightsUp = (buttonConfig.buttonConfigMask & 0b00000100) >> 2;
    button.isPOVAngle     = (buttonConfig.buttonConfigMask & 0b00001000) >> 3;
    button.isJoystickAngle = (buttonConfig.buttonConfigMask & 0b00010000) >> 4;
    button.signalInverted = (buttonConfig.buttonConfigMask & 0b00100000) >> 5;
    button.buttonBeginsOn = (buttonConfig.buttonConfigMask & 0b01000000) >> 6;
    button.unused2        = (buttonConfig.buttonConfigMask & 0b10000000) >> 7;

    Serial.printf("button %i: ", i);
    Serial.printf("buttonID - %i | ", button.buttonID);
    Serial.printf("buttonActive = %d | ", button.buttonActive);
    Serial.printf("buttonSticky = %d | ", button.buttonSticky);
    Serial.printf("buttonLightsUp = %d | ", button.buttonLightsUp);
    Serial.printf("isPOVAngle = %d | ", button.isPOVAngle);
    Serial.printf("isJoystickAngle = %d | ", button.isJoystickAngle);
    Serial.printf("signalInverted = %d | ", button.signalInverted);
    Serial.printf("buttonBeginsOn = %d | ", button.buttonBeginsOn);
    Serial.printf("unused = %d\n", button.unused2);
  }

  file.close();
}

void initializeConfigIDs() {
  for (decltype(configStruct::buttonID) i = 0; i < numberOfPossibleButtons; i++) {
    config[i].buttonID = i;
  }
}