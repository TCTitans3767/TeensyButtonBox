//
// Created by jc on 10/23/25.
//

#ifndef TEENSYBUTTONBOX_BUTTONBOX_TEENSY_CPP_H
#define TEENSYBUTTONBOX_BUTTONBOX_TEENSY_CPP_H

#endif //TEENSYBUTTONBOX_BUTTONBOX_TEENSY_CPP_H

#define configStructByteLength (sizeof(configStruct::buttonID) + sizeof(configStruct::buttonConfigMask) + sizeof(configStruct::groupID))
#define numberOfPossibleButtons 21

const uint16_t ledPinMap[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
const uint16_t buttonPinMap[] = {21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41};

 struct configStruct {
  uint16_t buttonID;
  uint16_t groupID;
  char buttonConfigMask;
} config[numberOfPossibleButtons];

struct buttonGroup {
  decltype(configStruct::buttonID) currentActiveButton;
} buttonGroups[255];

class Button {
  public:

    static Button* _instance;

    Button() {
      _instance = this;
    }

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
    int previousCheck = LOW;
    elapsedMillis elapsedMillis = 0;

    void begin() {
      pinMode(ledPinMap[buttonID], OUTPUT);
      Serial.printf("setting pin %i to OUTPUT\n", ledPinMap[buttonID]);
      pinMode(buttonPinMap[buttonID], INPUT_PULLDOWN);
      Serial.printf("setting pin %i to INPUT\n", buttonPinMap[buttonID]);
      if (buttonBeginsOn) {
        buttonOn = true;
        buttonLightOn = (buttonLightsUp) & !buttonLightOn;
        digitalWrite(ledPinMap[buttonID], buttonLightOn ? HIGH : LOW);
        Joystick.button(buttonID, buttonOn);
      }
    }

    void checkForInterrupts() {
      if (elapsedMillis < 15) {
        return;
      }
      int newCheck = digitalRead(buttonPinMap[buttonID]);
      if ((groupID != 0) && (buttonGroups[groupID].currentActiveButton != this->buttonID)) {
        this->buttonLightOn = false;
        this->buttonOn = false;
        digitalWrite(ledPinMap[buttonID], buttonLightOn ? HIGH : LOW);
        Joystick.button(buttonID, buttonOn);
      }
      if (buttonSticky) {
        if ((previousCheck < newCheck) &&(previousCheck != newCheck)) {
          Serial.printf("%i %i : ", previousCheck, newCheck);
          this->handleInterrupt();
        }
      } else {
        if (previousCheck != newCheck) {
          this->handleInterrupt();
        }
      }
      previousCheck = newCheck;
      elapsedMillis = 0;
    }

    void handleInterrupt() {
      if (!buttonActive) {
        return;
      }
      if (groupID == 0) {
        buttonOn = !buttonOn;
        buttonLightOn = (buttonLightsUp) & !buttonLightOn;
        digitalWrite(ledPinMap[buttonID], buttonLightOn ? HIGH : LOW);
        Joystick.button(buttonID, buttonOn);
      } else {
        buttonGroups[groupID].currentActiveButton = this->buttonID;
        buttonOn = buttonGroups[groupID].currentActiveButton == this->buttonID;
        buttonLightOn = (buttonLightsUp) & buttonGroups[groupID].currentActiveButton == this->buttonID;
        digitalWrite(ledPinMap[buttonID], buttonLightOn ? HIGH : LOW);
        Joystick.button(buttonID, buttonOn);
      }
      Serial.printf("Button %i pressed\n", buttonID);
    }

  private:

    static void staticHandleInterrupt() {
      _instance->handleInterrupt();
    }


} buttons[numberOfPossibleButtons];


void initializeConfigIDs();

void writeToConfigFile(const char * filepath, configStruct * config);

void readFileToConfig(const char * filepath);

void exitProgrammingMode();

decltype(configStruct::buttonID) hexStringToInt(char * str, size_t size, size_t size1);
