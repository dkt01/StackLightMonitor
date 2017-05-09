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

#define PERSISTENT_MEMORY_VERSION 2
#define PERSISTENT_MEMORY_DOMAIN_ADDRESS 2
#define PERSISTENT_MEMORY_PORT_ADDRESS 4
#define PERSISTENT_MEMORY_ENDPOINT_ADDRESS 6
#define PERSISTENT_MEMORY_DATA_START 8

const uint8_t modulePins[3] = {RPIN, YPIN, GPIN};
StackLight stackLight = StackLight(3, modulePins);

const char http_OK[] PROGMEM =
"HTTP/1.0 200 OK\r\n"
"Content-Type: text/html\r\n"
"Pragma: no-cache\r\n\r\n";

const char http_BadRequest[] PROGMEM =
"HTTP/1.0 400 Bad Request\r\n"
"Content-Type: text/html\r\n\r\n"

const char http_Unauthorized[] PROGMEM =
"HTTP/1.0 401 Unauthorized\r\n"
"Content-Type: text/html\r\n\r\n"
"<h1>401 Unauthorized</h1>";

bool SaveURL(char* urlString)
{
  Serial.println("Saving URL...");

  // Indices within urlString where newline delimiters are
  uint16_t divs[2];

  // Ensure delimiter at end of domain exists and domain string is longer than
  // four characters.  a.co style should be shortest domain.
  divs[0] = strcspn(urlString, "\n");
  if('\n' != urlString[divs[0]]
     || divs[0] < 4)
  {
    return false;
  }

  // Ensure delimiter at end of port exists
  divs[1] = strcspn(urlString + divs[0] + 1);
  if('\n' != urlString[divs[1]])
  {
    return false;
  }

  // Ensure port string is at least one digit
  if(divs[1] - divs[0] < 2)
  {
    return false;
  }

  // Ensure an API endpoint string exists
  if('\0' == urlString[divs[1] + 1])
  {
    return false;
  }

  // Extract and validate port
  uint16_t port = 0;
  uint8_t charVal = 0;
  for(uint16_t i = divs[0] + 1; i < divs[1]; i++)
  {
    // Validate character in range '0'-'9'
    charVal = urlString[i] - 48;
    if(charVal > 9)
    {
      return false;
    }
    // Port is decimal ascii
    port *= 10;
    port += charVal;
  }

  // Write pointer to domain string
  uint16_t eepromAddress = PERSISTENT_MEMORY_DATA_START;
  EEPROM.write(PERSISTENT_MEMORY_DOMAIN_ADDRESS, PERSISTENT_MEMORY_DATA_START & 0xFF);
  EEPROM.write(PERSISTENT_MEMORY_DOMAIN_ADDRESS + 1,
               (PERSISTENT_MEMORY_DATA_START >> 8) & 0xFF);
  // Write domain string
  for(uint16_t i = 0; i < divs[0]; i++)
  {
    EEPROM.write(eepromAddress, urlString[i]);
    eepromAddress++;
  }
  // Write null stop character
  EEPROM.write(eepromAddress, '\0');
  eepromAddress++;

  // Write pointer to port
  EEPROM.write(PERSISTENT_MEMORY_PORT_ADDRESS, eepromAddress & 0xFF);
  EEPROM.write(PERSISTENT_MEMORY_PORT_ADDRESS + 1,
               (eepromAddress >> 8) & 0xFF);
  // Write port
  EEPROM.write(eepromAddress, port & 0xFF);
  eepromAddress++;
  EEPROM.write(eepromAddress, (port >> 8) & 0xFF);
  eepromAddress++;

  // Write pointer to API endpoint URL
  EEPROM.write(PERSISTENT_MEMORY_ENDPOINT_ADDRESS, eepromAddress & 0xFF);
  EEPROM.write(PERSISTENT_MEMORY_ENDPOINT_ADDRESS + 1,
               (eepromAddress >> 8) & 0xFF);
  // Write API endpoint URL
  uint16_t i = divs[1] + 1;
  do
  {
    EEPROM.write(eepromAddress, urlString[i]);
    eepromAddress++;
    i++;
  } while(urlString[i] != '\0')
  // Write null stop character
  EEPROM.write(eepromAddress, '\0')

  // Indicate successful write
  return true;
}

uint16_t LoadURL(char* urlString)
{
  Serial.println("Loading URL...");
  char eepromByte;
  uint16_t eepromAddress;

  // Check that a URL has been written
  eepromAddress = EEPROM.read(PERSISTENT_MEMORY_DOMAIN_ADDRESS);
  eepromAddress |= ((EEPROM.read(PERSISTENT_MEMORY_DOMAIN_ADDRESS) & 0xFF) << 8);

  // Invalid or no address, return empty string
  if(eepromAddress < PERSISTENT_MEMORY_DATA_START)
  {
    urlString[0] = '\0';
    return 0;
  }

  //TODO: finish this for port and endpoint


  // Start at address 1 because 0 is a version identifier
  // Copy EEPROM URL to provided buffer.  Null stop character will be added to end
  do
  {
    eepromByte = EEPROM.read(eepromAddress);
    Serial.print("\t");
    Serial.print(eepromAddress,HEX);
    Serial.print(":");
    Serial.println(eepromByte,HEX);
    *urlString = eepromByte;
    eepromAddress++;
    urlString++;
  } while(eepromByte != '\0');

  return eepromAddress - 1;
}

void setup()
{
  Serial.begin(57600);

  // Check if persistent memory appears to contain data for this application
  if(PERSISTENT_MEMORY_VERSION != EEPROM.read(0))
  {
    EEPROM.write(PERSISTENT_MEMORY_DOMAIN_ADDRESS, 0); // Indicates empty string
    EEPROM.write(PERSISTENT_MEMORY_DOMAIN_ADDRESS+1, 0); // Indicates empty string
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
  static bool pendingPut = false;
  bool generalMemCopy = true;

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
      generalMemCopy = false; // Doing memcopy here to build response
      sz = sizeof(http_OK);
      memcpy_P(data, http_OK, sz);
      uint16_t urlSize = LoadURL(data+sz-1); // Start at null character from header string
      sz += (urlSize - 2); // Don't send null characters
    }
    else if (strncmp("PUT /apiURL", data, 10) == 0)
    {
      Serial.println("PUT /apiURL:");
      Serial.println(data);
      sendData = http_OK;
      if(sizeof(http_OK) < sz)
      {
        sz = sizeof(http_OK);
        complete = true;
        pendingPut = true;
      }
    }
    else
    {
      if(pendingPut)
      {
        Serial.println("PUT data:");
        Serial.println(data);
        if(SaveURL(data))
        {
          sendData = http_OK;
          sz = sizeof(http_OK);
        }
        else
        {
          sendData = http_BadRequest;
          sz = sizeof(http_BadRequest);
        }
        complete = true;
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
    }

    // Send http response
    if(generalMemCopy)
    {
      memcpy_P(data, sendData, sz); // Copy data from flash to RAM
    }
    // ether.httpServerReply(sz-1);
    ether.httpServerReply_with_flags(sz,TCP_FLAGS_ACK_V|TCP_FLAGS_PUSH_V);
    ether.httpServerReply_with_flags(0,TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
    startPoint = 0;
  }

  // put your main code here, to run repeatedly:
  stackLight.update();
}
