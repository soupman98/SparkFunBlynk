/******************************************************************************
BlynkBoard_Setup.h
BlynkBoard Firmware: Board initialization functions
Jim Lindblom @ SparkFun Electronics
February 24, 2016
https://github.com/sparkfun/Blynk_Board_ESP8266/Firmware

This file, part of the BlynkBoard Firmware, defines the hardware setup and
initialization functions. That includes EEPROM and flash (SPIFFS) reading
and writing.

Resources:
ESP8266 File System (FS.h) - (included with ESP8266 Arduino board definitions)

License:
This is released under the MIT license (http://opensource.org/licenses/MIT).
Please see the included LICENSE.md for more information.

Development environment specifics:
Arduino IDE 1.6.7
SparkFun BlynkBoard - ESP8266
******************************************************************************/

void initHardware(void)
{
  delay(1000);
  Serial.begin(SERIAL_TERMINAL_BAUD);
  BB_DEBUG("");
  BB_DEBUG("SparkFun Blynk Board Hardware v" + String(BLYNKBOARD_HARDWARE_VERSION));
  BB_DEBUG("SparkFun Blynk Board Firmware v" + String(BLYNKBOARD_FIRMWARE_VERSION));

  //! TODO: find something more unique to randomSeed
  //! Maybe the Si7021's guaranteed unique serial #
  randomSeed(millis() - analogRead(A0));

  // Initialize RGB LED and turn it off:
  rgb.begin();
  rgb.setPixelColor(0, rgb.Color(0, 0, 0));
  rgb.show();
  // Attach a timer to the RGB LED, begin by calling it
  blinker.attach_ms(RGB_PERIOD_AP, blinkRGBTimer);

  // Initialize the button, and setup a hardware interrupt
  // Look for RISING - when the button is released.
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, buttonPress, RISING);

  SPIFFS.begin(); //! TODO: consider returning error if this fails.
  EEPROM.begin(EEPROM_SIZE);
}

bool checkConfigFlag(void)
{  
  byte configFlag = EEPROM.read(EEPROM_CONFIG_FLAG_ADDRESS);

  return configFlag;
}

bool writeBlynkAuth(String authToken)
{
  File authFile = SPIFFS.open(BLYNK_AUTH_SPIFF_FILE, "w");
  if (authFile)
  {
    BB_DEBUG("Opened " + String(BLYNK_AUTH_SPIFF_FILE));
    authFile.print(authToken);
    authFile.close();
    BB_DEBUG("Wrote file.");
    return true;
  }
  else
  {
    BB_DEBUG("Failed to open file to write.");
    return false;
  }
}

String getBlynkAuth(void)
{
  String retAuth;
  
  if (SPIFFS.exists(BLYNK_AUTH_SPIFF_FILE))
  {
    BB_DEBUG("Opening auth file.");
    File authFile = SPIFFS.open(BLYNK_AUTH_SPIFF_FILE, "r");
    if(authFile)
    {
      BB_DEBUG("File opened.");
      size_t authFileSize = authFile.size();
      // Only return auth token if it's the right size (32 bytes)
      if (authFileSize == BLYNK_AUTH_TOKEN_SIZE)
      {
        while (authFile.available())
        {
          retAuth += (char)authFile.read();
        }
      }
      authFile.close();
    }
    else
    {
      BB_DEBUG("Failed to open auth file.");
    }
  }
  else
  {
    BB_DEBUG("File does not exist.");
  }

  return retAuth;
}

int8_t setupBlynkStation(String network, String psk, String blynk)
{
  long timeIn = WIFI_STA_CONNECT_TIMEOUT;
  
  runMode = MODE_CONNECTING_WIFI;
  blinker.attach_ms(0.1, blinkRGBTimer);

  WiFi.enableSTA(true);
  WiFi.disconnect();
  WiFi.begin(network.c_str(), psk.c_str());
  
  while ((WiFi.status() != WL_CONNECTED) && (--timeIn > 0))
    delay(1);

  if (timeIn <= 0)
  {
    BB_DEBUG("Timed out connecting to WiFi.");
    WiFi.enableSTA(false); // Disable station mode
    runMode = MODE_CONFIG; // Back to config LED blink
    return ERROR_CONNECT_WIFI;
  }

  runMode = MODE_CONNECTING_BLYNK;

  Blynk.config(blynk.c_str());
  timeIn = BLYNK_CONNECT_TIMEOUT;
  while ((!Blynk.connected()) && (--timeIn > 0))
  {
    Blynk.run();
    delay(1);
  }

  if (timeIn <= 0)
  {
    BB_DEBUG("Timed out connecting to Blynk.");
    runMode = MODE_CONFIG; // Back to config LED blink
    return ERROR_CONNECT_BLYNK;  
  }

  //! TODO: Put these three lines into a separate function:
  writeBlynkAuth(blynk); //! TODO: check return value of this
  EEPROM.write(EEPROM_CONFIG_FLAG_ADDRESS, 1);
  EEPROM.commit();

  //! TODO: Consider changing mode to an intermediary
  //  MODE_BLYNK_SETUP, where blynkSetup is called.
  blynkSetup();
  
  return WIFI_BLYNK_SUCCESS;
}

void resetEEPROM(void)
{
  EEPROM.write(EEPROM_CONFIG_FLAG_ADDRESS, 0);
  EEPROM.commit();
  writeBlynkAuth("");
}