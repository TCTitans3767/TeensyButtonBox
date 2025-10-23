#include<Arduino.h>
#include <SD.h>
#include "ButtonBox_Teensy.cpp.h"

const int chipSelect = 1; 

const char *filepath = "/config";

File configFile;

bool programmingMode = false;

/* 0000000x = is button active
/ 000000x0 = is button sticky
/ 00000x00 = does button light up
/ 0000x000 = does button return pov angle (currently unused)
/ 000x0000 = does button return joystick angle (currently unused)
/ 00x00000 = is button signal inverted
/ 0x000000 = does button start activated
/ x0000000 = unused
*/

Button* Button::_instance = nullptr;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  elapsedMillis waitForSerial = 0;
  while (!Serial) {
    // wait for serial port to connect
    if (waitForSerial > 2000) {
      break;
    }
  }

  Joystick.useManualSend(true);

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

  if (programmingMode) {
    // programming mode code here
    // while(true) {
    //   if (Serial.available() > 0) {
    //     break;
    //   }
    // }

    if ((Serial.available() > 0) && (uint)Serial.available() <= ((configStructByteLength * 2) + 4 /*add 2 for the two required "/"'s in the programming command and 1 for newline operator*/)) {

      if (Serial.available() == 4) {
        char stringBuffer[2];
        Serial.readBytes(stringBuffer, 2);

        Serial.println(stringBuffer);

        if (strcmp(stringBuffer , "!q") == 0) {
          Serial.println("Exiting Programming Mode");
          readFileToConfig(filepath);
          exitProgrammingMode();
          goto skipRestOfLoop;
        } else if (strcmp(stringBuffer , "wq") == 0) {
          exitProgrammingMode();
          goto skipRestOfLoop;
        } else {
          goto skipRestOfLoop;
        }
      }

      char buttonIDBuffer[sizeof(configStruct::buttonID)];
      char groupIDBuffer[sizeof(configStruct::groupID)];
      char configMaskBuffer[sizeof(configStruct::buttonConfigMask)];

      Serial.readBytesUntil(0x2F, buttonIDBuffer, (sizeof(configStruct::buttonID) * 2) + 1);
      Serial.read();
      Serial.readBytesUntil(0x2F, groupIDBuffer, (sizeof(configStruct::groupID) * 2) + 1);
      Serial.read();
      Serial.readBytesUntil(0x0A, configMaskBuffer, (sizeof(configStruct::buttonConfigMask) * 2) + 1);

      // String buttonIDString = Serial.readStringUntil("/", 5);
      // String groupIDString = Serial.readStringUntil("/", 5);
      // String configMaskString = Serial.readStringUntil(0x0A, 5);

      // Serial.println(buttonIDString);
      // Serial.println(groupIDString);
      // Serial.println(configMaskString);


      decltype(configStruct::buttonID) buttonID = hexStringToInt(buttonIDBuffer, sizeof(buttonIDBuffer), sizeof(configStruct::buttonID));
      Serial.printf("buttonID: %x\n", buttonID);
      config[buttonID].groupID = hexStringToInt(groupIDBuffer, sizeof(groupIDBuffer), sizeof(configStruct::groupID));
      Serial.printf("groupID: %x\n", config[buttonID].groupID);
      config[buttonID].buttonConfigMask = hexStringToInt(configMaskBuffer, sizeof(configMaskBuffer), sizeof(configStruct::buttonConfigMask));
      Serial.printf("config: %x\n", config[buttonID].buttonConfigMask);
    }
    
  } else {

    for (int i = 0; i < numberOfPossibleButtons; i++) {
      buttons[i].checkForInterrupts();
    }

    if (Serial.available() == 0) {
      goto skipRestOfLoop;
    }

    if (Serial.available() > 5) {
      Serial.print("Too many characters in command");
      goto skipRestOfLoop;
    }

    if (Serial.read() == 0x2F) {              // checking for "/" to start reading command
      switch (Serial.read()) {
        case -1:                              // checking for empty buffer, which means no command was input past "/"

          Serial.println("invalid input");
          break;

        case 0x48:                            // checking for lowercase "h"
        case 0x68:                            // chacking for uppercase "H"
          // print list of commands here
          break;

        case 0x50:                            // checking for lowercase "p"
        case 0x70:                            // checking for uppercase "P"
          programmingMode = true; 
          Serial.println("Programming Mode Acitivated");
          Serial.println("Proceed with caution");
          break;
        case 0x52:                            // checking for lowercase "r"
        case 0x72:                            // checking for lowercase "R"
          Serial.println("rebooting now");

          break;
        case 0x42:                            // checking for uppercase "B"
        case 0x62:                            // checking for lowercase "b"
          if (Serial.available() >=1) {
            switch (Serial.read()) {
              case 0x42:                      // uppercase "B"
              case 0x62:                      // lowercase "b"
                Serial.printf("%i\n", sizeof(configStruct::buttonID));
                break;
              case 0x47:                      // uppercase "G"
              case 0x67:                      // lowercase "g"
                Serial.printf("%i\n", sizeof(configStruct::groupID));
                break;
              default:
                break;
            }
          }
          break;
        case 0x54:                            // checking for uppercase "T"
        case 0x74:                            // checking for lowercase "t"
          decltype(configStruct::buttonID) buttonToTest = Serial.read() - 0x30; // converting ascii number to integer value
          Serial.printf("testing button %i\n", buttonToTest);
          testButton(buttonToTest); // converting ascii number to integer value
          break;
      }
    }
  }
  skipRestOfLoop:
  Serial.clear();
}

