#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>

#include <TimeLib.h>

# define numOfParams 5

enum ReadSMSMode {
  ReceivedUnread,
  ReceivedRead,
  StoredUnsent,
  StoredSent,
  AllSMS
};

enum DeleteSMSMode {
  Read = 1,
    Unread = 2,
    Sent = 3,
    Unsent = 4,
    Received = 5,
    All = 6
};

class GSM_SIM800: public TinyGsmSim800 {
  private: bool IsSMSInited = false;

  public:

    GSM_SIM800(Stream & stream): TinyGsmSim800(stream) {}

  struct SMSmessage {
    int Index;
    bool IsReaded;
    String PhoneNumber;
    String Date;
    String Message;
    TimeElements Time;
    public:
      void SetTime(String date, bool isPrint = false) {
        Time = parseDateTime(date);
        Date = date;
      }

    TimeElements parseDateTime(String date) {
      TimeElements tm;
      uint8_t Year, Month, Day, Hour, Minute, Second, TimeZone;
      const char * str = date.c_str(); //"19/05/05,21:27:58+12";
      sscanf(str, "%d/%d/%d,%d:%d:%d+%d", & Year, & Month, & Day, & Hour, & Minute, & Second, & TimeZone);
      tm.Year = Year; //CalendarYrToTm();
      tm.Month = Month;
      tm.Day = Day;
      tm.Hour = Hour;
      tm.Minute = Minute;
      tm.Second = Second;
      return tm;
    }
  };

  boolean isValidNumber(String str) {
    boolean isNum = false;
    if (!(str.charAt(0) == '+' || str.charAt(0) == '-' || isDigit(str.charAt(0)))) return false;

    for (byte i = 1; i < str.length(); i++) {
      if (!(isDigit(str.charAt(i)) || str.charAt(i) == '.')) return false;
    }
    return true;
  }

  String ShowNTPError(byte error) {
    switch (error) {
    case 1:
      return "Network time synchronization is successful";
    case 61:
      return "Network error";
    case 62:
      return "DNS resolution error";
    case 63:
      return "Connection error";
    case 64:
      return "Service response error";
    case 65:
      return "Service response timeout";
    default:
      return "Unknown error";
    }
  }

  byte NTPServerSync(String server = "pool.ntp.org", byte TimeZone = 3) {
    Serial.println("Sync time with NTP server.");
    sendAT(GF("+CNTPCID=1"));
    if (waitResponse(10000L) != 1) {
      Serial.println("AT command \"AT+CNTPCID=1\" executing fault.");;
    }

    String CNTP = "+CNTP=" + server + "," + String(TimeZone);
    sendAT(GF(CNTP));
    if (waitResponse(10000L) != 1) {
      Serial.println("AT command \"" + CNTP + "\" executing fault.");;
    }

    sendAT(GF("+CNTP"));
    if (waitResponse(10000L, GF(GSM_NL "+CNTP:"))) {
      String result = stream.readStringUntil('\n');
      result.trim();
      if (isValidNumber(result)) {
        return result.toInt();
      }
    } else {
      Serial.println("AT command \"+CNTP\" executing fault.");
    }
    return -1;
  }

  void SetAutoTimeSync() {
    Serial.println("Enable GSM RTC network time sync.");
    sendAT(GF("+CLTS=1"));
    if (waitResponse(10000L) != 1) {
      Serial.println("AT command \"AT+CLTS=1\" executing fault.");;
    }

    sendAT(GF("&W"));
    waitResponse();

    sendAT(GF("+CFUN=0"));
    if (waitResponse(10000L) != 1) {
      Serial.println("AT command \"AT+CFUN=0\" executing fault.");;
    }

    //delay(3000);
    sendAT(GF("+CFUN=1"));
    //if (modemGSM.waitResponse(10000L) != 1) 
    if (waitResponse(10000L, GF(GSM_NL "DST:")) != 1) {
      Serial.println("AT command \"AT+CFUN=1\" executing fault.");;
    }

    Serial.println("DateTime: " + GetDateTime());
  }

  String GetDateTime() {
    sendAT(GF("+CCLK?"));
    if (waitResponse(10000L, GF(GSM_NL "+CCLK:"))) {
      String body = stream.readStringUntil('\n');
      return body;
    } else {
      Serial.println("AT command \"+CCLK?\" executing fault.");
    }
    return "";
  }

