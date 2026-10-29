#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ReactESP.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

using namespace reactesp;

EventLoop event_loop;

//buttons
const int BUTTON_UP = 25;
const int BUTTON_DOWN = 26;
const int BUTTON_SELECT = 27;
const int BUTTON_BACK = 13;

const int BATTERY = 39;
const int REED_SWITCH = 34;

const float Rtop = 100000.0;
const float Rbottom = 100000.0;
const float DIV = Rbottom / (Rtop + Rbottom);

float calib = 0.98;

unsigned long lastReedFall[2] = { 0, 0 };
int reedFallCounter = 0;
unsigned int stopWatch[3] = { 0, 0, 0 };

bool riding_mode = false;

const int SCREEN_WIDTH = 128;  // OLED width, in pixels
const int SCREEN_HEIGHT = 64;  // OLED height, in pixels

enum MENU_DEPTH {
  MAIN_MENU,
  SUB_MENU,
  SUB_SUB_MENU,
  FILE_MANAGER,
  RIDING
};
enum MENU_DEPTH_ACTIONS {
  INCREASE,
  DECREASE,
  GO_TO_MAIN_MENU,
  GO_INTO_RIDE_MODE
};
enum MAIN_MENU_ELEMENTS {
  START_RIDE,
  STATISTICS,
  RECORDS,
  SETTINGS,
  NOT_IN_SUB_MENU,
  NOT_IN_SUB_SUB_MENU
};
enum SUB_SUB_MENU_SETTINGS {
  BATTERY_CALIBRATION,
  BUTTONS_MAP,
  FILE_SYSTEM_HANDLE
};
enum BUTTONS {
  UP,
  DOWN,
  SELECT,
  BACK,
  PRESSED,
  NOT_PRESSED
};
enum ERRORS {
  CURSOR_OUT_OF_RANGE,
  PAGE_DO_NOT_EXIST
};
enum REED_DETECTION {
  LAST_READ,
  PREVIOUS_REF_POINT,
  TIME
};
enum TIME_ELEMENTS {
  HOURS,
  MINUTES,
  SECONDS
};
enum BUTTONS_COMBOS {
  SUPER_BACK,
  COMBO_WASNT_PRESSED
};
BUTTONS pressedButton;

class Debug {
private:

public:
  void whatIsHappening(int whatToPrint, const char whatToSay[]) {
    Serial.print(whatToSay);
    Serial.print(": ");
    Serial.print(whatToPrint);
    Serial.print("\n");
  }
};

class MyDispaly : public Adafruit_SSD1306 {
private:
  uint8_t *frontBuffer;
  uint8_t *backBuffer;

public:
  MyDispaly(int w, int h, TwoWire *twi = &Wire, int8_t rst_pin = -1)
    : Adafruit_SSD1306(w, h, twi, rst_pin) {}

  void setBuffers(uint8_t *front, uint8_t *back) {
    frontBuffer = front;
    backBuffer = back;
    this->buffer = frontBuffer;
  }

  void setBuffer(uint8_t *buffer) {
    this->buffer = buffer;
  }

  void swapBuffers() {
    uint8_t *temp = frontBuffer;
    frontBuffer = backBuffer;
    backBuffer = temp;
    this->buffer = frontBuffer;
  }

  void show() {
    this->display();
  }

  uint8_t *getBackBuffer() {
    return backBuffer;
  }
};

MyDispaly display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
uint8_t bufferA[1024];
uint8_t bufferB[1024];

class HANDLE_ON_DISPLAY_PRINT {
private:
  Debug debug;
public:

  //default position on the screen. Y - 128 X - 64
  int cursorPositionY = 1;
  int cursorPositionX = 1;
  const int cursorPosBattX = 50;
  const int cursorPosBattY = 95;
  int textColor = 1;  //white color
  int textSizeSmall = 1;
  int textSizeBig = 2;
  const bool CLEAR_DISPLAY = true;
  const bool DONT_CLEAR_DISPLAY = false;

  const int menuLength = 3;
  const int sub_menu_length = 2;

  const char *print_this_menu[4];

  //maine menu
  const char main_menu[4][30] = { { "Start ride" }, { "Statistics" }, { "Records" }, { "Settings" } };