void exitProgrammingMode() {
  Serial.println("exiting Programming mode");
  writeToConfigFile(filepath, config);
  programmingMode = false;
  interrupts();
}

void writeToConfigFile(const char* fileName, configStruct config[]) {
  SD.remove(fileName);
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

decltype(configStruct::buttonID) hexStringToInt(char* buffer, uint8_t bufferLength, uint8_t sizeOfResult) {
  uint intResult = 0;
  for (int i = 0; i < bufferLength * 2; i++) {
    // Serial.printf("%x ", buffer[i]);
    // Serial.printf("Amount to shift = %d ", (4 * ((sizeOfResult * 2) - i - 1)));
    switch (buffer[i]) {
      case 0x30: intResult ^= 0; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x31: intResult ^= 1; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x32: intResult ^= 2; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x33: intResult ^= 3; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x34: intResult ^= 4; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x35: intResult ^= 5; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x36: intResult ^= 6; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x37: intResult ^= 7; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x38: intResult ^= 8; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x39: intResult ^= 9; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x61:
      case 0x41: intResult ^= 10; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x62:
      case 0x42: intResult ^= 11; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x63:
      case 0x43: intResult ^= 12; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x64:
      case 0x44: intResult ^= 13; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x65:
      case 0x45: intResult ^= 14; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
      case 0x66:
      case 0x46: intResult ^= 15; intResult <<= (4 * ((sizeOfResult * 2) - i - 1)); break;
    }
    Serial.printf("%x\n", intResult);
  }
  return intResult;
}

void testButton(decltype(configStruct::buttonID) buttonID) {
  Button button = buttons[buttonID];
  bool buttonState = button.buttonOn;
  bool lightState = button.buttonLightOn;
  bool buttonActiveState = button.buttonActive;

  button.buttonOn = false;
  button.buttonLightOn = false;
  button.buttonActive = false;

  for (int i = 0; i < 40; i++) {
    button.buttonLightOn = !button.buttonLightOn;
    digitalWrite(ledPinMap[button.buttonID], button.buttonLightOn);
    delay(100);
  }

  button.buttonOn = buttonState;
  button.buttonLightOn = lightState;
  button.buttonActive = buttonActiveState;
  digitalWrite(ledPinMap[buttonID], lightState);
  Serial.println("test complete");
}