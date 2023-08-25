#include <Arduino.h>

#include "WNetwork.h"
#ifndef MINIMAL
#include "WBecaDevice.h"
#include "WClock.h"
#include "WLogDevice.h"
#include "WNetworkDevice.h"
#include "Adafruit_SHT31.h"                                                 //Hakspiel
#include <Wire.h>                                                           //Hakspiel
Adafruit_SHT31 sht31 = Adafruit_SHT31();                                    //Hakspiel
float sht31_temp = 0;                                                       //Hakspiel
float sht31_humidity = 0;                                                   //Hakspiel
bool sht1_found = false;                                                    //Hakspiel
float offset_temp=-4.65;                                                    //Hakspiel
float offset_hum=12.15;                                                       //Hakspiel
char sht_temp_char[14];                                                     //Hakspiel
char sht_hum_char[14];                                                      //Hakspiel
char taupunkt_char[14];                                                     //Hakspiel   
unsigned long time_new=0;                                                   //Hakspiel
unsigned long time_old=0;                                                   //Hakspiel

#endif
#define APPLICATION "Thermostat"
#ifndef VERSION
#define VERSION "1.1" // gets defined in commandline
#endif

#ifdef MINIMAL
#define FULLVERSION VERSION "-minimal"
#elif DEBUG
#define FULLVERSION VERSION "-debug"
#else
#define FULLVERSION VERSION
#endif

#ifdef DEBUG // use platform.io environment to activate/deactive 
#define SERIALDEBUG true  // enables logging to serial console
#ifdef DEBUG_ESP_PORT
#define DEBUG_MSG(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#else
#define DEBUG_MSG(...)
#endif
#define SERIALSPEED 115200
#else
#define SERIALDEBUG false
#define DEBUG_MSG(...)
#define SERIALSPEED 9600
#endif

const byte FLAG_OPTIONS_APPLICATION = 0xF1;

WNetwork *network;
#ifndef MINIMAL
WNetworkDev *networkDevice;
WLogDevice *logDevice;
WBecaDevice *becaDevice;
WClock *wClock;
#endif



void setup() {
    Serial.begin(SERIALSPEED);
    Wire.begin(D6, D5);  //I2C Bus    D5 = SCL_Pin    D6 = SDA Pin       //Hakspiel
    Wire.setClock(100000); //I2C Bus frequency in Hz                     //Hakspiel
    sht31.begin(0x44); //Sensirion SHT31                                 //Hakspiel
    scan_i2c();                                                          //Hakspiel
    

    // Wifi and Mqtt connection
    network = new WNetwork(SERIALDEBUG, APPLICATION, FULLVERSION, NO_LED, FLAG_OPTIONS_APPLICATION);
#ifndef MINIMAL
    network->setOnNotify([]() {
        if (network->isSoftAPDesired()){
            becaDevice->reportNetworkToMcu(mcuNetworkMode::MCU_NETWORKMODE_SMARTCONFIG);
        } else if (network->isStation()){
            if (network->isWifiConnected()) {
                becaDevice->reportNetworkToMcu(mcuNetworkMode::MCU_NETWORKMODE_CONNECTEDCLOUD);
                if (network->isMqttConnected()) {
                    becaDevice->queryAllDPs();
                }
            } else {
                becaDevice->reportNetworkToMcu(mcuNetworkMode::MCU_NETWORKMODE_NOTCONNECTED);
            }
        }
        if (networkDevice) networkDevice->connectionChange();
    });
    network->setOnConfigurationFinished([]() {
        // Switch blinking thermostat in normal operating mode back
        network->log()->warning(F("ConfigurationFinished"));
        becaDevice->cancelConfiguration();
    });


    // networkDevice 
    network->log()->trace(F("Loading Network (%d)"), ESP.getMaxFreeBlockSize());
    networkDevice = new WNetworkDev(network, APPLICATION);
    network->addDevice(networkDevice);
    network->log()->trace(F("Loading Network Done (%d)"), ESP.getMaxFreeBlockSize());

    // KaClock - time sync
    network->log()->trace(F("Loading Clock (%d)"), ESP.getMaxFreeBlockSize());
    wClock = new WClock(network, APPLICATION);
    network->addDevice(wClock);
    wClock->setOnTimeUpdate([]() { becaDevice->sendActualTimeToBecaRequested(true); });
    wClock->setOnError([](const char *error) {
        network->log()->error(F("Clock Error: %s"), error);
    });
    network->log()->trace(F("Loading Clock Done (%d)"), ESP.getMaxFreeBlockSize());

    // Communication between ESP and Beca-Mcu
    network->log()->trace(F("Loading BecaDevice (%d)"), ESP.getMaxFreeBlockSize());
    becaDevice = new WBecaDevice(network, wClock);
    becaDevice->setMqttSendChangedValues(true);
    network->addDevice(becaDevice);

    becaDevice->setOnConfigurationRequest([]() {
        network->setDesiredModeAp();
        return true;
    });
    becaDevice->setOnPowerButtonOn([]() {
        // try to go to Station mode if we should be in any other state
        // example: long press power + down for 8 seconds switches to 
        // WiFi reset mode, but blinking mode is not accepted
        network->setDesiredModeStation();
        return true;
    });
    network->log()->trace(F("Loading BecaDevice Done (%d)"), ESP.getMaxFreeBlockSize());

    // add MQTTLog
    network->log()->trace(F("Loading LogDevice (%d)"), ESP.getMaxFreeBlockSize());
    logDevice = new WLogDevice(network);
    network->addDevice(logDevice);
    network->log()->trace(F("Loading LogDevice Done (%d)"), ESP.getMaxFreeBlockSize());

    if (network->getSettings()->settingsNeedsUpdate()){
        network->deleteSettingsOld();
        network->log()->trace(F("Writing Config (%d)"), ESP.getMaxFreeBlockSize());
        #ifndef DEBUG
        network->getSettings()->save();
        #else
        network->log()->trace(F("Writing Config - not really, because debug MODE"));
        #endif
    }
   if (sht1_found == false) {  
        network->setOnMqttHassAutodiscover([](bool removeDiscovery) {
            // https://www.home-assistant.io/integrations/climate.mqtt/
            return becaDevice->sendMqttHassAutodiscover(removeDiscovery);
    });
     }   

    
   if (sht1_found == true) {                                                           //Hakspiel, incl
        network->setOnMqttHassAutodiscover([](bool removeDiscovery) {
            // https://www.home-assistant.io/integrations/climate.mqtt/
            return becaDevice->sendMqttHassAutodiscover2(removeDiscovery);
        });
    }
    
#endif
    network->log()->trace(F("Starting Webserver (%d)"), ESP.getMaxFreeBlockSize());
    network->startWebServer();
    network->log()->trace(F("Starting Webserver Done (%d)"), ESP.getMaxFreeBlockSize());

    network->log()->trace(F("Starting initStatic (%d)"), ESP.getMaxFreeBlockSize());
    initStatic();
    network->log()->trace(F("Starting initStatic Done (%d)"), ESP.getMaxFreeBlockSize());

}