  //sub menues
  //1.Start ride - 0; 2.Milage - 1; 3.Records - 2; 4.Settings - 3
  const char sub_menu[4][3][40] = {
    { { "No ride menu yet" }, { "Empty slot 2" }, { "Empty slot 3" } },
    { { "No milage menu yet" }, { "Empty slot 2" }, { "Empty slot 3" } },
    { { "No records menu yet" }, { "Empty slot 2" }, { "Empty slot 3" } },
    { { "Battery calibration" }, { "Buttons map" }, { "File manager" } }
  };

  //file manager
  const char file_manager_items[4][30] = { { "Initialize FS" }, { "Create and write JSON" }, { "Read JSON" }, { "Print JSON" } };

  //errors
  const char error_message[5][100] = { { "ERROR: Cursor is out of range!" }, { "ERROR! Sub menu in development" }, { "Menu.json load failed" }, { "Deserialization error" }, { "LittleFS initialization failed" } };

  //cursor by default is 0, meaning on the first menu item
  int cursor = 0;
  int sub_menu_cursor = 0;

  void printMessage(int textSize, int textColor, int cursorPosX, int cursorPosY, auto const *textToPrint, bool clearOled, bool readyToPrint) {
    if (clearOled) {
      display.setBuffer(display.getBackBuffer());
      display.clearDisplay();
    }
    display.setTextSize(textSize);
    display.setTextColor(textColor);
    display.setCursor(cursorPosX, cursorPosY);
    display.print(*textToPrint);
    if (readyToPrint) {
      display.swapBuffers();
      display.show();
    }
  }
  float CurrentBatteryCharge() {
    const int samples = 16;
    uint32_t sum = 0;
    float cal = calib;
    for (int i = 0; i < samples; ++i) {
      sum += analogRead(BATTERY);
      delay(2);
    }
    float raw = sum / float(samples);
    float vadc = (raw / 4095.0f) * 3.60f;
    float vbat = vadc / DIV * cal;




    return vbat;
  }
  void get_main_menu_array() {
    for (int i = 0; i <= menuLength; ++i) {
      print_this_menu[i] = main_menu[i];
    }
  }
  void get_sub_menu_array(int *selected_sub_menu) {
    debug.whatIsHappening(*selected_sub_menu, "Getting sub menu array (selected_sub_menu)");
    for (int i = 0; i <= sub_menu_length; ++i) {
      print_this_menu[i] = sub_menu[*selected_sub_menu][i];
    }
  }
  void choose_menu_to_print(int *menu_depth, int *selected_sub_menu) {
    switch (*menu_depth) {
      case MAIN_MENU: get_main_menu_array(); break;
      case SUB_MENU: get_sub_menu_array(selected_sub_menu); break;
    }
  }


  void printMenu(int *cursor, const char *menu[], int menu_length) {
    bool ready_to_print = false;
    display.setBuffer(display.getBackBuffer());
    display.clearDisplay();
    for (int i = 0; i <= menu_length; ++i) {
      if (i == menu_length) {
        ready_to_print = true;
      }
      if (i != *cursor) {
        printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, &menu[i], DONT_CLEAR_DISPLAY, ready_to_print);
      }
      if (i == *cursor) {
        printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, ">", DONT_CLEAR_DISPLAY, false);
        cursorPositionX += 10;
        printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, &menu[i], DONT_CLEAR_DISPLAY, ready_to_print);
        //cursorPositionY += 3;
        cursorPositionX -= 10;
      }
      cursorPositionY += 14;
    }
    cursorPositionY = 0;
  }
  const int returnMenuLength(int *menu_depth) {
    switch (*menu_depth) {
      case MAIN_MENU: return menuLength;
      case SUB_MENU: return sub_menu_length;
    }
  }
  void print(int *cursor, int *menu_depth, int *selected_sub_menu) {
    choose_menu_to_print(menu_depth, selected_sub_menu);
    printMenu(cursor, print_this_menu, returnMenuLength(menu_depth));
  }


  void printError(int error_code) {
    printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, &error_message[error_code], CLEAR_DISPLAY, true);
  }




  void printSubSubMenu() {
    float batteryCharge = CurrentBatteryCharge();
    printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, "Calibration of battery: ", CLEAR_DISPLAY, false);
    {
      cursorPositionY = 10;
      printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, &calib, DONT_CLEAR_DISPLAY, false);
      cursorPositionY = 0;
    }
    {
      cursorPositionY = 20;
      printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, "The battery voltage:", DONT_CLEAR_DISPLAY, false);
      cursorPositionY = 0;
    }
    {
      cursorPositionY = 30;
      printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, &batteryCharge, DONT_CLEAR_DISPLAY, true);
      cursorPositionY = 0;
    }
  }
};




