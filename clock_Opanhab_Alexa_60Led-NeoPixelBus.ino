  /* NeoPixelAnimation
  * Save Following in EEPROM
  * light1,runningClock,runningTimer and endTimer, brightness,alarmHour, alarmMinute 
  */
  #include <FS.h>
  
  //#include <NeoPixelBus.h>
  #include <NeoPixelBrightnessBus.h> // instead of NeoPixelBus.h
  #include <NeoPixelAnimator.h>
  
  
  
  #ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #else
  #include <ESP8266WiFi.h>
  #endif
  #include <ESP8266WebServer.h>
  
  ///#define ESPALEXA_DEBUG    //this enables espalexa debug
  #include <ESP8266mDNS.h>
  ///#include <Espalexa.h>
  #include <ESP8266Ping.h>
  #include <NTPClient.h>
  #include <TimeLib.h> //used in function displaying power on time in debug page
  #include <EEPROM.h>
  #include <PubSubClient.h>
  #include "index.h"
  
  
  const uint16_t PixelCount = 60; // make sure to set this to the number of pixels in your strip
  const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266
  
  #define colorSaturation 255 // saturation of color constants
  RgbColor red(colorSaturation, 0, 0);
  RgbColor green(0, colorSaturation, 0);
  RgbColor blue(0, 0, colorSaturation);
  RgbColor cyan(0, colorSaturation, colorSaturation);
  RgbColor white(colorSaturation);
  RgbColor black(0);
  
  ////////////////////////////////
  const int buzzer = D2; //buzzer to wemosd1 pin D2
  
  const char* ssid = "Suresh";
  const char* password = "mvls$1488";
  
  IPAddress staticIP(192, 168, 1, 167);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dnsIP1(192, 168, 1, 1);
  
  
  
  // Turn on debug statements to the serial output
  #define  DEBUG  1
  
  #if  DEBUG
  #define  PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
  #define PRINTS(x) Serial.print(F(x))
  #define PRINTLN(x) Serial.println(F(x))
  #define PRINTD(x) Serial.println(x, DEC)
  //include ESP8266 SDK C functions to get heap
  extern "C" {                                          // ESP8266 SDK C functions
  #include "user_interface.h"
  }
  
  #else
  #define PRINT(s, x)
  #define PRINTS(x)
  #define PRINTLN(x)
  #define PRINTD(x)
  
  #endif
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //OTA Section
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  #include <ESP8266HTTPUpdateServer.h>
  const char* otahost = "clock167-webupdate";
  const char* update_path = "/firmware";
  const char* update_username = "admin";
  const char* update_password = "admin";
  
  ESP8266WebServer httpServer(90);
  ESP8266HTTPUpdateServer httpUpdater;
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  
  ////////////////////////////////////////////////////////////////////////////////
  //Web Interface
  ///////////////////////////////////////////////////////////////////////////////
  //extern const char index_html[];
  //extern const char main_js[];
  String setcolor="#ff00ff"; //Set color for HTML
  
  // QUICKFIX...See https://github.com/esp8266/Arduino/issues/263
  #define min(a,b) ((a)<(b)?(a):(b))
  #define max(a,b) ((a)>(b)?(a):(b))
  
  #define HTTP_PORT 80
  #define DEFAULT_COLOR 0xFF5900
  #define DEFAULT_BRIGHTNESS 100  //for clock range is 0-100 and for colorLight 0-255. this difference is due to Alexa tags Lighting and colorLighting respectively.
  #define DEFAULT_SPEED 1000
  #define DEFAULT_MODE FX_MODE_STATIC
  
  unsigned long auto_last_change = 0;
  unsigned long last_wifi_check_time = 0;
  boolean auto_cycle = false;
  uint8_t next_mode=0;
  int cycle_effect_sec=10000;
  ESP8266WebServer server(HTTP_PORT);
  
  
  
  
  //////////////////////////////////////////////
  //NTP
  /////////////////////////////////////////////
  WiFiUDP ntpUDP;
  
  // You can specify the time server pool and the offset (in seconds, can be
  // changed later with setTimeOffset() ). Additionaly you can specify the
  // update interval (in milliseconds, can be changed using setUpdateInterval() ).
  NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 19800, 300000);
  
  /////////////////////////////////////////////
  //Clock and Timer
  /////////////////////////////////////////////
  long lastUpdate = millis();
  long lastSecond = millis();
  long lastSubSecond = millis();
  bool runningClock = false;

  bool colorLight=false;
  
  time_t endTimer;
  long startTimer;
  int16_t timerSeconds;
  uint32_t timerRGB = 0xFFFFFF; //timer LED color, 0xFFFFFF is white
  bool runningTimer = false;
  bool firstCycle=true;
  
  long animStart;
  long animTime = 30000;
  bool animRunning = false;
  
  //bool showAlarm=false;
  
  int subSecPixel = 0;
  int currentHourLed;
  int currentMinLed;
  int currentSecLed;
  bool toggleMinHour = true; //used to toggle LED color if Hour and Min on same Led
  
  String hours, minutes, seconds;
  int currentSecond, currentMinute, currentHour;
  
  int alarmHour, alarmMinute;
  double alarmStartTime = 0;
  
  time_t myPowerOnTime = 0; // Variable to store ESP Power on time
  time_t prevEvtTime = 0; // Variable to store ping time
  
  ////////////////////////////////////////////////////////
  
  //MQTT
  const char* mqtt_server = "192.168.1.29";
  WiFiClient espClient;
  PubSubClient mqttClient(espClient);
  unsigned long lastMsg = 0;
  #define MSG_BUFFER_SIZE  (50)
  char msg[MSG_BUFFER_SIZE];
  int value = 0;
  
  
  //const int mqtt_port = 1883;
  //const char* mqtt_user = "";
  //const char* mqtt_password = "";
  const char* mqtt_colorLight_state_topic = "colorLight/color/state";
  const char* mqtt_colorLight_command_topic = "colorLight/color/command";
  
  //const char* mqtt_colorLight_clock_state = "colorLight/clock/state";
  //const char* mqtt_colorLight_clock_command = "colorLight/clock/command";
  const char* mqtt_colorLight_clock_dimmer_state = "colorLight/clock/dimmer/state";
  const char* mqtt_colorLight_clock_dimmer_command = "colorLight/clock/dimmer/command";
  
  ///////////////////////////////
  
  NeoPixelBrightnessBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
  // For Esp8266, the Pin is omitted and it uses GPIO3 due to DMA hardware use.  
  // There are other Esp8266 alternative methods that provide more pin options, but also have
  // other side effects.
  // for details see wiki linked here https://github.com/Makuna/NeoPixelBus/wiki/ESP8266-NeoMethods 
  
  
  // NeoPixel animation time management object
  NeoPixelAnimator animations(PixelCount, NEO_CENTISECONDS);
  
  // create with enough animations to have one per pixel, depending on the animation
  // effect, you may need more or less.
  //
  // since the normal animation time range is only about 65 seconds, by passing timescale value
  // to the NeoPixelAnimator constructor we can increase the time range, but we also increase
  // the time between the animation updates.   
  // NEO_CENTISECONDS will update the animations every 100th of a second rather than the default
  // of a 1000th of a second, but the time range will now extend from about 65 seconds to about
  // 10.9 minutes.  But you must remember that the values passed to StartAnimations are now 
  // in centiseconds.
  //
  // Possible values from 1 to 32768, and there some helpful constants defined as...
  // NEO_MILLISECONDS        1    // ~65 seconds max duration, ms updates
  // NEO_CENTISECONDS       10    // ~10.9 minutes max duration, centisecond updates
  // NEO_DECISECONDS       100    // ~1.8 hours max duration, decisecond updates
  // NEO_SECONDS          1000    // ~18.2 hours max duration, second updates
  // NEO_DECASECONDS     10000    // ~7.5 days, 10 second updates
  //
  
  #if defined(NEOPIXEBUS_NO_STL)
  // for AVR, you need to manage the state due to lack of STL/compiler support
  // for Esp8266 you can define the function using a lambda and state is created for you
  // see below for an example
  struct MyAnimationState
  {
      RgbColor StartingColor;  // the color the animation starts at
      RgbColor EndingColor; // the color the animation will end at
      AnimEaseFunction Easeing; // the acceleration curve it will use 
  };
  
  MyAnimationState animationState[PixelCount];
  // one entry per pixel to match the animation timing manager
  
  void AnimUpdate(const AnimationParam& param)
  {
      // first apply an easing (curve) to the animation
      // this simulates acceleration to the effect
      float progress = animationState[param.index].Easeing(param.progress);
  
      // this gets called for each animation on every time step
      // progress will start at 0.0 and end at 1.0
      // we use the blend function on the RgbColor to mix
      // color based on the progress given to us in the animation
      RgbColor updatedColor = RgbColor::LinearBlend(
          animationState[param.index].StartingColor,
          animationState[param.index].EndingColor,
          progress);
      // apply the color to the strip
      strip.SetPixelColor(param.index, updatedColor);
  }
  #endif
  
  void SetRandomSeed()
  {
      uint32_t seed;
  
      // random works best with a seed that can use 31 bits
      // analogRead on a unconnected pin tends toward less than four bits
      seed = analogRead(0);
      delay(1);
  
      for (int shifts = 3; shifts < 31; shifts += 3)
      {
          seed ^= analogRead(0) << shifts;
          delay(1);
      }
  
      // Serial.println(seed);
      randomSeed(seed);
  }
  
  void setup()
  {
      Serial.begin(115200);
      while (!Serial); // wait for serial attach
  
      strip.Begin();
      strip.Show();
  
  ///////////////////////////////////////////////
    EEPROM.begin(512);
    pinMode(buzzer, OUTPUT); // Set buzzer - pin as an output
    loadSettingsFromEEPROM(); //Load previous settings
  
  
    // init WiFi
    WIFI_Connect();
  
    PRINT("\nConnected to ", ssid);
    PRINTS("\nIP address: ");
    Serial.println(WiFi.localIP());
  
  
    ///////////////////////////////////////////////////////////////
    //HTTP Server
    //////////////////////////////////////////////////////////////
  
    PRINTS("\nHTTP server setup");
    server.on("/", srv_handle_index_html);
    server.on("/form",handleForm);
    server.on("/debug", srv_handle_debug);  //present some debug info
    server.onNotFound(srv_handle_not_found);
    server.begin();
    PRINTS("\nHTTP server started.");
  
    PRINTS("\nready!");
  
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //OTA Code
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    MDNS.begin(otahost);
  
    httpUpdater.setup(&httpServer, update_path, update_username, update_password);
    httpServer.begin();
  
    MDNS.addService("http", "tcp", 90);
  
    Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser and login with username '%s' and password '%s'\n", otahost, update_path, update_username, update_password);
  
    //OTA code end
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////
    //NTP
    ///////////////////////////////////////////////
    timeClient.begin();
  
  
    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setCallback(callback);   //MQTT Callback
  
    lastUpdate = millis();
    lastSecond = millis();
    lastSubSecond = millis();
  
  //////////////////////////////////////////////
  
  
  
      SetRandomSeed();
  /*
      // just pick some colors
      for (uint16_t pixel = 0; pixel < PixelCount; pixel++)
      {
          RgbColor color = RgbColor(random(255), random(255), random(255));
          strip.SetPixelColor(pixel, color);
      }
  */
      Serial.println();
      Serial.println("Running...");
  }
  
  
  void SetupAnimationSet()
  {
      // setup some animations
      for (uint16_t pixel = 0; pixel < PixelCount; pixel++)
      {
          const uint8_t peak = 128;
  
          // pick a random duration of the animation for this pixel
          // since values are centiseconds, the range is 1 - 4 seconds
          uint16_t time = random(100, 400);
  
          // each animation starts with the color that was present
          RgbColor originalColor = strip.GetPixelColor(pixel);
          // and ends with a random color
          RgbColor targetColor = RgbColor(random(peak), random(peak), random(peak));
          // with the random ease function
          AnimEaseFunction easing;
  
          switch (random(3))
          {
          case 0:
              easing = NeoEase::CubicIn;
              break;
          case 1:
              easing = NeoEase::CubicOut;
              break;
          case 2:
              easing = NeoEase::QuadraticInOut;
              break;
          }
  
  #if defined(NEOPIXEBUS_NO_STL)
          // each animation starts with the color that was present
          animationState[pixel].StartingColor = originalColor;
          // and ends with a random color
          animationState[pixel].EndingColor = targetColor;
          // using the specific curve
          animationState[pixel].Easeing = easing;
  
          // now use the animation state we just calculated and start the animation
          // which will continue to run and call the update function until it completes
          animations.StartAnimation(pixel, time, AnimUpdate);
  #else
          // we must supply a function that will define the animation, in this example
          // we are using "lambda expression" to define the function inline, which gives
          // us an easy way to "capture" the originalColor and targetColor for the call back.
          //
          // this function will get called back when ever the animation needs to change
          // the state of the pixel, it will provide a animation progress value
          // from 0.0 (start of animation) to 1.0 (end of animation)
          //
          // we use this progress value to define how we want to animate in this case
          // we call RgbColor::LinearBlend which will return a color blended between
          // the values given, by the amount passed, hich is also a float value from 0.0-1.0.
          // then we set the color.
          //
          // There is no need for the MyAnimationState struct as the compiler takes care
          // of those details for us
          AnimUpdateCallback animUpdate = [=](const AnimationParam& param)
          {
              // progress will start at 0.0 and end at 1.0
              // we convert to the curve we want
              float progress = easing(param.progress);
  
              // use the curve value to apply to the animation
              RgbColor updatedColor = RgbColor::LinearBlend(originalColor, targetColor, progress);
              strip.SetPixelColor(pixel, updatedColor);
          };
  
          // now use the animation properties we just calculated and start the animation
          // which will continue to run and call the update function until it completes
          animations.StartAnimation(pixel, time, animUpdate);
  #endif
      }
  }
  
  void loop()
  {
  ////////////////////////////////////////////////////////////////////////
  
    if (WiFi.status() != WL_CONNECTED) {
      WIFI_Connect();
    }
    else {
      server.handleClient();
      httpServer.handleClient(); // OTA code
  
      timeClient.update();
      if ((myPowerOnTime == 0) && (timeClient.getEpochTime() > 1483228800)) {
        myPowerOnTime = timeClient.getEpochTime(); // Set the power on time after NTP sync. NTP sync is confirmed by comparing epoch of 1-Jan-2017 with epoch of time library.
        
        //check Timer set in EEPROM is valid or not ..
        timerSeconds = (endTimer - timeClient.getEpochTime());
        startTimer = millis();
        PRINT("\n endTimer:",endTimer);
        PRINT("\n timeClient.getEpochTime:",timeClient.getEpochTime());        
        PRINT("\n timerSeconds:",timerSeconds);
        PRINT("\n startTimer:",startTimer);
        PRINT("\n runningTimer:",runningTimer);        
                
        if (timerSeconds < 0) {timerSeconds=0; runningTimer=false;}
      }
  
      //MQTT
      if (!mqttClient.connected()) {
        reconnect();
      }
      else {
        mqttClient.loop();
      }
      
    }
  
    if (runningTimer){timerRun(timerSeconds);}
  
    if ((runningClock)&& !auto_cycle && !animRunning && !colorLight) {clockRun();  }
  
  
    if (auto_cycle){
      if (millis() - auto_last_change > 10000) { // cycle effect mode every 10 seconds
      next_mode = random(19); //Set it one number more than total effects. Returns upto max-1
      PRINT("\n mode is ",next_mode);
      auto_last_change = millis();
      }
    //runningClock=false;
    runSelectedEffect(next_mode);
    }
    //else {runningClock = EEPROM.read(1);}
  
    
  
    //Ping GW, if not reachable reset ESP.
    if ((millis() - prevEvtTime) > 360000) {          //every 6 min (6*60*1000 ms)
      prevEvtTime = millis();
      if (Ping.ping(gateway)) {
        PRINTS("\nGW Pinging!!");
      }
      else {
        PRINTS("\nGW not Pinging, Restarting Now :");
        ESP.restart();
      }
    }
  
  /////////////////////////////////////////////////////////////////////  
  
  if (animRunning){
       if (animations.IsAnimating())
      {
          // the normal loop just needs these two to run the active animations
          animations.UpdateAnimations();
  ////        strip.Show();
      }
      else
      {
          Serial.println();
          Serial.println("Setup Next Set...");
          // example function that sets up some animations
          SetupAnimationSet();
      }
  }
  
      strip.Show();
  }
  
  //////////////////////////////////////////////////
  void runSelectedEffect (int selectedEffect) {
  
    switch(selectedEffect) {
      
      case 0  : {
                  // RGBLoop - no parameters
                  RGBLoop();
                  break;
                }
  
      case 1  : {
                  // FadeInOut - Color (red, green. blue)
                  FadeInOut(0xff, 0x00, 0x00); // red
                  FadeInOut(0xff, 0xff, 0xff); // white 
                  FadeInOut(0x00, 0x00, 0xff); // blue
                  break;
                }
                
      case 2  : {
                  // Strobe - Color (red, green, blue), number of flashes, flash speed, end pause
                  Strobe(0xff, 0xff, 0xff, 10, 50, 1000);
                  break;
                }
  
      case 3  : {
                  // HalloweenEyes - Color (red, green, blue), Size of eye, space between eyes, fade (true/false), steps, fade delay, end pause
                  HalloweenEyes(0xff, 0x00, 0x00, 
                                1, 4, 
                                true, random(5,50), random(50,150), 
                                random(1000, 10000));
                  HalloweenEyes(0xff, 0x00, 0x00, 
                                1, 4, 
                                true, random(5,50), random(50,150), 
                                random(1000, 10000));
                  break;
                }
                
      case 4  : {
                  // CylonBounce - Color (red, green, blue), eye size, speed delay, end pause
                  CylonBounce(0xff, 0x00, 0x00, 4, 10, 50);
                  break;
                }
                
      case 5  : {
                  // NewKITT - Color (red, green, blue), eye size, speed delay, end pause
                  NewKITT(0xff, 0x00, 0x00, 8, 10, 50);
                  break;
                }
                
      case 6  : {
                  // Twinkle - Color (red, green, blue), count, speed delay, only one twinkle (true/false) 
                  Twinkle(0xff, 0x00, 0x00, 10, 100, false);
                  break;
                }
                
      case 7  : { 
                  // TwinkleRandom - twinkle count, speed delay, only one (true/false)
                  TwinkleRandom(20, 100, false);
                  break;
                }
                
      case 8  : {
                  // Sparkle - Color (red, green, blue), speed delay
                  Sparkle(0xff, 0xff, 0xff, 0);
                  break;
                }
                 
      case 9  : {
                  // SnowSparkle - Color (red, green, blue), sparkle delay, speed delay
                  SnowSparkle(0x10, 0x10, 0x10, 20, random(100,1000));
                  break;
                }
                
      case 10 : {
                  // Running Lights - Color (red, green, blue), wave dealy
                  RunningLights(0xff,0x00,0x00, 50);  // red
                  RunningLights(0xff,0xff,0xff, 50);  // white
                  RunningLights(0x00,0x00,0xff, 50);  // blue
                  break;
                }
                
      case 11 : {
                  // colorWipe - Color (red, green, blue), speed delay
                  colorWipe(0x00,0xff,0x00, 50);
                  colorWipe(0x00,0x00,0x00, 50);
                  break;
                }
  
      case 12 : {
                  // rainbowCycle - speed delay
                  rainbowCycle(20);
                  break;
                }
  
      case 13 : {
                  // theatherChase - Color (red, green, blue), speed delay
                  theaterChase(0xff,0,0,50);
                  break;
                }
  
      case 14 : {
                  // theaterChaseRainbow - Speed delay
                  theaterChaseRainbow(50);
                  break;
                }
  
      case 15 : {
                  // Fire - Cooling rate, Sparking rate, speed delay
                  Fire(55,120,15);
                  break;
                }
  
  
                // simple bouncingBalls not included, since BouncingColoredBalls can perform this as well as shown below
                // BouncingColoredBalls - Number of balls, color (red, green, blue) array, continuous
                // CAUTION: If set to continuous then this effect will never stop!!! 
                
      case 16 : {
                  // mimic BouncingBalls
                  byte onecolor[1][3] = { {0xff, 0x00, 0x00} };
                  BouncingColoredBalls(1, onecolor, false);
                  break;
                }
  
      case 17 : {
                  // multiple colored balls
                  byte colors[3][3] = { {0xff, 0x00, 0x00}, 
                                        {0xff, 0xff, 0xff}, 
                                        {0x00, 0x00, 0xff} };
                  BouncingColoredBalls(3, colors, false);
                  break;
                }
  
      case 18 : {
                  // meteorRain - Color (red, green, blue), meteor size, trail decay, random trail decay (true/false), speed delay 
                  meteorRain(0xff,0xff,0xff,10, 64, true, 30);
                  break;
                }
    } //Switch ends
    
   } 
  
  
  /////////////////////////////////////////////////
  
  void clockCallback(uint8_t brightness) {
    PRINTS("\nClock changed to ");
  
    if (brightness) {
      PRINT("ON, brightness ", brightness);
      strip.SetBrightness(brightness);
      runningClock = true;
     // mqttClient.publish(mqtt_colorLight_clock_state, "ON");
    }
    else  {
      PRINTS("OFF");
  ////    ws2812fx.stop();
      setAll(0,0,0);
      runningClock = false;
   //   mqttClient.publish(mqtt_colorLight_clock_state, "OFF");
    }
  
    saveSettingsToEEPROM();
  }
  
  
  //the my timer device callback function has two parameters, it usages brightness parameter as Minutes of timer.
  
  
  void clockRun() {
    //PRINT("\nLoading Clock ",brightness);
  
    if ((millis() - lastSubSecond) > 100) {
      myPattern(); //run centisecond led
    }
  
    if ((millis() - lastSecond) > 1000)
    {
      //My clock has LED-0 at place of 12-O-Clock
  
      //turn off leds that were turned on last Sec
      currentHourLed = (currentHour * 60 + currentMinute) / 12;
      currentMinLed = (currentMinute);
      currentSecLed = (currentSecond);
      if (currentHourLed > 59) {
        currentHourLed = 0;
      }
      if (currentMinLed > 59) {
        currentMinLed = 0;
      }
      if (currentSecLed > 59) {
        currentSecLed = 0;
      }
  
      //PRINT("\nHourLED: ",currentHourLed);
  
      strip.SetPixelColor(currentSecLed, black);
      strip.SetPixelColor(currentMinLed, black);
      strip.SetPixelColor(currentHourLed, black);
  
  ////    ws2812fx.show();
      lastSecond = millis();
  
  
      //Update LED info and turn on LED's
      //timeClient.getHours()   timeClient.getMinutes()     timeClient.getSeconds()
      currentHour = timeClient.getHours();
      currentMinute = timeClient.getMinutes();
      currentSecond =  timeClient.getSeconds();
  
      if (currentHour >= 12) {
        currentHour = currentHour - 12;
      }
  
      String currentTime = String(currentHour) + ':' + String(currentMinute) + ':' + String(currentSecond);
      PRINT("\nTime:", currentTime);
      Serial.print(F("Heap in loop start: "));
      Serial.println(system_get_free_heap_size());
  
      //My clock has LED-0 at place of 12-O-Clock
      currentHourLed = (currentHour * 60 + currentMinute) / 12;
      currentMinLed = (currentMinute);
      currentSecLed = (currentSecond);
      if (currentHourLed > 59) {
        currentHourLed = 0;
      }
      if (currentMinLed > 59) {
        currentMinLed = 0;
      }
      if (currentSecLed > 59) {
        currentSecLed = 0;
      }
  
  
  
      strip.SetPixelColor(currentSecLed, blue);
      strip.SetPixelColor(currentMinLed, green);
      strip.SetPixelColor(currentHourLed, red);
  
  ////    ws2812fx.show();
      checkAlarm();
    }
  }
  
  
  
  
  void myPattern() {
  
    lastSubSecond = millis();
    subSecPixel++;    //next subsecond pixel
    if (subSecPixel == PixelCount + 1) {
      subSecPixel = 0; //Restart form first pixel when reached last pixel
    }
  
  
    //turn off last pixel. Do not turn off hour/minute/second pixel
    if ((subSecPixel - 1 == currentSecLed) or (subSecPixel - 1 == currentMinLed) or (subSecPixel - 1 == currentHourLed)) {
      return;
    }
    else {
      //PRINT("subSecPixel Off: ",subSecPixel);
      strip.SetPixelColor(subSecPixel - 1, black);
  ////    ws2812fx.show();
    }
  
    //turn on next pixel
    if ((subSecPixel == currentSecLed) or (subSecPixel == currentMinLed) or (subSecPixel == currentHourLed)) {
      return;
    }
    else {
      //PRINT("\nsubSecPixel On: ",subSecPixel);
      strip.SetPixelColor(subSecPixel, cyan);
  ////    ws2812fx.show();
    }
  
    //toggle clolors if Hour and Min are on same LED
    if (currentMinLed == currentHourLed) {
      if (toggleMinHour) {
        strip.SetPixelColor(currentMinLed, green);
        toggleMinHour = false;
      }
      else {
        strip.SetPixelColor(currentHourLed, red);
        toggleMinHour = true;
      }
  ////    ws2812fx.show();
    }
  
  }
  
  
  
  
  void checkAlarm() {
    //PRINT("\nalarmHour: ",alarmHour);
    //PRINT("\nalarmMinute: ",alarmMinute);
  
    //PRINT("\nHH:",timeClient.getHours());
    //PRINT("\nMM:",timeClient.getMinutes());
  
    if (timeClient.getHours() == alarmHour && timeClient.getMinutes() == alarmMinute) {
  
      if (alarmStartTime == 0) {
        alarmStartTime = millis();
        PRINT("\nalarmStartTime", alarmStartTime);
      }
  
      if (((int)(millis() / 1000) - (int)(alarmStartTime / 1000)) % 2 == 0) {
        tone(buzzer, 1000); Serial.println("HIGH");
      }
  
      if (((int)(millis() / 1000) - (int)(alarmStartTime / 1000)) % 2 != 0) {
        noTone(buzzer); PRINTLN("LOW");
      }
  
      if (millis() - alarmStartTime >= 60000) {
        alarmStartTime = 0;
        noTone(buzzer); PRINTLN("LOW");
        PRINTLN("Alarm OFF");
        return;
      }
    }
    else {
      return;
    }
  }
  
  
  
  ///////////////////////////////////////////
  //WebServer
  ////////////////////////////////////////////
  
  
  /*
     Build <li> string for all modes.
  
  void modes_setup() {
    modes = "";
    uint8_t num_modes = sizeof(myModes) > 0 ? sizeof(myModes) : ws2812fx.getModeCount();
    for (uint8_t i = 0; i < num_modes; i++) {
      uint8_t m = sizeof(myModes) > 0 ? myModes[i] : i;
      modes += "<li><a href='#' class='m' id='";
      modes += m;
      modes += "'>";
      modes += ws2812fx.getModeName(m);
      modes += "</a></li>";
    }
  }
  */
  /* #####################################################
    #  Webserver Functions
    ##################################################### */
  
  void srv_handle_not_found() {
    server.send(404, "text/plain", "File Not Found");
  }
  
  void srv_handle_index_html() {

  String p = index_html;  
  p.replace("@@color@@",setcolor);
  
  if(WiFi.status() == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    p.replace("@@ip@@", ipStr);
  }
    
    String userInput = server.arg(0);
    //PRINT("\nUserInput received is " , userInput);
    if (userInput == "1On") {
      // PRINTS("\nTurning 1On");
      setAll(255,255,255);
      colorLight = true;
      server.send(200, "text/html", p);
    }
    else if (userInput == "1Off") {
      // PRINTS("\nTurning 1Off");
      setAll(0,0,0);
      colorLight = false;
      server.send(200, "text/html", p);
    }
    else if (server.hasArg("runningClock")) {
      runningClock = server.arg("runningClock").toInt();
      server.send(200, "text/html", p);      
    }
    else if (server.hasArg("Brightness")) {
      strip.SetBrightness(server.arg("Brightness").toInt());
      server.send(200, "text/html", p);      
    }     
    else if (server.hasArg("nextTimer")) {
      String nextTimer = server.arg("nextTimer");
      PRINT("\nNextTimer input is there:", nextTimer);
      timerSeconds = (getEpochFromString(nextTimer)- timeClient.getEpochTime());
      //One simple call, with the address first and the object second.
   //   time_t temp_getEpochFromString=getEpochFromString(nextTimer);
   //long temp_getEpochFromString = 1581274504;
      EEPROMWriteTime(12, getEpochFromString(nextTimer));
      
      if (timerSeconds > 0) {
        PRINT("\nTimer ON for Seconds: ", timerSeconds);
        startTimer = millis();
        runningTimer = true;
      }
      else  {
        PRINTLN("Timer OFF");
  ////      ws2812fx.stop();
        runningTimer = false;
        startTimer = 0;
        timerSeconds = 0;
      }
      yield();
      server.send_P(200, "text/html", "nextTimer request received by Clock. http://192.168.1.167/");
    }
    else if (server.hasArg("showAlarm")) {
      PRINT("\nshowAlarm input is there", server.arg("showAlarm"));
      if (server.arg("showAlarm").toInt() == 0) {
        //clockCallback(100);
        auto_cycle = false;
      }
      if (server.arg("showAlarm").toInt() == 1) {
       // clockCallback(0);
        auto_cycle = true;
      }
      server.send_P(200, "text/html", "nextAlarm request received by Clock. <a href=\"http://192.168.1.167/\">Infinity Mirror & Clock</a>");
    }
  
    else if (server.hasArg("showReminder")) {
      PRINT("\nshowReminder input is there", server.arg("showReminder"));
      if (server.arg("showReminder").toInt() == 0) {
        //clockCallback(100);
        auto_cycle = false;
      }
      if (server.arg("showReminder").toInt() == 1) {
//        clockCallback(0);
        auto_cycle = true;
      }
      server.send_P(200, "text/html", "nextReminder request received by Clock. <a href=\"http://192.168.1.167/\">Infinity Mirror & Clock</a>");
    }
  
    else {
      server.send(200, "text/html", p);
    }
      saveSettingsToEEPROM();
  }
  

  
  
  void srv_handle_debug() {
    String webpage = "<!DOCTYPE HTML>\r\n";
    webpage += "<html>\r\n";
    webpage += "<head>";
    webpage += "<meta name='mobile-web-app-capable' content='yes' />";
    webpage += "<meta name='viewport' content='width=device-width' />";
    webpage += "</head>";
  
    webpage += "<body>";
    webpage += "<i><font face=\"ravie\"><h1>Debug Info</h1></font></i>";
  
    webpage += "<p>epower-esp power on Time:" + time2string(myPowerOnTime) + "</p>";
    webpage += "<p>epower-esp Last Reset:" + ESP.getResetInfo() + "</p>";
    webpage += "</body></html>\r\n";
  
    server.send(200, "text/html", webpage);
  }