  String Trim(String str) {
    str.trim();
    if (str.length() > 0) {
      if (str.startsWith(" "))
        str.remove(0, 1);
      if (str.startsWith("\""))
        str.remove(0, 1);
      if (str.endsWith(" "))
        str.remove(str.length() - 1, 1);
      if (str.endsWith("\""))
        str.remove(str.length() - 1, 1);
    }
    return str;
  }

  boolean isNumber(String str) {
    for (byte i = 0; i < str.length(); i++) {
      if (isDigit(str.charAt(i))) return true;
    }
    return false;
  }

  SMSmessage parseSMSmessage(String response, int index = -1) {
    int pos = 0;
    int i = 0;
    String * list = new String[numOfParams];
    String str = "";
    for (int j = 0; j < response.length(); j++) {
      if (response[j] == ',') {
        if (j + 1 >= response.length()) {
          list[i] = Trim(str);
          i++;
        } else {
          if (response[j + 1] == '\"' || response[j + 1] == ' ') {
            str = Trim(str);
            if (i == 0 && !isNumber(str)) {
              list[0] = String(index);
              i++;
            }
            list[i] = str;
            str = "";
            i++;
          } else {
            str += response[j];
          }
        }
      } else {
        str += response[j];
      }
    }
    list[i] = Trim(str);
    SMSmessage msg;
    msg.Index = list[0].toInt();
    msg.IsReaded = (list[1].indexOf("UNREAD") == -1);
    msg.PhoneNumber = list[2];
    //msg.Date = list[4];
    msg.SetTime(list[4], true);
    return msg;
  }

  String GSMSignalLevel(int level) {
    switch (level) {
    case 0:
      return "-115 dBm or less";
    case 1:
      return "-111 dBm";
    case 31:
      return "-52 dBm or greater";
    case 99:
      return "not known or not detectable";
    default:
      if (level > 1 && level < 31)
        return "-110... -54 dBm";
    }
    return "Unknown";
  }

  String GSMRegistrationStatus(RegStatus state) {
    switch (state) {
    case REG_UNREGISTERED:
      return "Not registered, MT is not currently searching a new operator to register to";
    case REG_SEARCHING:
      return "Not registered, but MT is currently searching a new operator to register to";
    case REG_DENIED:
      return "Registration denied";
    case REG_OK_HOME:
      return "Registered, home network";
    case REG_OK_ROAMING:
      return "Registered, roaming";
    case REG_UNKNOWN:
      return "Unknown";
    }
    return "Unknown";
  }

  String SIMStatus(SimStatus state) {
    switch (state) {
    case SIM_ERROR:
      return "SIM card ERROR";
    case SIM_READY:
      return "SIM card is READY";
    case SIM_LOCKED:
      return "SIM card is LOCKED. PIN/PUK needed.";
    }
    return "Unknown STATUS";
  }

  void smsInit() {
    //modemGSM.sendAT(GF("+CPMS=ME,ME,ME"));
    sendAT(GF("+CPMS=SM,SM,SM"));
    waitResponse();

    sendAT(GF("+CMGF=1"));
    waitResponse();

    sendAT(GF("+CNMI=1,0"));
    waitResponse();

    sendAT(GF("+CSCS=\"GSM\""));
    waitResponse();

    IsSMSInited = true;
  }

  typedef std::vector<byte> Outputs;
  Outputs processIOOHcommand(String message) {
    Outputs outputs;
    if (message.startsWith("IOOH")) {
      int startPos = 0;

      bool detectOutput = false;
      String output = "";
      for (int i = 4; i <= message.length(); i++) {
        if (message[i] == '<') {
          startPos = i;
        } else {
          if (message[i] == 'n' || message[i] == '>') {
            if (output.length() > 0) {
              outputs.push_back(output.toInt());
              output = "";
            }
          } else {
            if (isDigit(message[i])) {
              output += message[i];
            }
          }
        }
      }
    }
    return outputs;
  }

  bool processSMScommand(SMSmessage message) {

  }