class FileSystemReadAndWrite {
private:
  int cursorPositionY = 1;
  int cursorPositionX = 1;
  int textColor = 1;  //white color
  int textSizeSmall = 1;
  int textSizeBig = 2;
  int READ_WRITE_ERROR = 4;
  const bool CLEAR_DISPLAY = true;
  const bool DONT_CLEAR_DISPLAY = false;
  bool READY_TO_PRINT = true;
  HANDLE_ON_DISPLAY_PRINT OLED;
  File file;
  StaticJsonDocument<512> doc_array;
  const char confirmation[22] = "Data has been created";
  const char FS_initialized[15] = "FS initialized";




public:




  void initializeFS() {
    if (!LittleFS.begin(false)) {
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
    } else {
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, FS_initialized, CLEAR_DISPLAY, READY_TO_PRINT);
    }
  }




  void createConfigFile() {
    file = LittleFS.open("/config.json", "w");
    if (!file) {
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
      delay(1500);
    } else {
      //0 means not exist; 1 means exist
      JsonDocument doc;
      doc["main_menu"] = "0";
      doc["sub_menu"] = "0";
      doc["sub_sub_menu"] = "0";
      doc["language"] = "en";
      //doc[""]




      serializeJson(doc, file);
      file.close();
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, confirmation, CLEAR_DISPLAY, READY_TO_PRINT);
      delay(1500);
    }
  }




  void createMenusFile() {
    //---MAIN MENU---
    file = LittleFS.open("/main_menu.json", "w");
    if (!file) {
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
    } else {
      JsonArray data = doc_array["main_menu"].to<JsonArray>();
      data.add("Start ride");
      data.add("Statistics");
      data.add("Settings");




      serializeJson(doc_array, file);
      file.close();
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, confirmation, CLEAR_DISPLAY, READY_TO_PRINT);
      delay(1500);
    }
    //---SUB MENU---
    file = LittleFS.open("/sub_menu.json", "w");
    if (!file) {
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
    } else {
      JsonArray statistics = doc_array["statistics"].to<JsonArray>();
      statistics.add("Total driven distance (meters)");
      statistics.add("Top speed (km/h)");
      statistics.add("Total driven time");




      JsonArray settings = doc_array["Settings"].to<JsonArray>();
      settings.add("Battery calibration");
      settings.add("Button maping");
      settings.add("Language");
      settings.add("Reset statistics");




      serializeJson(doc_array, file);
      file.close();
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, confirmation, CLEAR_DISPLAY, READY_TO_PRINT);
      delay(1500);
    }
    //---SUB SUB MENU---
    //---STATISTICS---
    file = LittleFS.open("/statistics_data.json", "w");
    if (!file) {
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
    } else {
      JsonDocument doc;
      doc["total_driven_distance"] = "0";
      doc["top_speed"] = "0";
      doc["total_driven_time"] = "0";




      serializeJson(doc, file);
      file.close();
      doc.clear();
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, confirmation, CLEAR_DISPLAY, READY_TO_PRINT);
    }
    //---BATTERY CALIBRATION---
    file = LittleFS.open("/battery_calibration.json", "w");
    if (!file) {
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
    } else {
      JsonDocument doc;
      doc["battery_offset_title:"] = "Battery offset:";
      doc["battery_offset"] = "0.98";  //default value
      doc["battery_charge_title"] = "Battery charge:";




      serializeJson(doc, file);
      file.close();
      doc.clear();
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, confirmation, CLEAR_DISPLAY, READY_TO_PRINT);
    }
    //---BUTTONS MAPPING---
    file = LittleFS.open("/buttons_mapping.json", "w");
    if (!file) {
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
    } else {
      JsonDocument doc;
      doc["pin_25"] = "BUTTON_UP";
      doc["pin_26"] = "BUTTON_DOWN";
      doc["pin_27"] = "BUTTON_SELECT";
      doc["pin_13"] = "BUTTON_BACK";




      serializeJson(doc, file);
      file.close();
      doc.clear();
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, confirmation, CLEAR_DISPLAY, READY_TO_PRINT);
    }
    //---SYSTEM LANGUAGE---
    file = LittleFS.open("/system_language.json", "w");
    if (!file) {
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
    } else {
      //1 means the language is selected
      JsonDocument doc;
      doc["english"] = "1";
      doc["german"] = "0";
      doc["polish"] = "0";




      serializeJson(doc, file);
      file.close();
      doc.clear();
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, confirmation, CLEAR_DISPLAY, READY_TO_PRINT);
    }
    //---RESET SYSTEM---
    file = LittleFS.open("/reset_system.json", "w");
    if (!file) {
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
    } else {
      JsonDocument doc;
      doc["confirm_reset"] = "To proceed press SELECT";
      doc["reset_flag"] = 0;




      serializeJson(doc, file);
      file.close();
      doc.clear();
      OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, confirmation, CLEAR_DISPLAY, READY_TO_PRINT);
    }
  }




  void readMenu(char menuOrder[][30], int menuLevel, int CountOfMenuItems) {
    //---if ordered menu is an array---
    if (menuLevel == 0 || menuLevel == 1) {
      file = LittleFS.open("/main_menu.json", "r");
      {
        if (!file) {
          OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
        } else {
          DeserializationError error = deserializeJson(doc_array, file);
          if (error) {
            OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
          } else {
            for (int i = 0; i <= CountOfMenuItems; ++i) {
              const char *item = doc_array["main_menu"][i];
              strncpy(menuOrder[i], item, 29);
            }
            //JsonArray menu = doc[menuLevel].as<JsonArray>();
            /*for(int i = 0; i <menu.size() && i < CountOFMenuItems; ++i)
         {
           const char *item = menu[i]; //get string from Json
           strncpy(menuOrder[i], item, 29); //copy in the array
           menuOrder[i][29] = '\0'; //just to be sure there is a terminator in the end
         }*/
          }
        }
      }
    }
    //---if oredered menu or whatever is not an array---
    /*else
 {
   file = LittleFS.open(menuLevel, "r");
   if(!file)
   {
     OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
   }
   else
   {
     DeserializationError error = deserializeJson(doc, file);
     if(error)
     {
       OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, OLED.error_message[READ_WRITE_ERROR], CLEAR_DISPLAY, READY_TO_PRINT);
     }
     else
     {
       //JsonDocument menu = doc[]
     }
   }
 }*/
  }
};




