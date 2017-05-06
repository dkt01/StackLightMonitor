#include <EtherCard.h>
#include <IPAddress.h>
#include <EEPROM.h>

#include "StackLight.h"
#include "config_html.h"
#include "favicon_ico.h"

#define STATIC 0  // set to 1 to disable DHCP (adjust myip/gwip values below)
#define BUFFERSIZE 1024

#if STATIC
// ethernet interface ip address
static byte myip[] = { 
  192,168,1,253 };
// gateway ip address
static byte gwip[] = { 
  192,168,1,254 };
#endif

// ethernet mac address - must be unique on your network
static byte mymac[] = { 0x44,0x4b,0x54,0x53,0x4c,0x4d };

byte Ethernet::buffer[BUFFERSIZE]; // tcp/ip send and receive buffer

#define RED    0
#define YELLOW 1
#define GREEN  2

#define RPIN 9
#define YPIN 6
#define GPIN 10

#define PERSISTENT_MEMORY_VERSION 1

const uint8_t modulePins[3] = {RPIN, YPIN, GPIN};
StackLight stackLight = StackLight(3, modulePins);

const char http_OK[] PROGMEM =
"HTTP/1.0 200 OK\r\n"
"Content-Type: text/html\r\n"
"Pragma: no-cache\r\n\r\n";

const char http_Unauthorized[] PROGMEM =
"HTTP/1.0 401 Unauthorized\r\n"
"Content-Type: text/html\r\n\r\n"
"<h1>401 Unauthorized</h1>";

void setup()
{
  Serial.begin(57600);

  // Check if persistent memory appears to contain data for this application
  if(PERSISTENT_MEMORY_VERSION != EEPROM.read(0))
  {
    EEPROM.write(1, 0); // Indicates empty string
    EEPROM.write(0, PERSISTENT_MEMORY_VERSION);
  }

  if(0 == ether.begin(sizeof Ethernet::buffer, mymac, 8))
  {
    Serial.println("Failed to access Ethernet controller");
  }
#if STATIC
  ether.staticSetup(myip, gwip);
#else
  if(false == ether.dhcpSetup())
  {
    Serial.println("DHCP failed");
  }
#endif

  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);
  ether.printIp("DNS: ", ether.dnsip);

  stackLight.setPattern(RED,
                        StackLight::PULSE,
                        255,
                        1000);
  stackLight.setPattern(YELLOW,
                        StackLight::PULSE,
                        255,
                        2000);
  stackLight.setPattern(GREEN,
                        StackLight::PULSE,
                        255,
                        4000);
}

void loop()
{
  word pos = ether.packetLoop(ether.packetReceive());

  if (pos)
  {
    uint16_t startPoint = 0; // Multi-packet response
    int sz = BUFFERSIZE - pos;
    char* data = (char*) Ethernet::buffer + pos;
    char* sendData = NULL;
    bool complete = false;

    ether.httpServerReplyAck();

    if (strncmp("GET / ", data, 6) == 0)
    {
      Serial.println("GET /:");
      Serial.println(data);
      sendData = config_html;
      if(sizeof(config_html) < sz)
      {
        sz = sizeof(config_html);
        complete = true;
      }
      else
      {
        do
        {
          memcpy_P(data, sendData + startPoint, sz); // Copy data from flash to RAM
          ether.httpServerReply_with_flags(sz, TCP_FLAGS_ACK_V);
          startPoint += sz;
          sz = BUFFERSIZE - pos;
          if(sizeof(config_html) - startPoint < sz)
          {
            sz = sizeof(config_html) - startPoint;
            complete = true;
          }
        } while (false == complete);
        sendData += startPoint;
      }
    }
    else if (strncmp("GET /favicon.ico", data, 16) == 0)
    {
      Serial.println("GET /favicon.ico:");
      Serial.println(data);
      sendData = favicon_ico;
      if(sizeof(favicon_ico) < sz)
      {
        sz = sizeof(favicon_ico);
        complete = true;
      }
      else
      {
        do
        {
          memcpy_P(data, sendData + startPoint, sz); // Copy data from flash to RAM
          ether.httpServerReply_with_flags(sz, TCP_FLAGS_ACK_V);
          startPoint += sz;
          sz = BUFFERSIZE - pos;
          if(sizeof(favicon_ico) - startPoint < sz)
          {
            sz = sizeof(favicon_ico) - startPoint;
            complete = true;
          }
        } while (false == complete);
        sendData += startPoint;
      }
    }
    else if (strncmp("GET /apiURL", data, 10) == 0)
    {
      Serial.println("GET /apiURL:");
      Serial.println(data);
      sz = sizeof(http_OK);
      sendData = http_OK;
    else if (strncmp("PUT /apiURL", data, 10) == 0)
    {
      Serial.println("PUT /apiURL:");
      Serial.println(data);
      sendData = http_OK;
      if(sizeof(http_OK) < sz)
      {
        sz = sizeof(http_OK);
        complete = true;
      }
    }
    else
    {
      // Page not found
      Serial.println("???:");
      Serial.println(data);
      sendData = http_Unauthorized;
      if(sizeof(http_Unauthorized) < sz)
      {
        sz = sizeof(http_Unauthorized);
        complete = true;
      }
    }

    // Send http response
    memcpy_P(data, sendData, sz); // Copy data from flash to RAM
    // ether.httpServerReply(sz-1);
    ether.httpServerReply_with_flags(sz,TCP_FLAGS_ACK_V|TCP_FLAGS_PUSH_V);
    ether.httpServerReply_with_flags(0,TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
    startPoint = 0;
  }

  // put your main code here, to run repeatedly:
  stackLight.update();
}