  bool processSMScommands() {
    SMSmessages messages = readSMSs();
    for (const SMSmessage message: messages) {
      Serial.println("Index: " + String(message.Index));
      Serial.println("Is readed: " + String(message.IsReaded));
      Serial.println("Number: " + message.PhoneNumber);
      Serial.println("Date: " + message.Date);
      Serial.println("Message: " + message.Message);
    }
  }

  //bool deleteAllSmsMessages(DeleteSmsMode method);
  bool deleteAllSmsMessages(DeleteSMSMode method) {
    // Select SMS Message Format: PDU mode. Spares us space now
    sendAT(GF("+CMGF=0"));

    if (waitResponse() != 1) {
      return false;
    }

    sendAT(GF("+CMGDA="), static_cast <
      const uint8_t > (method));
    const bool ok = waitResponse(25000L) == 1;

    sendAT(GF("+CMGF=1"));
    waitResponse();

    return ok;
  }

  bool deleteSmsMessage(const uint8_t index) {
    sendAT(GF("+CMGD="), index, GF(","), 0); // Delete SMS Message from <mem1> location
    return waitResponse(5000L) == 1;
  }

  SMSmessage readSMS(uint8_t i) {
    if (!IsSMSInited)
      smsInit();
    //char message[300];

    /*modemGSM.sendAT(GF("+CMGF=1"));
    modemGSM.waitResponse();
    modemGSM.sendAT(GF("+CNMI=1,2,0,0,0"));
    modemGSM.waitResponse();*/

    SMSmessage sms; // = (const struct SMSmessage) { "", "", "" };
    sms.PhoneNumber = "";
    sms.Date = "";
    sms.Message = "";

    sendAT(GF("+CMGR="), i);
    if (waitResponse(10000L, GF(GSM_NL "+CMGR:"))) {

      String header = stream.readStringUntil('\n');
      String body = stream.readStringUntil('\n');

      Serial.println("Header: " + header);
      Serial.println("Body: " + body);

      //body.toCharArray(message, body.length());
      //Serial.println("Message: " + String(message));
      sms = parseSMSmessage(header, i);
      sms.Message = body;
    }
    return sms;
  }

  typedef std::vector<SMSmessage> SMSmessages;
  SMSmessages readSMSs(ReadSMSMode mode = ReadSMSMode::AllSMS) {
    /*modemGSM.sendAT(GF("+CPMS=SM,SM,SM"));
    modemGSM.waitResponse();
    modemGSM.sendAT(GF("+CMGF=1"));
    modemGSM.waitResponse();
    //modemGSM.sendAT(GF("+CNMI=0,0,0,0,0"));
    modemGSM.sendAT(GF("+CNMI=1,0,0,0,0"));
    //modemGSM.sendAT(GF("+CNMI=1,2,0,0,0"));
    //modemGSM.sendAT(GF("+CNMI=2,2,0,0,0"));
    modemGSM.waitResponse();*/

    String readMode = "ALL";
    if (!IsSMSInited)
      smsInit();

    switch (mode) {
    case ReadSMSMode::ReceivedUnread:
      readMode = "REC UNREAD";
      break;
    case ReadSMSMode::ReceivedRead:
      readMode = "REC READ";
      break;
    case ReadSMSMode::StoredUnsent:
      readMode = "STO UNSENT";
      break;
    case ReadSMSMode::StoredSent:
      readMode = "STO SENT";
      break;
    case ReadSMSMode::AllSMS:
      readMode = "ALL";
      break;
    }

    SMSmessages messages;
    sendAT(GF("+CMGL=\"" + readMode + "\""));
    Serial.println("-----------------------------------");
    while (true) {
      if (waitResponse(10000L, GF(GSM_NL "+CMGL:"), GFP(GSM_OK), GFP(GSM_ERROR))) {
        String header = stream.readStringUntil('\n');
        SMSmessage msg = parseSMSmessage(header);
        header.trim();
        if (header.length() == 0)
          break;
        //Serial.println("Header: " + header);
        String message = stream.readStringUntil('\n');
        //Serial.println("Message: " + message);
        msg.Message = message;
        messages.push_back(msg);
      } else {
        break;
      }
    }
    Serial.println("-----------------------------------");
    return messages;
  }
};