class RIDE {
private:
  const double wheelCircumference = 2.0;  //in meters. The actual length is 208 cm
  const static int ONE_SECOND = 1000;
  //const static int ONE_SECOND_HIGH_END = 1150;
  bool readyToPrint = false;
  int cursorPositionY = 1;
  int cursorPositionX = 1;
  const int cursorPosBattX = 50;
  const int cursorPosBattY = 95;
  int textColor = 1;  //white color
  int textSizeSmall = 1;
  int textSizeBig = 2;
  const bool CLEAR_DISPLAY = true;
  const bool DONT_CLEAR_DISPLAY = false;
  unsigned long trackOfTime = 0;
  unsigned long distance = 0;
  HANDLE_ON_DISPLAY_PRINT OLED;

public:
  //unsigned long temp_time;
  //int RPM;

  double speed;

  void turnOnRidingMode() {
    riding_mode = true;
  }

  void turnOffRidingMode() {
    riding_mode = false;
  }

  void calculateSpeed() {
    //speed in km/h
    speed = (reedFallCounter * wheelCircumference) * 3.6;
  }

  void calculateDistance() {
    distance += wheelCircumference;
  }

  void timeTrack() {
    stopWatch[SECONDS] += 1;
    if (stopWatch[SECONDS] >= 60) {
      stopWatch[SECONDS] = 0;
      stopWatch[MINUTES] += 1;
      if (stopWatch[MINUTES] >= 60) {
        stopWatch[MINUTES] = 0;
        stopWatch[HOURS] += 1;
      }
    }
  }

