// Feather9x_RX
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messaging client (receiver)
// with the RH_RF95 class. RH_RF95 class does not provide for addressing or
// reliability, so you should only use RH_RF95 if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example Feather9x_TX

#include <SPI.h>
#include <RH_RF95.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DS3231.h>

#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7
#define RF95_FREQ 915.0

#define WIRE Wire

#define BUTTON_UP  9
#define BUTTON_SL  6
#define BUTTON_DN  5
#define SCREEN_TIMEOUT 60000

#define ENTER_EDIT_TIMEOUT 2000

#define HOLD_COOLDOWN 150

#define YEAR_UPPER_LIMIT 159
#define YEAR_LOWER_LIMIT 0
#define YEAR_UNDERLINE "\n\n____"

#define MONTH_UPPER_LIMIT 12
#define MONTH_LOWER_LIMIT 1
#define MONTH_UNDERLINE "\n\n     __"

#define DAY_LOWER_LIMIT 1
#define DAY_UNDERLINE "\n\n        __"

#define HOUR_UPPER_LIMIT 23
#define HOUR_LOWER_LIMIT 0
#define HOUR_UNDERLINE "\n\n           __"

#define MINUTE_UPPER_LIMIT 59
#define MINUTE_LOWER_LIMIT 0
#define MINUTE_UNDERLINE "\n\n              __"

#define SECOND_UPPER_LIMIT 59
#define SECOND_LOWER_LIMIT 0
#define SECOND_UNDERLINE "\n\n                 __"

const unsigned long int DAY_UPPER_LIMIT[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &WIRE);

DS3231 rtc;

char msg[85];

void setup()
{
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_SL, INPUT_PULLUP);
  pinMode(BUTTON_DN, INPUT_PULLUP);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  delay(100);

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  rtc.setClockMode(false);

  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (!rf95.init())
  {
    while (1)
    {
      display.clearDisplay();
      display.print("Failed to init radio");
      display.setCursor(0, 0);
      display.display();
    }
  }

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ))
  {
    while (1)
    {
      display.clearDisplay();
      display.print("Failed to set freq");
      display.setCursor(0, 0);
      display.display();
    }
  }

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);

  strcpy(msg, "Waiting for message");
  refreshDisplay();
}

unsigned long screenTime = SCREEN_TIMEOUT;
bool screenState = true;

bool isEditing = false;
int menuMode = 0;
void (*menuFunction[])(unsigned long) = {enterEditMode, editYear, editMonth, editDay, editHour, editMinute, editSecond};

bool century = false;
bool h12Flag;
bool pmFlag;

void loop()
{
  static unsigned long prevtime = 0;
  unsigned long currenttime = millis();
  unsigned long deltatime = currenttime - prevtime;

  if (screenTime > SCREEN_TIMEOUT)
  {
    if (screenState)
    {
      display.clearDisplay();
      display.display();
      screenState = false;
      menuMode = 0;
      isEditing = false;
    }
    if (!(digitalRead(BUTTON_SL) && digitalRead(BUTTON_UP) && digitalRead(BUTTON_DN)))
      wake();
  }
  else
    menuFunction[menuMode](deltatime);

  screenTime -= deltatime;
  prevtime = currenttime;


  if (rf95.available())
  {
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len))
    {
      snprintf(msg, sizeof msg, "Last message at:\n2%03i-%02i-%02i %2i:%02i:%02i\n\nLast VBat: %sV", rtc.getYear(), rtc.getMonth(century), rtc.getDate(), rtc.getHour(h12Flag, pmFlag), rtc.getMinute(), rtc.getSecond(), buf);
      if (!isEditing)
        wake();

      // Send a reply
      uint8_t ack[] = "ACK";
      rf95.send(ack, sizeof ack);
      rf95.waitPacketSent();
    }
  }
}

void refreshDisplay(void)
{
  display.clearDisplay();
  display.print(msg);
  display.setCursor(0, 0);
  display.display();
}

void wake(void)
{
  refreshDisplay();
  screenTime = SCREEN_TIMEOUT;
  screenState = true;
}

unsigned long int yearBuffer, monthBuffer, dayBuffer, hourBuffer, minuteBuffer, secondBuffer;

unsigned long int dayUpperLimit;

char editmsg[85];

