#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>
#include "GSM_SIM800.h"

#define RXD2 16
#define TXD2 17

#define MTS

/*class MyTinyGSM : public TinyGsmSim800
{

};*/

GSM_SIM800 modemGSM(Serial2);

#define SIM_PIN ""
#ifdef BEELINE
  #define APN_NAME "internet.beeline.ru"
  #define APN_USER "beeline"
  #define APN_PSWD "beeline"
#else
  #define APN_NAME "internet.mts.ru"
  #define APN_USER "mts"
  #define APN_PSWD "mts"
#endif

#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 38400
#define Max_Modem_Reboots 5

int Modem_Reboots_Counter = 0;

void Init_GSM_SIM800() 
{
  Serial.println("Initialize GSM modem...");
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial GSM Txd is on GPIO" + String(TXD2));
  Serial.println("Serial GSM Rxd is on GPIO" + String(RXD2));

  //delay(3000);

  TinyGsmAutoBaud(Serial2, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);

  String info = modemGSM.getModemInfo();
  
  Serial.println(info);

  if (!modemGSM.restart())
  {
    RestartGSMModem();
  }
  else
  {
    Serial.println("Modem restart OK");
  }

  modemGSM.SetAutoTimeSync();
  
  if (modemGSM.getSimStatus() != SIM_READY)
  {
    Serial.println("Check PIN code for the SIM. SIM status: " + SimStatus(modemGSM.getSimStatus()));
    if (SIM_PIN != "")
    {
     Serial.println("Try to unlock SIM PIN.");
      modemGSM.simUnlock(SIM_PIN);
      delay(3000);
      if (modemGSM.getSimStatus() != 3)
      {
        RestartGSMModem();
      }
    }
  }
 
  if (!modemGSM.waitForNetwork()) 
  {
    Serial.println("Failed to connect to network");
    RestartGSMModem();
  }
  else
  {
    RegStatus registration = modemGSM.getRegistrationStatus();
    Serial.println("Registration: [" + modemGSM.GSMRegistrationStatus(registration) + "]"); 
    Serial.println("Modem network OK");
  }
  
  Serial.println(modemGSM.gprsConnect(APN_NAME,APN_USER,APN_PSWD) ? "GPRS Connect OK" : "GPRS Connection failed");

  bool stateGPRS = modemGSM.isGprsConnected();
  if (!stateGPRS)
  {
    RestartGSMModem();
  }
  
  String state = stateGPRS ? "connected" : "not connected";
  Serial.println("GPRS status: " + state);
  Serial.println("CCID: " + modemGSM.getSimCCID());
  Serial.println("IMEI: " + modemGSM.getIMEI());
  Serial.println("Operator: " + modemGSM.getOperator());

  /*IPAddress local = modemGSM.localIP();
  Serial.println("Local IP: " + local.toString());*/

  int csq = modemGSM.getSignalQuality();
  if (csq == 0)
  {
    Serial.println("Signal quality is 0. Restart modem.");
    RestartGSMModem();
  }
  Serial.println("Signal quality: " + modemGSM.GSMSignalLevel(csq) + " [" + String(csq) + "]");

  int battLevel = modemGSM.getBattPercent();
  Serial.println("Battery level: " + String(battLevel) + "%");

  float battVoltage = modemGSM.getBattVoltage() / 1000.0F;
  Serial.println("Battery voltage: " + String(battVoltage));

  String gsmLoc = modemGSM.getGsmLocation();
  Serial.println("GSM location: " + gsmLoc);
  //Serial.println("GSM location: " + SwapLocation(gsmLoc));

  modemGSM.sendAT(GF("AT+CMEE=2"));
  modemGSM.waitResponse();
}

//Parse GSM modem DateTime string "yy/mm/dd,hh:mm:ss+/-zz"
bool ParseDateTimeString(String datetime, byte &year, byte &month, byte &day, byte &hours, byte &minutes, byte &seconds, byte &timezone)
{ 
   return sscanf(datetime.c_str(), "%d/%d/%d,%d:%d:%d+%d", &year, &month, &day, &hours, &minutes, &seconds, &timezone) == 7;
}

void RestartGSMModem()
{
    Serial.println("Restarting GSM...");
    if (!modemGSM.restart())
    {
      Serial.println("\tFailed. :-(\r\n");
      //ESP.restart();
    } 
    if (Modem_Reboots_Counter < Max_Modem_Reboots)
    {
      Init_GSM_SIM800();
    }
    Modem_Reboots_Counter++;
}
  /*void smsInit()
  {
        //modemGSM.sendAT(GF("+CPMS=ME,ME,ME"));
        modemGSM.sendAT(GF("+CPMS=SM,SM,SM"));
        modemGSM.waitResponse();

        modemGSM.sendAT(GF("+CMGF=1"));
        modemGSM.waitResponse();

        modemGSM.sendAT(GF("+CNMI=1,0"));
        modemGSM.waitResponse();

        modemGSM.sendAT(GF("+CSCS=\"GSM\""));
        modemGSM.waitResponse();
  }*/

void setup() {
  Serial.begin(9600);
  Init_GSM_SIM800();

  GSM_SIM800::SMSmessage msg = modemGSM.readSMS(2);

  Serial.println("------------------------------------");
  Serial.println("Index: " + String(msg.Index));
  Serial.println("Is readed: " + String(msg.IsReaded));
  Serial.println("Number: " + msg.PhoneNumber);
  Serial.println("Date: " + msg.Date);
  Serial.println("Message: " + msg.Message);
  Serial.println("------------------------------------");
  
  //readAllSMSs();
  /*modemGSM.waitResponse(10000L);
  delay(10000);
  String data = modemGSM.stream.readStringUntil('\n');
  Serial.println("Stop waiting for the response.");  
  Serial.println("Data: " + data);*/


  byte year = 0;
  byte month = 0;
  byte day = 0; 
  byte hours = 0;
  byte minutes = 0;
  byte seconds = 0;
  byte timezone = 0;
  ParseDateTimeString("19/05/25,21:50:06+12", year, month, day, hours, minutes, seconds, timezone);
  char str[25];
  sprintf(str, "%d/%d/%d,%d:%d:%d+%d", year, month, day, hours, minutes, seconds, timezone);
  Serial.println(str);

  Serial.println(modemGSM.ShowNTPError(modemGSM.NTPServerSync()));
}

int lastMillis = 0;
void loop() 
{
  int currentMillis = millis();
  if (currentMillis - lastMillis > 60000*5)
  {
    //readSMS1(6);
    GSM_SIM800::SMSmessages messages = modemGSM.readSMSs();
    for (const  GSM_SIM800::SMSmessage message : messages)
    {
      Serial.println("Index: " + String(message.Index));
      Serial.println("Is readed: " + String(message.IsReaded));
      Serial.println("Number: " + message.PhoneNumber);
      Serial.println("Date: " + message.Date);
      Serial.println("Message: " + message.Message);
    }

    GSM_SIM800::Outputs outputs = modemGSM.processIOOHcommand("IOOH <n1 n 5 n7 >");
    for (byte output : outputs)
    {
      Serial.println("Output: " + String(output));
    }
        
    lastMillis = currentMillis;
  }
    
  if (Serial2.available())
  {
    Serial.write(Serial2.read());
  }
  if (Serial.available())
  {
    String str = Serial.readString();
    Serial.println("Command: " + str);
    Serial2.print(str);
  }
}