  void printSpeed() {
    cursorPositionX = 20;
    cursorPositionY = 40;
    OLED.printMessage(textSizeBig, textColor, cursorPositionX, cursorPositionY, &speed, CLEAR_DISPLAY, false);
    cursorPositionX = 50;
    cursorPositionY = 80;
    OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, &stopWatch[SECONDS], DONT_CLEAR_DISPLAY, false);
    cursorPositionX = 50;
    cursorPositionY = 70;
    OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, &stopWatch[MINUTES], DONT_CLEAR_DISPLAY, false);
    cursorPositionX = 50;
    cursorPositionY = 60;
    OLED.printMessage(textSizeSmall, textColor, cursorPositionX, cursorPositionY, &stopWatch[HOURS], DONT_CLEAR_DISPLAY, true);
  }

  void trackingTime() {
    if (millis() - trackOfTime >= ONE_SECOND) {
      trackOfTime += ONE_SECOND;
      timeTrack();
      calculateSpeed();
      reedFallCounter = 0;
      printSpeed();
    }
  }

  bool isRidingMode() {
    if (riding_mode) {
      return true;
    } else {
      return false;
    }
  }

  void upToSpeed() {
    trackingTime();
  }
};

BUTTONS HANDLE_BUTTONS_PRESSED_BUTTON;
bool buttonFlag = false;

class HANDLE_BUTTONS {
private:

  HANDLE_ON_DISPLAY_PRINT OLED;
  RIDE RIDE_MODE;

public:
  BUTTONS ButtonPressed;
  unsigned long last[5];

  void onButton(BUTTONS pressedButton) {
    //remember what button was pressed
    HANDLE_BUTTONS_PRESSED_BUTTON = pressedButton;
    //set the button flag for activity tracker
    buttonFlag = true;
  }

  bool checkButtons(int *array) {
    bool answer;
    //if asked let know if any button was pressed
    if (buttonFlag) {
      //tell what button was pressed
      if (HANDLE_BUTTONS_PRESSED_BUTTON == UP) {
        array[UP] = PRESSED;
        answer = true;
      }
      if (HANDLE_BUTTONS_PRESSED_BUTTON == DOWN) {
        array[DOWN] = PRESSED;
        answer = true;
      }
      if (HANDLE_BUTTONS_PRESSED_BUTTON == SELECT) {
        array[SELECT] = PRESSED;
        answer = true;
      }
      if (HANDLE_BUTTONS_PRESSED_BUTTON == BACK) {
        array[BACK] = PRESSED;
        answer = true;
      }
      buttonFlag = false;
    } else {
      answer = false;
      return answer;
    }

    return answer;
  }
};

class HANDLE_SUB_MENU {
private:
  HANDLE_BUTTONS CHECK_PRESSED_BUTTON;
  HANDLE_ON_DISPLAY_PRINT OLED;
  int pressed_buttons[4] = { NOT_PRESSED, NOT_PRESSED, NOT_PRESSED, NOT_PRESSED };
  int file_manager_cursor = 0;

public:

  void batteryCalibration(int *menu_depth) {
    OLED.printSubSubMenu();
    CHECK_PRESSED_BUTTON.checkButtons(pressed_buttons);

    if (pressed_buttons[UP] == PRESSED) {
      calib += 0.01;
      OLED.printSubSubMenu();
      pressed_buttons[UP] = NOT_PRESSED;
    }
    if (pressed_buttons[DOWN] == PRESSED) {
      calib -= 0.01;
      OLED.printSubSubMenu();
      pressed_buttons[DOWN] = NOT_PRESSED;
    }
    if (pressed_buttons[BACK] == PRESSED) {
      *menu_depth = SUB_MENU;
      pressed_buttons[BACK] = NOT_PRESSED;
    }
  }