void enterEditMode(unsigned long deltatime)
{
  static unsigned long timeout = ENTER_EDIT_TIMEOUT;
  if (digitalRead(BUTTON_SL))
  {
    if (timeout != ENTER_EDIT_TIMEOUT)
      timeout = ENTER_EDIT_TIMEOUT;
  }
  else
  {
    timeout -= deltatime;
    if (timeout > ENTER_EDIT_TIMEOUT)
    {
      isEditing = true;
      menuMode = 1;
      yearBuffer = rtc.getYear();
      monthBuffer = rtc.getMonth(century);
      dayBuffer = rtc.getDate();
      hourBuffer = rtc.getHour(h12Flag, pmFlag);
      minuteBuffer = rtc.getMinute();
      secondBuffer = rtc.getSecond();
      timeout = ENTER_EDIT_TIMEOUT;
      refreshEditMenu(YEAR_UNDERLINE);
    }
  }
}

unsigned long cooldown = 0;
bool entry = false;

void editYear(unsigned long deltatime)
{
  if (entry)
  {
    bool up = !digitalRead(BUTTON_UP), down = !digitalRead(BUTTON_DN), select = !digitalRead(BUTTON_SL);
    if (up || down || select)
    {
      screenTime = SCREEN_TIMEOUT;
      if (cooldown > HOLD_COOLDOWN)
      {
        if (up)
        {
          if (yearBuffer < YEAR_UPPER_LIMIT)
            yearBuffer++;
          refreshEditMenu(YEAR_UNDERLINE);
        }
        else if (down)
        {
          if (yearBuffer > YEAR_LOWER_LIMIT)
            yearBuffer--;
          refreshEditMenu(YEAR_UNDERLINE);
        }
        else if (select)
        {
          entry = false;
          cooldown = 0;
          refreshEditMenu(MONTH_UNDERLINE);
          menuMode = 2;
          return;
        }
        cooldown = HOLD_COOLDOWN;
      }
      cooldown -= deltatime;
    }
    else if (cooldown != 0)
      cooldown = 0;
  }
  else if (digitalRead(BUTTON_SL))
    entry = true;
}

void editMonth(unsigned long deltatime)
{
  if (entry)
  {
    bool up = !digitalRead(BUTTON_UP), down = !digitalRead(BUTTON_DN), select = !digitalRead(BUTTON_SL);
    if (up || down || select)
    {
      screenTime = SCREEN_TIMEOUT;
      if (cooldown > HOLD_COOLDOWN)
      {
        if (up)
        {
          if (monthBuffer < MONTH_UPPER_LIMIT)
            monthBuffer++;
          else
            monthBuffer = MONTH_LOWER_LIMIT;
          refreshEditMenu(MONTH_UNDERLINE);
        }
        else if (down)
        {
          if (monthBuffer > MONTH_LOWER_LIMIT)
            monthBuffer--;
          else
            monthBuffer = MONTH_UPPER_LIMIT;
          refreshEditMenu(MONTH_UNDERLINE);
        }
        else if (select)
        {
          entry = false;
          cooldown = 0;
          dayUpperLimit = monthBuffer == 2 && isLeap(yearBuffer) ? 29 : DAY_UPPER_LIMIT[monthBuffer - 1];
          if (dayBuffer > dayUpperLimit)
            dayBuffer = dayUpperLimit;
          refreshEditMenu(DAY_UNDERLINE);
          menuMode = 3;
          return;
        }
        cooldown = HOLD_COOLDOWN;
      }
      cooldown -= deltatime;
    }
    else if (cooldown != 0)
      cooldown = 0;
  }
  else if (digitalRead(BUTTON_SL))
    entry = true;
}

void editDay(unsigned long deltatime)
{
  if (entry)
  {
    bool up = !digitalRead(BUTTON_UP), down = !digitalRead(BUTTON_DN), select = !digitalRead(BUTTON_SL);
    if (up || down || select)
    {
      screenTime = SCREEN_TIMEOUT;
      if (cooldown > HOLD_COOLDOWN)
      {
        if (up)
        {
          if (dayBuffer < dayUpperLimit)
            dayBuffer++;
          else
            dayBuffer = DAY_LOWER_LIMIT;
          refreshEditMenu(DAY_UNDERLINE);
        }
        else if (down)
        {
          if (dayBuffer > DAY_LOWER_LIMIT)
            dayBuffer--;
          else
            dayBuffer = dayUpperLimit;
          refreshEditMenu(DAY_UNDERLINE);
        }
        else if (select)
        {
          entry = false;
          cooldown = 0;
          refreshEditMenu(HOUR_UNDERLINE);
          menuMode = 4;
          return;
        }
        cooldown = HOLD_COOLDOWN;
      }
      cooldown -= deltatime;
    }
    else if (cooldown != 0)
      cooldown = 0;
  }
  else if (digitalRead(BUTTON_SL))
    entry = true;
}

