//EEPROM
void saveSettingsToEEPROM() {
  PRINTLN("Saving Settings to eeprom");
  EEPROM.write(1, runningClock);
  EEPROM.write(2, colorLight);
  EEPROM.write(11, runningTimer);
  EEPROM.commit();
}


void loadSettingsFromEEPROM() {
  PRINTLN("Reading Settings from eeprom");
  runningClock = EEPROM.read(1);
  colorLight = EEPROM.read(2);
  runningTimer = EEPROM.read(11);
  //Get the float data from the EEPROM at position 'eeAddress'
  endTimer=EEPROMReadTime(12);
  PRINT("endTimer:",endTimer);
}

long EEPROMReadTime(int address) {
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);
 
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

void EEPROMWriteTime(int address, long value) {
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
  EEPROM.commit();
  Serial.println("EEPROM write");
}


void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
byte len = strToWrite.length();
EEPROM.write(addrOffset, len);
for (int i = 0; i < len; i++)
{
EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
}
EEPROM.commit();
}


String readStringFromEEPROM(int addrOffset)
{
int newStrLen = EEPROM.read(addrOffset);
char data[newStrLen + 1];
for (int i = 0; i < newStrLen; i++)
{
data[i] = EEPROM.read(addrOffset + 1 + i);
}
data[newStrLen] = '\0';
return String(data);
}
//////////////////////////////////

String time2string(time_t &thisTime) {  //convert time to time and date string for display in webpage.
  // Populate myDate String
  String time2str;
  time2str = hour(thisTime);

  if (minute(thisTime) < 10) {
    time2str = time2str + ":0" + minute(thisTime);
  }
  else {
    time2str = time2str + ":" + minute(thisTime);
  }


  if (second(thisTime) < 10) {
    time2str = time2str + ":0" + second(thisTime);
  }
  else {
    time2str = time2str + ":" + second(thisTime);
  }

  time2str = time2str + "  " + dayShortStr(weekday(thisTime)) + "  " + day(thisTime) + " " + monthShortStr(month(thisTime)) + " " + year(thisTime) ;
  //time2str= (" " + myTime +" ");
  //Serial.println("time2str: " + time2str);

  return time2str;
}

/////////////////////
//
time_t getEpochFromString(String nextTimer) {
  //Make time from event
  time_t startEvt;// Declare a time_t object for the start time (time_t)
  tmElements_t tmStartEvt; // Declare a tmElement_t object to manually input the start Time


  // Creates a tmElement by putting together all of Maggies Start Time and Dates
  tmStartEvt.Year = nextTimer.substring(2, 4).toInt() + 30; // years since 1970 in two digits
  tmStartEvt.Month = nextTimer.substring(5, 7).toInt();
  tmStartEvt.Day = nextTimer.substring(8, 10).toInt();
  tmStartEvt.Hour = nextTimer.substring(11, 13).toInt();
  tmStartEvt.Minute = nextTimer.substring(14, 16).toInt();
  tmStartEvt.Second = nextTimer.substring(17, 19).toInt();

  // Takes the tmElement and converts it to a time_t variable. Which can now be compared to the current (now) unix time
  startEvt = makeTime(tmStartEvt);

  PRINT("\nTimerEnd received: ", startEvt);

  PRINT("\n Timer Start at : ", timeClient.getEpochTime());

  return (startEvt);
}