  int checkSelectedSubMenu(int *selectedSubMenuItem) {
    if (*selectedSubMenuItem == START_RIDE) {
      return START_RIDE;
    }
    if (*selectedSubMenuItem == STATISTICS) {
      return STATISTICS;
    }
    if (*selectedSubMenuItem == RECORDS) {
      return RECORDS;
    }
    if (*selectedSubMenuItem == SETTINGS) {
      return SETTINGS;
    }
  }

  void checkSelectedSubSubMenu(int *selectedSubMenuItem, int *selectedSubSubMenuItem, int *menu_depth) {
    if (*selectedSubMenuItem == STATISTICS) {
      return;
    }
    if (*selectedSubMenuItem == RECORDS) {
      return;
    }
    if (*selectedSubMenuItem == SETTINGS) {
      if (*selectedSubSubMenuItem == BATTERY_CALIBRATION) {
        batteryCalibration(menu_depth);
      }
      if (*selectedSubSubMenuItem == FILE_SYSTEM_HANDLE) {
        *menu_depth = FILE_MANAGER;
      }
    }
  }
};

class NAVIGATION {
private:
  HANDLE_BUTTONS PRESSED_BUTTONS;
  HANDLE_ON_DISPLAY_PRINT OLED;
  HANDLE_SUB_MENU SUB_MENU_HANDLER;

  const int buttons_count = 4;
  int buttons[buttons_count] = { NOT_PRESSED, NOT_PRESSED, NOT_PRESSED, NOT_PRESSED };

  const bool increase = true;
  const bool decrease = false;

  const int cursors_num = 4;
  //0 - main menu, 1 - sub menu, 2 - sub sub menu, 3 - file manager_cursor
  int cursors[cursors_num] = { 0, 0, 0, 0 };

  const int length_for_menus = 2
    //0 - main menu, 1 - sub menu, 2 - sub sub menu, 3 - file manager
    const int menu_length[length_for_menus] = { 3, 2, 0, 0 };

  //by default riding is off
  bool riding_mode = false;

  //by default 0 - main menu
  int menu_depth = 0;

  //by default 4 - not in a sub menu
  int selected_sub_menu = NOT_IN_SUB_MENU;

  //by default 5 - not in sub sub menu
  int selected_sub_sub_menu = NOT_IN_SUB_SUB_MENU;

  const int menu_levels = 4;
  bool first_menu_print = true;
  //0 - main menu, 1 - sub menu, 2 - sub sub menu, 3 - file manager
  bool menu_print_state[menu_levels] = { true, true, true, true };

public:

  void cursorPosition(int button, int cursor, int menu) {
    //UP pressed and the cursor is not on the first menu item - move it one position up
    if (button == UP && cursor > 0) {
      cursors[cursor] -= 1;
    }
    //Up pressed and the cursor is on the first menu item - move it to the last menu item
    else if (button == UP && cursor == 0) {
      cursors[cursor] = menu_length[menu];
    }
    //DOWN pressed and the cursor is not on the last menu item - move cursor one position down
    else if (button == DOWN && cursor != menu_length[menu]) {
      cursors[cursor] += 1;
    }
    //DOWN pressed and the cursor is on the last menu item - move the cursor to the first menu position
    else if (button == DOWN && cursor == menu_length[menu]) {
      cursors[cursor] = 0;
    }
  }

  //---CHANGE MENU DEPTH---
  void change_menu_depth(MENU_DEPTH_ACTIONS ACTION) {
    //increase_decrease = true - meanse increase menu depth; increase_decrease = false - mease decrease menu depth
    switch (ACTION) {
      case INCREASE: ++(*menu_depth); break;
      case DECREASE: --(*menu_depth); break;
      case GO_TO_MAIN_MENU: *menu_depth = MAIN_MENU; break;
      case GO_INTO_RIDE_MODE: *menu_depth = RIDING; break;
    }
  }

  //---GET SELECTED MENU POSITION---
  void getSelectedMenuPosition() {
    *selected_menu_position = *cursor;
  }

  //---CHECK IF CAN BE INCREASED---
  bool canBeIncreased() {
    if (*menu_depth < SUB_SUB_MENU) {
      return true;
    } else {
      return false;
    }
  }

