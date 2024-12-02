#include <MemoryFree.h>

#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

//python "C:\Users\alpha\OneDrive - Loughborough University\Embedded Systems Programming Coursework\coa202ser.py" COM6

//#define blank 0
#define red 1
#define green 2
#define yellow 3
//#define blue 4
#define violet 5
//#define teal 6
#define white 7

const unsigned long addMask = 0b00010000000101;
const unsigned long pstMask = 0b00010000000100;
const unsigned long grdMask = 0b00010000000100;
const unsigned long salMask = 0b00010000000100;
const unsigned long cjtMask = 0b00010000000100;
const unsigned long delMask = 0b00010000000000;

byte upArrow[8] = {
  0b00000,
  0b00100,
  0b01110,
  0b10101,
  0b00100,
  0b00100,
  0b00100,
  0b00000
};

byte downArrow[8] = {
  0b00000,
  0b00100,
  0b00100,
  0b00100,
  0b10101,
  0b01110,
  0b00100,
  0b00000
};

byte defaultBacklight = yellow;

bool displayingStudentID = false;
bool displayingAccounts = false;

enum State { SyncPhase,
             MainPhase };

enum displayState { displayDefault,
                    displayStudentID };

enum Functions { IDL,
                 ADD,
                 PST,
                 GRD,
                 SAL,
                 CJT,
                 DEL };

struct account {
  unsigned long id : 24;   //7 digit decimal
  unsigned int grade : 4;  //single digit 1 to 9
  char title[18];          // 17 chars pplus a null terminator
  unsigned long salary;    // max 9999999, store in pence
  bool pension : 1;
};

byte accountsSize = 0;  //256 max
byte accountIndex = 0;
const byte maxAccountsSize = 40;
account accounts[maxAccountsSize];  //accouts

State currentState = SyncPhase;
displayState currentDisplay = displayDefault;
Functions currentFunction = IDL;

unsigned long lastTime = 0;
String msg;
//msg.reserve(32);

bool holdingButton = false;
unsigned long buttonHoldTime = 0;
bool updateDisplay = false;
bool scroll = false;
unsigned long scrollTime;

unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 300;



bool insertAccount(account newAccount) {

  if (accountsSize == 0) {
    accounts[0] = newAccount;
    accountsSize++;
    accountIndex = 0;
    return true;
  }

  if (accountsSize < maxAccountsSize) {
    int i;
    for (i = accountsSize - 1; i >= 0; i--) {

      if (newAccount.id < accounts[i].id) {
        accounts[i + 1] = accounts[i];
      }

      else {
        break;
      }
    }

    accounts[i + 1] = newAccount;
    accountsSize++;
    accountIndex = i + 1;
    return true;
  }

  else {
    return false;
  }
}



bool checkDashes(String msg, int bitMask) {

  int len = msg.length();

  for (int i = 0; i < len and i < 14; ++i) {  // 13 is the highest dassh index
    bool hasDash = (bitMask >> (14 - i - 1)) & (1);

    if ((msg[i] == '-' and !hasDash) or (msg[i] != '-' and hasDash)) {
      return false;
    }
  }
  return true;
}



bool verifyTitle(char* title) {
  for (int i = 0; title[i] != '\0'; ++i) {  // Iterate until the null terminator is found
    char c = title[i];
    if (!((c >= 'A' and c <= 'Z') or ((c >= 'a' and c <= 'z')) or c == '_' or c == '.')) {
      return false;
    }
  }
  return true;
}



int findAccount(unsigned long id) {
  for (int i = 0; i < accountsSize; i++) {
    if (id == accounts[i].id) {
      return i;
    }
  }
  return -1;
}



String receiveMsg() {
  String msg = "";
  if (Serial.available() > 0) {
    msg = Serial.readStringUntil('\n');
    msg.trim();
  }
  return msg;
}



void displayAccount(account accountToDisplay) {
  lcd.clear();

  if (accountIndex > 0) {
    lcd.setCursor(0, 0);
    lcd.write(0);
  }

  if (accountIndex < accountsSize - 1) {
    lcd.setCursor(0, 1);
    lcd.write(1);
  }

  lcd.setCursor(1, 0);

  lcd.print(accountToDisplay.grade);
  lcd.print(" ");

  if (accountToDisplay.pension) {
    lcd.print(" PEN");
    lcd.setBacklight(green);
  }

  else {
    lcd.print("NPEN");
    lcd.setBacklight(red);
  }

  lcd.print(" ");

  long intPart = accountToDisplay.salary / 100;
  int fracPart = accountToDisplay.salary % 100;

  int digits = 0;
  int temp = intPart;

  do {
    digits++;  //000
    temp /= 10;
  } while (temp != 0);

  int padding = 5 - digits;
  for (int i = 0; i < padding; i++) {
    lcd.print("0");
  }

  lcd.print(intPart);

  lcd.print(".");
  if (fracPart < 10) {
    lcd.print("0");
  }

  lcd.print(fracPart);

  lcd.setCursor(1, 1);

  lcd.print(accountToDisplay.id);

  lcd.print(" ");

  lcd.print(accountToDisplay.title);
}