void loop() {
    
    network->loop(millis());
    delay(50);

    time_new = millis();
    unsigned long cycle_time = time_new - time_old;
    
                                                                                             //hakspiel     
    if (cycle_time >= 10 * 1000){  //run this all 10 sec
      if (sht1_found == true) {
        
        sht31_humidity = sht31.readHumidity() + offset_hum;
        dtostrf(sht31_humidity, 10, 2, sht_hum_char);//convert float to char 
        String hum_topic = network->getMqttTopic() + (String)"/" + String(MQTT_STAT) + (String)"/things/thermostat/SHT_Hum";
        network->publishMqtt((hum_topic).c_str(), sht_hum_char);
        //delay(100);
        sht31_temp = sht31.readTemperature() + offset_temp;
        dtostrf(sht31_temp, 10, 2, sht_temp_char);//convert float to char
        String temp_topic = network->getMqttTopic() + (String)"/" + String(MQTT_STAT) + (String)"/things/thermostat/SHT_Temp";
        network->publishMqtt((temp_topic).c_str(), sht_temp_char);

        //Dewpoint
        float taupunkt = Taupunkt_berechnen(sht31_temp, sht31_humidity);
        dtostrf(taupunkt, 10, 2, taupunkt_char);//convert float to char
        String taupunkt_topic = network->getMqttTopic() + (String)"/" + String(MQTT_STAT) + (String)"/things/thermostat/SHT_Dewpoint";
        network->publishMqtt((taupunkt_topic).c_str(), taupunkt_char);

      }
      time_old = time_new;
    }

    
}

                                                                                            //hakspiel
void scan_i2c()            
{
    byte error;
    Wire.beginTransmission(68);
    error = Wire.endTransmission();
    if (error == 0) sht1_found = true;
}


//taupunkt berechnen
  // See: http://www.wetterochs.de/wetter/feuchte.html
  
float Taupunkt_berechnen(float Temp2, float feuchte){
  float a;
  float b;
  float Saettigungsdampfdruck;
  float Dampfdruck;
  float v;
  float Taupunkt;
  float absolute_Feuchte;
  
  float molekulargewicht_wasserdampf;
  float universelle_Gaskonstante;
  float aTp;
  float bTp;
  
    
    
  universelle_Gaskonstante = 8314.3; //J/(kmol*K) (universelle Gaskonstante)
  molekulargewicht_wasserdampf = 18.016; //kg/kmol (Molekulargewicht des Wasserdampfes)
  
  if(Temp2 >= 0) {
      a = 7.5;
      b = 237.3;
  }else{
      a = 9.5; 
      b = 265.5;
      }
    
        
      Saettigungsdampfdruck = 6.1078 * pow(10,((a*Temp2)/(b+Temp2))); //(T)
      Dampfdruck = feuchte/100 * Saettigungsdampfdruck;        //(r,T)
      v = log10(Dampfdruck/6.1078);                            //(r,T)
      Taupunkt = (b*v)/(a-v);                                    //(r,T)
      
      absolute_Feuchte = pow(10,5) * molekulargewicht_wasserdampf/universelle_Gaskonstante * Dampfdruck/(Temp2+273.15);           //(r,TK)

return Taupunkt ; 
}