//=======================================================================
//                    Handle Set Color
//=======================================================================
void handleForm() {
  String color = server.arg("color");
  //form?color=%23ff0000
  setcolor = color; //Store actual color set for updating in HTML
  Serial.println(color);

  //See what we have recived
  //We get #RRGGBB in hex string

  // Get rid of '#' and convert it to integer, Long as we have three 8-bit i.e. 24-bit values
  long number = (int) strtol( &color[1], NULL, 16);

  //Split them up into r, g, b values
  uint8_t valueR = number >> 16;
  uint8_t valueG = (number >> 8) & 0xFF;
  uint8_t valueB = number & 0xFF;

      setAll(valueR,valueG,valueB);
      if ( valueR==0 && valueG==0 && valueB==0){
        colorLight = false;
        } else {
      colorLight=true;}

  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Updated-- Press Back Button");
  saveSettingsToEEPROM();
  delay(500);  
}
  
  //////////////////////////////////////////////////////////////////////////////////////////////////////
  
  void WIFI_Connect() {
    WiFi.disconnect();
    PRINTS("\nConnecting WiFi...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, password);
    WiFi.config(staticIP, gateway, subnet, dnsIP1);
    // Wait for connection
    for (uint8_t i = 0; i < 25; i++) {
      if (WiFi.status() != WL_CONNECTED)  {
        delay ( 500 );
        PRINTS ( "." );
      }
    }
  } // end of WIFI_Connect
  
  
  
  
  
  
  
  ////////////////////////////////////////////////////////////////////////////////////////////
  
  //MQTT Functions
  void callback(char* topic, byte* payload, unsigned int length) {
    PRINT("\nMessage arrived [", topic);
    PRINTS("] ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  
  
    char c_payload[length];
    memcpy(c_payload, payload, length);
    c_payload[length] = '\0';
  
    String s_topic = String(topic);
    String s_payload = String(c_payload);
    Serial.print(s_topic);
    Serial.println(s_payload);
  
  
  
  
    // Switch on/off lights or fan, change color
    if (s_topic == mqtt_colorLight_command_topic) {
  
      uint8_t valueR = s_payload.substring(s_payload.indexOf('(') + 1, s_payload.indexOf(',')).toInt();
      uint8_t valueG = s_payload.substring(s_payload.indexOf(',') + 1, s_payload.lastIndexOf(',')).toInt();
      uint8_t valueB = s_payload.substring(s_payload.lastIndexOf(',') + 1).toInt();
      PRINTLN("Loading color preset with following ");
      Serial.println(valueR);
      Serial.println(valueG);
      Serial.println(valueB);
  
      setAll(valueR,valueG,valueB);
      colorLight=true;
   ////   if ( valueR==0 && valueG==0 && valueB==0){runningClock = EEPROM.read(1);}else{ runningClock=false;}
      if ( valueR==0 && valueG==0 && valueB==0){
        colorLight = false;
        }
  
      if (s_payload == "ON") {
            setAll(valueR,valueG,valueB);
            colorLight=true;
            mqttClient.publish(mqtt_colorLight_state_topic, "ON");
      }
      if (s_payload == "OFF")  {
            setAll(0,0,0);
            colorLight=false;
            mqttClient.publish(mqtt_colorLight_state_topic, "OFF");
      }
    }
  
    // Switch on/off clock
  /*
    if (s_topic == mqtt_colorLight_clock_command) {
      if (s_payload == "ON") {
        clockCallback(100);
      }
      if (s_payload == "OFF") {
        clockCallback(0);
      }
    }
 */ 
    // clock dimmer
    if (s_topic == mqtt_colorLight_clock_dimmer_command) {
      PRINTS("\nmqtt clock dimmer command is there");
      clockCallback(s_payload.toInt() * 255 / 100);
      mqttClient.publish(mqtt_colorLight_clock_dimmer_state, c_payload); //charArray to be used in publish
    }
  
  
    saveSettingsToEEPROM();
  }
  
  
  void reconnect() {
    // Loop until we're reconnected
    for (uint8_t i = 0; i < 5; i++) {
      if (!mqttClient.connected()) {
        PRINTLN("Attempting MQTT connection...");
        Serial.print(F("Heap in loop start: "));
        Serial.println(system_get_free_heap_size());
        // Create a random client ID
        String mqttClientId = "ESP8266Client-";
        mqttClientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (mqttClient.connect(mqttClientId.c_str())) {
          PRINTLN("connected");
          // Once connected, publish an announcement...
          //mqttClient.publish("outTopic", "hello world");
          // ... and resubscribe
          //mqttClient.subscribe("inTopic");
  
          if (mqttClient.subscribe(mqtt_colorLight_command_topic)) {
            PRINTLN("Subscribed: colorLight/color/command");
          }
      /*
          if (mqttClient.subscribe(mqtt_colorLight_clock_command)) {
            PRINTLN("Subscribed: colorLight/clock/command");
          }
      */    
          if (mqttClient.subscribe(mqtt_colorLight_clock_dimmer_command)) {
            PRINTLN("Subscribed: colorLight/clock/dimmer/command");
          }
  
        } else {
          PRINT("\nFailed, rc=", mqttClient.state());
          PRINTLN(" try again in 5 seconds");
          // Wait 5000 ms before retrying
          delay(5000);
        }
      }
    }
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  
  
  
  
  
  void timerRun (int16_t timerSeconds) {
    int timerMinute;
  
    if (timerSeconds > 3600) {
      return; //We have 60 LED so reject timers greater than 60 minutes.
    }
  
    //remaining time in minutes
  
    int temp_timerSeconds = timerSeconds * 1000 - (millis() - startTimer); //in milliSeconds
  
    PRINT("\n timerSeconds:", timerSeconds);
    PRINT("\n millies:", millis());
    PRINT("\n startTimer:", startTimer);
    PRINT("\n temp_timerSeconds:", temp_timerSeconds);
  
    if (temp_timerSeconds > 30000) {
      timerMinute = temp_timerSeconds / (60 * 1000);
      //Round up
      if (temp_timerSeconds % (60 * 1000) > 0) {
        ++timerMinute;
      }
    
                  //turn off all pixel except hour/minute/second pixel
                for ( int i = 0; i < PixelCount; i++) {
                  if (i == currentSecLed or i == currentMinLed or i == currentHourLed) {
                    yield();
                  }
                  else {
                    strip.SetPixelColor(i, black);
                    yield();
                  }
                }
    
    
                    //turn on pixel equal to timerMinute.
                  for ( int i = 0; i <= timerMinute; i++) {
                    if (runningClock && (i == currentSecLed or i == currentMinLed or i == currentHourLed)) {
                      yield(); //if Clock is running then skip hour min sec pixcels
                    }
                    else {
                      strip.SetPixelColor(i, white);
                      // PRINT("\n SetTimerLedNumber: ",i); //remove comment to print numbers of LED turned on
                    }
                  }
    
    }
    else {
      timerMinute = 0;
    }
  
  
  
  
    //if timer is near expiry run some animations
    if (temp_timerSeconds < 30000) { //run animtion for 30 Sec
      PRINTLN("Timer near expiry : Run some Anim");
  
      if (!animRunning) {
        animRunning = true;
        ///runningClock = false;
  ////      ws2812fx.setSegment(0, 0, PixelCount - 1, FX_MODE_RAINBOW_CYCLE, RED, 2000, false);
  ////      ws2812fx.start();
      }
  
    }
  
  
    //if timer is over, reset timer parameters and break from function. Add any animations you want to play at end of timer here.
    if (temp_timerSeconds <= 0) {
      PRINT("\n Timer Stop at : ", millis());
   ////   ws2812fx.show();
      strip.SetPixelColor(0, black);
      startTimer = 0;
      timerMinute = 0;
      timerSeconds = 0;
      runningTimer = false;
      //auto_cycle=false;
      animRunning = false;
      ///runningClock = EEPROM.read(1); 
   ////   ws2812fx.stop();
      return;
    }
  
  
    PRINT("\n timerMinute: ", timerMinute);
 //   strip.SetBrightness(100); //if not set here then brightness is lower with small timers
  ////  ws2812fx.show();
  }