void updateScroll(account accountToDisplay) {
  static int titleIndex = 0;
  int titleLength;

  lcd.setCursor(9, 1);

  titleLength = strlen(accountToDisplay.title);

  for (int i = titleIndex; i < titleIndex + 7; i++) {
    lcd.print(accountToDisplay.title[i]);
  }

  titleIndex = (titleIndex + 1) % (titleLength - 7 + 1);
}



void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);

  lcd.setBacklight(defaultBacklight);

  lcd.createChar(0, upArrow);
  lcd.createChar(1, downArrow);

}



void loop() {
  uint8_t buttons = lcd.readButtons();

  switch (currentDisplay) {

    case displayDefault:

      if (updateDisplay) {
        updateDisplay = false;
        if (accountsSize > 0) {
          displayAccount(accounts[accountIndex]);
        }
      }


      if (strlen((accounts[accountIndex].title)) > 7) {
        if (scrollTime == 0) {
          scrollTime = millis();
        }

        if (millis() - scrollTime >= 500) {
          updateScroll(accounts[accountIndex]);
          scrollTime = millis();
        }

      } else {
        scrollTime = 0;
      }


      if (buttons & BUTTON_SELECT) {
        if (buttonHoldTime == 0) {
          buttonHoldTime = millis();
        }

        if ((millis() - buttonHoldTime >= 1000)) {
          currentDisplay = displayStudentID;
        }
      }

      else {
        //ACCOUNT SCROLL LOGIC HERE
        if (buttons & BUTTON_UP) {
          if (millis() - lastButtonPressTime > debounceDelay) {
            if (accountIndex > 0) {
              updateDisplay = true;
              accountIndex--;
              lastButtonPressTime = millis();
            }
          }
        }

        else if (buttons & BUTTON_DOWN) {
          if (millis() - lastButtonPressTime > debounceDelay) {
            if (accountIndex < accountsSize - 1) {
              updateDisplay = true;
              accountIndex++;
              lastButtonPressTime = millis();
            }
          }
        }
      }
      break;


    case displayStudentID:
      if (!(displayingStudentID)) {
        displayingStudentID = true;
        lcd.clear();
        lcd.setBacklight(violet);
        lcd.setCursor(0, 0);
        lcd.print(F("F419073"));
        lcd.setCursor(0, 1);
        lcd.print(freeMemory());
        lcd.print(F(" Bytes"));
      }

      if (!(buttons & BUTTON_SELECT)) {
        displayingStudentID = false;
        buttonHoldTime = 0;
        lcd.clear();
        lcd.setBacklight(defaultBacklight);
        updateDisplay = true;
        currentDisplay = displayDefault;
      }

      break;
  }

  switch (currentState) {

    case SyncPhase:
      if (millis() - lastTime >= 2000) {
        Serial.print(F("R"));
        lastTime = millis();
      }

      msg = receiveMsg();

      if (msg.endsWith("BEGIN")) {
        Serial.print(F("BASIC\n"));
        defaultBacklight = white;
        lcd.setBacklight(defaultBacklight);
        currentState = MainPhase;
        msg = "";
      }

      break;


    case MainPhase:

      msg = receiveMsg();

      if (msg.startsWith("ADD")) {  //25 - 39 FOR LENGTH    ///verify size,then dashes, then individual checks
        if (msg.length() >= 17 and msg.length() <= 31 and checkDashes(msg, addMask)) {
          unsigned long id = msg.substring(4, 11).toInt();
          unsigned int grade;
          char c;

          c = msg.charAt(12);

          if (c >= '1' and c <= '9') {
            grade = c - '0';  // SUBTRACTS ASCII VALUES RESULT IS AN INTEGER
          } else {
            Serial.print(F("ERROR: NOT NON ZERO DIGIT"));
            break;
          }

          if (findAccount(id) == -1) {


            char title[18];

            msg.substring(14, msg.length()).toCharArray(title, sizeof(title));
            title[msg.length() - 14] = '\0';

            if (verifyTitle(title)) {
              account a;

              a.id = id;
              a.grade = grade;

              strncpy(a.title, title, sizeof(a.title) - 1);
              a.title[sizeof(a.title) - 1] = '\0';

              a.salary = 0;
              a.pension = true;

              if (insertAccount(a)) {
                Serial.print(F("DONE!"));
                updateDisplay = true;
              }

              else {
                Serial.print(F("ERROR: ARRAY FULL"));
              }
            }

            else {
              Serial.print(F("ERROR: TITLE WRONG"));
            }
          } else {
            Serial.print(F("ERROR: ACCOUNT ALREADY EXISTS"));
          }
        } else {
          Serial.print(F("ERROR: IN COMMAND"));
        }
      }


      else if (msg.startsWith("PST")) {
        if ((msg.length() == 15 or msg.length() == 16) and checkDashes(msg, pstMask)) {
          unsigned long id = msg.substring(4, 11).toInt();

          int index = findAccount(id);

          if (index != -1) {
            char pen[5];
            msg.substring(12, msg.length()).toCharArray(pen, 5);
            pen[4] = '\0';

            bool currentPenStatus = accounts[index].pension;

            if (accounts[index].salary > 0) {
              bool changedPension = false;
              if (strcmp(pen, "PEN") == 0 and !currentPenStatus) {
                accounts[index].pension = true;
                changedPension = true;
              }

              else if (strcmp(pen, "NPEN") == 0 and currentPenStatus) {
                accounts[index].pension = false;
                changedPension = true;
              }

              if (changedPension) {
                Serial.print(F("DONE!"));
                updateDisplay = true;
              }

              else {
                Serial.print(F("ERROR: PENSION STATUS NOT CHANGED"));
              }
            }

            else {
              Serial.print(F("ERROR: SALARY NOT SET"));
            }
          }

          else {
            Serial.print(F("ERROR: ACCOUNT NOT FOUND"));
          }
        } else {
          Serial.print(F("ERROR: COMMAND WRONG LENGTH"));
        }
      }


      else if (msg.startsWith("GRD")) {
        if (msg.length() == 13 and checkDashes(msg, grdMask)) {
          unsigned long id = msg.substring(4, 11).toInt();

          int index = findAccount(id);
          if (index != -1) {
            unsigned int grade;
            char c;
            c = msg.charAt(12);

            if (c >= '1' and c <= '9') {
              grade = c - '0';  // SUBTRACTS ASCII VALUES RESULT IS AN INTEGER

              if (grade <= accounts[index].grade) {
                accounts[index].grade = grade;
                Serial.print(F("DONE!"));
                updateDisplay = true;
              }

              else {
                Serial.print(F("ERROR: NEW GRADE MUST BE HIGHER"));
              }

            }

            else {
              Serial.print(F("ERROR: NOT NON ZERO DIGIT"));
            }
          }

          else {
            Serial.print(F("ERROR: ACCOUNT NOT FOUND"));
          }
        }

        else {
          Serial.print(F("ERROR: COMMAND WRONG LENGTH"));
        }
      }


      else if (msg.startsWith("SAL")) {
        if (msg.length() >= 12 and msg.length() <= 20 and checkDashes(msg, salMask)) {  //msg limit of 20 as it can still read a few numebrs afte rthe decimal place but any more woudl be too much, ithink limitng it to 20 is pretty snesible
          unsigned long id = msg.substring(4, 11).toInt();
          int index = findAccount(id);

          if (index != -1) {
            float salaryFloat;
            salaryFloat = msg.substring(12, msg.length()).toFloat();

            if (salaryFloat >= 0 and salaryFloat < 100000) {
              //Serial.println(F("DEBUG:") + String(salaryFloat));
              unsigned long salaryInPence = (unsigned long)round(salaryFloat * 100.0);

              //Serial.println(F("DEBUG:") + String(salaryInPence));
              accounts[index].salary = salaryInPence;
              Serial.print(F("DONE!"));
              updateDisplay = true;
            }

            else {
              Serial.print(F("ERROR: SALARY OUT OF RANGE"));
            }

          }

          else {
            Serial.print(F("ERROR: ACCOUNT NOT FOUND"));
          }
        }

        else {
          Serial.print(F("ERROR: INCORRECT COMMAND SYNTAXX"));
        }
      }


      else if (msg.startsWith("CJT")) {
        if (msg.length() >= 17 and msg.length() <= 31 and checkDashes(msg, cjtMask)) {
          unsigned long id = msg.substring(4, 11).toInt();
          int index = findAccount(id);

          if (index != -1) {

            char title[18];

            msg.substring(14, msg.length()).toCharArray(title, sizeof(title));
            title[msg.length() - 14] = '\0';


            if (verifyTitle(title)) {

              strncpy(accounts[index].title, title, sizeof(accounts[index].title) - 1);
              accounts[index].title[sizeof(accounts[index].title) - 1] = '\0';

              Serial.print(F("DONE!"));
              updateDisplay = true;
            }

            else {
              Serial.print(F("ERROR: INCORRECT TITLE FORMAT"));
            }
          }

          else {
            Serial.print(F("ERROR: ACCOUNT NOT FOUND"));
          }
        }
      }


      else if (msg.startsWith("DEL")) {
        if (accountsSize == 0) {
          Serial.print(F("ERROR: NOHTING TO DELETE"));
        }

        else if (msg.length() == 11 and checkDashes(msg, delMask)) {
          unsigned long id = msg.substring(4, 11).toInt();
          int index = findAccount(id);
          if (index != -1) {
            for (byte i = index; i < accountsSize - 1; i++) {
              accounts[i] = accounts[i + 1];
            }

            accountsSize--;

            if (accountIndex >= accountsSize) {
              accountIndex = (accountsSize > 0) ? accountsSize - 1 : 0;  //ternary operator, value = (condition ? if_true : if_false)
            }

            Serial.print(F("DONE!"));
            updateDisplay = true;
          }

          else {
            Serial.print(F("ERROR: ACCOUNT NOT FOUND"));
          }
        }
      }

      else {
        Serial.print(F("ERROR: COMMAND WRONG LENGTH"));
      }
        
      

      break;
  }
}