void editHour(unsigned long deltatime)
{
  if (entry)
  {
    bool up = !digitalRead(BUTTON_UP), down = !digitalRead(BUTTON_DN), select = !digitalRead(BUTTON_SL);
    if (up || down || select)
    {
      screenTime = SCREEN_TIMEOUT;
      if (cooldown > HOLD_COOLDOWN)
      {
        if (up)
        {
          if (hourBuffer < HOUR_UPPER_LIMIT)
            hourBuffer++;
          else
            hourBuffer = HOUR_LOWER_LIMIT;
          refreshEditMenu(HOUR_UNDERLINE);
        }
        else if (down)
        {
          if (hourBuffer > HOUR_LOWER_LIMIT)
            hourBuffer--;
          else
            hourBuffer = HOUR_UPPER_LIMIT;
          refreshEditMenu(HOUR_UNDERLINE);
        }
        else if (select)
        {
          entry = false;
          cooldown = 0;
          refreshEditMenu(MINUTE_UNDERLINE);
          menuMode = 5;
          return;
        }
        cooldown = HOLD_COOLDOWN;
      }
      cooldown -= deltatime;
    }
    else if (cooldown != 0)
      cooldown = 0;
  }
  else if (digitalRead(BUTTON_SL))
    entry = true;
}

void editMinute(unsigned long deltatime)
{
  if (entry)
  {
    bool up = !digitalRead(BUTTON_UP), down = !digitalRead(BUTTON_DN), select = !digitalRead(BUTTON_SL);
    if (up || down || select)
    {
      screenTime = SCREEN_TIMEOUT;
      if (cooldown > HOLD_COOLDOWN)
      {
        if (up)
        {
          if (minuteBuffer < MINUTE_UPPER_LIMIT)
            minuteBuffer++;
          else
            minuteBuffer = MINUTE_LOWER_LIMIT;
          refreshEditMenu(MINUTE_UNDERLINE);
        }
        else if (down)
        {
          if (minuteBuffer > MINUTE_LOWER_LIMIT)
            minuteBuffer--;
          else
            minuteBuffer = MINUTE_UPPER_LIMIT;
          refreshEditMenu(MINUTE_UNDERLINE);
        }
        else if (select)
        {
          entry = false;
          cooldown = 0;
          refreshEditMenu(SECOND_UNDERLINE);
          menuMode = 6;
          return;
        }
        cooldown = HOLD_COOLDOWN;
      }
      cooldown -= deltatime;
    }
    else if (cooldown != 0)
      cooldown = 0;
  }
  else if (digitalRead(BUTTON_SL))
    entry = true;
}

void editSecond(unsigned long deltatime)
{
  if (entry)
  {
    bool up = !digitalRead(BUTTON_UP), down = !digitalRead(BUTTON_DN), select = !digitalRead(BUTTON_SL);
    if (up || down || select)
    {
      screenTime = SCREEN_TIMEOUT;
      if (cooldown > HOLD_COOLDOWN)
      {
        if (up)
        {
          if (secondBuffer < SECOND_UPPER_LIMIT)
            secondBuffer++;
          else
            secondBuffer = SECOND_LOWER_LIMIT;
          refreshEditMenu(SECOND_UNDERLINE);
        }
        else if (down)
        {
          if (secondBuffer > SECOND_LOWER_LIMIT)
            secondBuffer--;
          else
            secondBuffer = SECOND_UPPER_LIMIT;
          refreshEditMenu(SECOND_UNDERLINE);
        }
        else if (select)
        {
          entry = false;
          cooldown = 0;
          rtc.setYear(yearBuffer);
          rtc.setMonth(monthBuffer);
          rtc.setDate(dayBuffer);
          rtc.setHour(hourBuffer);
          rtc.setMinute(minuteBuffer);
          rtc.setSecond(secondBuffer);
          refreshDisplay();
          menuMode = 0;
          isEditing = false;
          return;
        }
        cooldown = HOLD_COOLDOWN;
      }
      cooldown -= deltatime;
    }
    else if (cooldown != 0)
      cooldown = 0;
  }
  else if (digitalRead(BUTTON_SL))
    entry = true;
}

bool isLeap(int yr)
{
    if (yr % 400 == 0)
        return true;
    if (yr % 100 == 0)
        return false;
    if (yr % 4 == 0)
        return true;
    return false;
}

void refreshEditMenu(const char * underline)
{
  display.clearDisplay();
  snprintf(editmsg, sizeof editmsg, "Edit time:\n\n2%03lu-%02lu-%02lu %2lu:%02lu:%02lu", yearBuffer, monthBuffer, dayBuffer, hourBuffer, minuteBuffer, secondBuffer);
  display.print(editmsg);
  display.setCursor(0, 2);
  display.print(underline);
  display.setCursor(0, 0);
  display.display();
}