  //---CHECK IF CAN BE DECREASED---
  bool canBeDecreased() {
    if (menu_depth > MAIN_MENU) {
      return true;
    } else {
      return false;
    }
  }

  //---INCREASE MENU DEPTH---
  void increaseMenuDepth() {
    if (SUB_MENU_HANDLER.checkSelectedSubMenu(cursor) == START_RIDE) {
      change_menu_depth(GO_INTO_RIDE_MODE);
    } else {
      change_menu_depth(INCREASE);
      getSelectedMenuPosition();
    }
  }

  //---DECREASE MENU DEPTH---
  void decreaseMenuDepth() {
    change_menu_depth(DECREASE);
  }

  //---CHECK WHAT CAN BE DONE WITH MENU DEPTH---
  void check_available_actions(MENU_DEPTH_ACTIONS ACTION) {
    if (ACTION == INCREASE) {
      if (canBeIncreased()) {
        increaseMenuDepth();
      } else {
        return;
      }
    }
    if (ACTION == DECREASE) {
      if (canBeDecreased()) {
        decreaseMenuDepth();
      } else {
        return;
      }
    }
  }

  void whatMenuJustPrinted() {
    for (int i = 0; i <= SUB_SUB_MENU) {
      if (menu_depth == i) {
        menu_print_state[i] = false;
      }
    }
    first_menu_print = false;
  }

  void first_print() {
    whatMenuJustPrinted();
    OLED.print(cursor, menu_depth, selected_sub_menu);
  }

  int checkCombo() {
    //gotta add time of press check!
    if (buttons[UP] == PRESSED && buttons[BACK] == PRESSED) {
      return SUPER_BACK;
    } else {
      return COMBO_WASNT_PRESSED;
    }
  }

  void executeCombo() {
    switch (checkCombo()) {
      default:
      case COMBO_WASNT_PRESSED: break;
      case SUPER_BACK: change_menu_depth(GO_TO_MAIN_MENU); break;
    }
  }

  int isComboPressed() {
    return checkCombo();
  }

  void changeCursorPosition(BUTTONS button) {
    switch (menu_depth) {
      case MAIN_MENU: cursorPosition(button, MAIN_MENU, MAIN_MENU); break;
      case SUB_MENU: cursorPosition(button, SUB_MENU, SUB_MENU); break;
      case FILE_MANAGER: cursorPosition(button, FILE_MANAGER, FILE_MANGER); break;
    }
  }

  void moveCursorUp() {
    changeCursorPosition(UP);
    buttons[UP] = NOT_PRESSED;
  }

  void moveCursorDown() {
    changeCursorPosition(DOWN);
    buttons[DOWN] = NOT_PRESSED;
  }

  void moveCursor() {
    if (buttons[UP] == PRESSED) {
      moveCursorUp();
    }
    if (buttons[DOWN] == PRESSED) {
      moveCursorDown();
    } else {
      return;
    }
  }

  bool selectWasPressed() {
    if (buttons[SELECT] == PRESSED) {
      buttons[SELECT] = NOT_PRESSED;
      return true;
    } else {
      return false;
    }
  }

  bool backWasPressed() {
    if (buttons[BACK] == PRESSED) {
      buttons[BACK] = NOT_PRESSED;
      return true;
    } else {
      return false;
    }
  }

  void moveThroughMenu() {
    if (selectWasPressed()) {
      check_available_actions(INCREASE);
    }
    if (backWasPressed()) {
      check_available_actions(DECREASE);
    } else {
      return;
    }
  }

  void doWhatIsPressed() {
    //step 0. Get pressed buttons
    PRESSED_BUTTONS.checkButtons(buttons);
    //step 1. Check for combos
    if (isComboPressed() != COMBO_WASNT_PRESSED) {
      executeCombo();
      return;
    } else {
      moveCursor();
      moveThroughMenu(menu_depth, selected_menu_position, cursor);
      OLED.print(cursor, menu_depth, selected_menu_position);
    }
  }

  void navigate_menu() {
    if (first_menu_print) {
      first_print();
    } else {
      doWhatIsPressed();
    }
  }
};

class HANDLE_MENU {

private:

  //HANDLE_BUTTONS Handle_Buttons;
  //HANDLE_ON_DISPLAY_PRINT OLED;
  HANDLE_SUB_MENU SUB_MENUS;
  NAVIGATION Navigate;
  Debug debug;
  FileSystemReadAndWrite File_Syste_Handler;

public:

  void main_menu() {
    //---debug---//
    debug.whatIsHappening(selected_sub_menu, "void main_menu. selected_sub_menu");
    debug.whatIsHappening(selected_sub_sub_menu, "viod main_menu. selected_sub_sub_menu");

    Navigate.navigate_menu(&main_menu_cursor, &menu_depth, &selected_sub_menu, &main_menu_length, &first_main_menu_print);
  }

  void sub_menu() {
    //---debug---//
    debug.whatIsHappening(selected_sub_menu, "void sub_menu. selected_sub_menu");
    debug.whatIsHappening(selected_sub_sub_menu, "viod sub_menu. selected_sub_sub_menu");

    Navigate.navigate_menu(&sub_menu_cursor, &menu_depth, &selected_sub_sub_menu, &sub_menu_length, &first_sub_menu_print);
  }

  void sub_sub_menu() {
    //---debug---//
    debug.whatIsHappening(selected_sub_menu, "void sub_sub_menu. selected_sub_menu");
    debug.whatIsHappening(selected_sub_sub_menu, "viod sub_sub_menu. selected_sub_sub_menu");
    //step 1. Check to which sub menu should be displayed list of sub sub menus and take actions from there
    SUB_MENUS.checkSelectedSubSubMenu(&selected_sub_menu, &selected_sub_sub_menu, &menu_depth);
  }

  void file_manager() {
  }

  void whereWeAreAt() {
    //check the menu depth and call a respective method
    if (menu_depth == MAIN_MENU) { main_menu(); }
    if (menu_depth == SUB_MENU) { sub_menu(); }
    if (menu_depth == SUB_SUB_MENU) { sub_sub_menu(); }
    if (menu_depth == RIDING) {}
    if (menu_depth == FILE_MANAGER) { file_manager(); }
  }

  void status_check() {
    //step 1. Let's see where we are and let's take some actions from there
    whereWeAreAt();
  }
};

HANDLE_BUTTONS Handle_Buttons;
HANDLE_ON_DISPLAY_PRINT OLED;
RIDE Ride;
HANDLE_MENU MENU;
FileSystemReadAndWrite FS_Handling;

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // Address 0x3C is common
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Infinite loop on failure
  }
  display.setBuffers(bufferA, bufferB);

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY, ADC_11db);

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_BACK, INPUT_PULLUP);
  pinMode(REED_SWITCH, INPUT);

  //async press detecting
  event_loop.onInterrupt(BUTTON_UP, FALLING, [] {
    unsigned long now = millis();
    if (now - Handle_Buttons.last[UP] < 250) return;
    Handle_Buttons.last[UP] = now;
    Handle_Buttons.onButton(UP);
  });
  event_loop.onInterrupt(BUTTON_DOWN, FALLING, [] {
    unsigned long now = millis();
    if (now - Handle_Buttons.last[DOWN] < 250) return;
    Handle_Buttons.last[DOWN] = now;
    Handle_Buttons.onButton(DOWN);
  });
  event_loop.onInterrupt(BUTTON_SELECT, FALLING, [] {
    unsigned long now = millis();
    if (now - Handle_Buttons.last[SELECT] < 250) return;
    Handle_Buttons.last[SELECT] = now;
    Handle_Buttons.onButton(SELECT);
  });
  event_loop.onInterrupt(BUTTON_BACK, FALLING, [] {
    unsigned long now = millis();
    if (now - Handle_Buttons.last[BACK] < 250) return;
    Handle_Buttons.last[BACK] = now;
    Handle_Buttons.onButton(BACK);
  });

  //async reed switch close detection
  event_loop.onInterrupt(REED_SWITCH, FALLING, [] {
    unsigned long now = millis();
    if (now - lastReedFall[LAST_READ] < 100) return;
    lastReedFall[LAST_READ] = now;
    reedFallCounter += 1;
  });
}

void loop() {
  event_loop.tick();
  MENU.status_check();
}
