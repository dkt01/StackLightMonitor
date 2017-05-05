#include <EtherCard.h>
#include <IPAddress.h>
#include <EEPROM.h>

#include "StackLight.h"

#define STATIC 0  // set to 1 to disable DHCP (adjust myip/gwip values below)

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

byte Ethernet::buffer[1000]; // tcp/ip send and receive buffer
BufferFiller bfill;

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
    bfill = ether.tcpOffset();
    char* data = (char*) Ethernet::buffer + pos;
    if (strncmp("GET /", data, 5) == 0)
    {
      Serial.println("GET /:");
      Serial.println(data);
      bfill.emit_p(http_OK);
    }
    else if (strncmp("PUT apiURL", data, 10) == 0)
    {
      Serial.println("PUT apiURL:");
      Serial.println(data);
    }
    else
    {
      // Page not found
      Serial.println("???:");
      Serial.println(data);
      bfill.emit_p(http_Unauthorized);
    }

    // Send http response
    ether.httpServerReply(bfill.position());
  }

  // put your main code here, to run repeatedly:
  stackLight.update();
}
