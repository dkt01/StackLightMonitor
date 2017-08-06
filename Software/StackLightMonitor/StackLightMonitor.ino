#include <EtherCard.h>
#include <IPAddress.h>
#include <EEPROM.h>

#include "StackLight.h"
#include "config_html.h"
#include "favicon_ico.h"

#define DNS_RETRY_INTERVAL_MS 5000
#define SERVER_POLL_INTERVAL_MS 10000

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
#define NUMCOLORS 3

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

uint8_t buildStatus;

#define BUILDSTATUS_SUCCESS    0x01
#define BUILDSTATUS_UNSTABLE   0x02
#define BUILDSTATUS_FAILURE    0x04
#define BUILDSTATUS_OTHER      0x08
#define BUILDSTATUS_UNKNOWN    0x10
#define BUILDSTATUS_BUILDING   0x80

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// HTTP Response Headers ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const char http_OK[] PROGMEM =
"HTTP/1.0 200 OK\r\n"
"Content-Type: text/html\r\n"
"Pragma: no-cache\r\n\r\n";

const char http_BadRequest[] PROGMEM =
"HTTP/1.0 400 Bad Request\r\n"
"Content-Type: text/html\r\n\r\n";

const char http_Unauthorized[] PROGMEM =
"HTTP/1.0 401 Unauthorized\r\n"
"Content-Type: text/html\r\n\r\n"
"<h1>401 Unauthorized</h1>";

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// API Query Components /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const char http_Get_Prefix[] PROGMEM =
"GET ";

const char http_Get_Middle[] PROGMEM =
" HTTP/1.0\r\nHost: ";

const char http_Get_Suffix[] PROGMEM =
"\r\nAccept: text/html\r\n\r\n";


////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// EEPROM Access ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

uint16_t GetEEPROMWord(uint16_t address)
{
  uint16_t retval = (EEPROM.read(address) & 0xFF);
  retval |= ((EEPROM.read(address + 1) & 0xFF) << 8);
  return retval;
}

void SetEEPROMWord(uint16_t address, uint16_t val)
{
  EEPROM.write(address, val & 0xFF);
  EEPROM.write(address + 1, (val >> 8) & 0xFF);
}

bool HaveURL()
{
  return GetEEPROMWord(PERSISTENT_MEMORY_DOMAIN_ADDRESS) >= PERSISTENT_MEMORY_DATA_START;
}

bool SaveURL(char* urlString)
{
  Serial.println(F("Saving URL..."));

  // Indices within urlString where newline delimiters are
  uint16_t divs[2];

  // Ensure delimiter at end of domain exists and domain string is longer than
  // four characters.  a.co style should be shortest domain.
  divs[0] = strcspn(urlString, "\n");
  if('\n' != urlString[divs[0]]
     || divs[0] < 4)
  {
    Serial.println(F("Could not find first newline"));
    return false;
  }

  // Ensure delimiter at end of port exists
  divs[1] = strcspn(urlString + divs[0] + 1, "\n");
  divs[1] += (divs[0] + 1);
  if('\n' != urlString[divs[1]])
  {
    Serial.println(F("Could not find second newline"));
    Serial.print(F("First newline at "));
    Serial.println(divs[0],DEC);
    Serial.print(F("Second newline at "));
    Serial.println(divs[1],DEC);
    return false;
  }

  // Ensure port string is at least one digit
  if(divs[1] - divs[0] < 2)
  {
    Serial.println(F("Port not present"));
    return false;
  }

  // Ensure an API endpoint string exists
  if('\0' == urlString[divs[1] + 1])
  {
    Serial.println(F("API endpoint not present"));
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
      Serial.print(F("Invalid character "));
      Serial.println(charVal, DEC);
      return false;
    }
    // Port is decimal ascii
    port *= 10;
    port += charVal;
    ether.hisport = port;
  }

  // Write pointer to domain string
  uint16_t eepromAddress = PERSISTENT_MEMORY_DATA_START;
  SetEEPROMWord(PERSISTENT_MEMORY_DOMAIN_ADDRESS, PERSISTENT_MEMORY_DATA_START);

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
  SetEEPROMWord(PERSISTENT_MEMORY_PORT_ADDRESS, eepromAddress);

  // Write port
  SetEEPROMWord(eepromAddress, port);
  eepromAddress += 2;

  // Write pointer to API endpoint URL
  SetEEPROMWord(PERSISTENT_MEMORY_ENDPOINT_ADDRESS, eepromAddress);

  // Write API endpoint URL
  uint16_t i = divs[1] + 1;
  do
  {
    EEPROM.write(eepromAddress, urlString[i]);
    eepromAddress++;
    i++;
  } while(urlString[i] != '\0');
  // Write null stop character
  EEPROM.write(eepromAddress, '\0');

  // Indicate successful write
  return true;
}

uint16_t LoadURL(char* urlString)
{
  Serial.println(F("Loading URL..."));
  uint16_t totalLength = 0;

  // Check that a URL has been written
  if(!HaveURL())
  {
    Serial.println(F("Don't have URL"));
    urlString[0] = '\0';
    return 0;
  }

  // Output full formatted API URL string
  totalLength += LoadDomain(urlString);
  urlString[totalLength] = ':';
  totalLength++;
  totalLength += LoadPort(urlString + totalLength);
  totalLength += LoadEndpoint(urlString + totalLength);

  return totalLength;
}

uint16_t LoadDomain(char* destString)
{
  uint16_t domainSize = 0;
  uint8_t eepromByte = '\0';
  // Get start address for domain string
  uint16_t eepromAddress = GetEEPROMWord(PERSISTENT_MEMORY_DOMAIN_ADDRESS);

  // If start address is invalid, return empty string
  if(!HaveURL())
  {
    destString[0] = '\0';
    return domainSize;
  }

  // Load null-terminated string from EEPROM
  do
  {
    eepromByte = EEPROM.read(eepromAddress);
    destString[domainSize] = eepromByte;
    eepromAddress++;
    domainSize++;
  } while(eepromByte != '\0');

  // Return size not including null stop character
  return domainSize - 1;
}

uint16_t LoadPort()
{
  // Check if URL data is set
  if(!HaveURL())
  {
    return -1;
  }

  uint16_t port = GetEEPROMWord(GetEEPROMWord(PERSISTENT_MEMORY_PORT_ADDRESS));
  ether.hisport = port;
  return port;
}

uint16_t LoadPort(char* destString)
{
  // Check if URL data is set
  if(!HaveURL())
  {
    return -1;
  }

  uint16_t port = GetEEPROMWord(GetEEPROMWord(PERSISTENT_MEMORY_PORT_ADDRESS));
  return sprintf(destString, "%d", port);
}

uint16_t LoadEndpoint(char* destString)
{
  uint16_t endpointSize = 0;
  uint8_t eepromByte = '\0';
  // Get start address for endpoint string
  uint16_t eepromAddress = GetEEPROMWord(PERSISTENT_MEMORY_ENDPOINT_ADDRESS);

  // If start address is invalid, return empty string
  if(!HaveURL())
  {
    destString[0] = '\0';
    return endpointSize;
  }

  // Load null-terminated string from EEPROM
  do
  {
    eepromByte = EEPROM.read(eepromAddress);
    destString[endpointSize] = eepromByte;
    eepromAddress++;
    endpointSize++;
  } while(eepromByte != '\0');

  // Return size not including null stop character
  return endpointSize - 1;
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// API Query Helpers //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static uint16_t FillServerQuery(uint8_t sessionID)
{
  // Create API query in payload
  uint8_t* startPos = EtherCard::tcpOffset();
  uint16_t len = sizeof(http_Get_Prefix) - 1;
  memcpy_P(startPos, http_Get_Prefix, len);
  len += LoadEndpoint(startPos+len);
  memcpy_P(startPos + len, http_Get_Middle, sizeof(http_Get_Middle));
  len += sizeof(http_Get_Middle) - 1;
  len += LoadDomain(startPos + len);
  memcpy_P(startPos + len, http_Get_Suffix, sizeof(http_Get_Suffix));
  len += sizeof(http_Get_Suffix) - 1;
  return len;
}

static uint8_t ReceiveServerResponse( uint8_t sessionID,
                                      uint8_t flags,
                                      uint16_t offset,
                                      uint16_t length )
{
  char* responseStart = (char*)(Ethernet::buffer + offset);
  Ethernet::buffer[offset+length] = '\0';
  Serial.println(F("Request callback:"));
  // Serial.println(F("Content:"));
  // Serial.println(responseStart);

  // Find beginning of JSON
  size_t jsonStartIdx = strcspn(responseStart, "{");
  responseStart += jsonStartIdx;
  if(*responseStart != '{')
  {
    Serial.println(F("JSON not found!"));
    return 0;
  }

  char* key = NULL;
  uint8_t keyLen = 0;
  char* value = NULL;
  uint8_t valueLen = 0;

  while(getKVPair(responseStart, key, keyLen, value, valueLen))
  {
    if(keyLen > 0 && valueLen > 0 && 0 == strncmp(key,"building",keyLen))
    {
      if(0 == strncmp(value,"true",valueLen))
      {
        // Set building flag
        buildStatus |= BUILDSTATUS_BUILDING;
        Serial.println(F("Building!"));
      }
      else
      {
        // Clear building flag
        uint8_t statusMask = ~BUILDSTATUS_BUILDING;
        buildStatus &= statusMask;
        Serial.println(F("Not Building!"));
      }
    }
    else if(keyLen > 0 && valueLen > 0 && 0 == strncmp(key,"result",keyLen))
    {
      if(0 == strncmp(value,"\"SUCCESS\"",valueLen))
      {
        // Clear all status flags except success and building
        uint8_t statusMask = BUILDSTATUS_BUILDING | BUILDSTATUS_SUCCESS;
        buildStatus &= statusMask;
        // Set success flag
        buildStatus |= BUILDSTATUS_SUCCESS;
        Serial.println(F("Build: SUCCESS"));
        Serial.print(buildStatus);
      }
      else if(0 == strncmp(value,"\"FAILURE\"",valueLen))
      {
        // Clear all status flags except failure and building
        uint8_t statusMask = BUILDSTATUS_BUILDING | BUILDSTATUS_FAILURE;
        buildStatus &= statusMask;
        // Set failure flag
        buildStatus |= BUILDSTATUS_FAILURE;
        Serial.println(F("Build: FAILURE"));
        Serial.print(buildStatus);
      }
      else if(0 == strncmp(value,"\"NOT_BUILT\"",valueLen))
      {
        // Clear all status flags except other and building
        uint8_t statusMask = BUILDSTATUS_BUILDING | BUILDSTATUS_OTHER;
        buildStatus &= statusMask;
        // Set other flag
        buildStatus |= BUILDSTATUS_OTHER;
        Serial.println(F("Build: NOT_BUILT"));
        Serial.print(buildStatus);
      }
      else if(0 == strncmp(value,"\"ABORTED\"",valueLen))
      {
        // Clear all status flags except other and building
        uint8_t statusMask = BUILDSTATUS_BUILDING | BUILDSTATUS_OTHER;
        buildStatus &= statusMask;
        // Set other flag
        buildStatus |= BUILDSTATUS_OTHER;
        Serial.println(F("Build: ABORTED"));
        Serial.print(buildStatus);
      }
      else if(0 == strncmp(value,"\"UNSTABLE\"",valueLen))
      {
        // Clear all status flags except failure and building
        uint8_t statusMask = BUILDSTATUS_BUILDING | BUILDSTATUS_UNSTABLE;
        buildStatus &= statusMask;
        // Set unstable flag
        buildStatus |= BUILDSTATUS_UNSTABLE;
        Serial.println(F("Build: UNSTABLE"));
        Serial.print(buildStatus);
      }
      else
      {
        // Do nothing and maintain previous status
        Serial.println(F("Build: OTHER"));
        Serial.print(buildStatus);
      }
    }
    responseStart = value + valueLen + 1;
  }
  return 0; // Not used for anything
}

bool getKVPair(char* responseStart, char*& key, uint8_t& keyLen, char*& value, uint8_t& valueLen)
{
  // Every key should start with a double quote and end with a double quote
  size_t findOffset = strcspn(responseStart, "\"");
  if(responseStart[findOffset] != '\"')
  {
    // No key found
    return false;
  }
  // Key is always a string, so don't include the first double quote
  key = responseStart + findOffset + 1;
  responseStart = key;

  findOffset = strcspn(responseStart, "\"");
  if(responseStart[findOffset] != '\"')
  {
    // No key end found
    return false;
  }
  keyLen = findOffset;
  responseStart += findOffset;

  // A colon will always appear immediately before the value
  findOffset = strcspn(responseStart, ":");
  if(responseStart[findOffset] != ':')
  {
    // No KV pair separator found
    return false;
  }
  value = responseStart + findOffset + 1;
  responseStart = value;

  // Comma separates keys, end brace indicates end of selection
  findOffset = strcspn(responseStart, ",}");
  if(responseStart[findOffset] != ',' &&
     responseStart[findOffset] != '}')
  {
    // No value end found
    return false;
  }
  valueLen = findOffset;
  return true;
}

void updatePaterns(StackLight& sl, uint8_t status)
{
  uint8_t illuminateColor = 0;
  // While building, pulse light corresponding to previous build status
  if(status & BUILDSTATUS_BUILDING)
  {
    if(status & BUILDSTATUS_SUCCESS)
    {
      illuminateColor = GREEN;
    }
    else if(status & BUILDSTATUS_FAILURE)
    {
      illuminateColor = RED;
    }
    else
    {
      illuminateColor = YELLOW;
    }
    for(uint8_t i = 0; i < NUMCOLORS; i++)
    {
      if(i == illuminateColor)
      {
        sl.setPattern(i, StackLight::PULSE, 255);
      }
      else
      {
        sl.setPattern(i, StackLight::SOLID, 0);
      }
    }
  }
  else
  {
    // Unknown flash all lights
    if(status & BUILDSTATUS_UNKNOWN)
    {
      for(uint8_t i = 0; i < NUMCOLORS; i++)
      {
        sl.setPattern(i, StackLight::FLASH, 255);
      }
    }
    // Solid corresponding to build status
    else
    {
      if(status & BUILDSTATUS_SUCCESS)
      {
        illuminateColor = GREEN;
      }
      else if(status & BUILDSTATUS_FAILURE)
      {
        illuminateColor = RED;
      }
      else
      {
        illuminateColor = YELLOW;
      }
      for(uint8_t i = 0; i < NUMCOLORS; i++)
      {
        if(i == illuminateColor)
        {
          sl.setPattern(i, StackLight::SOLID, 255);
        }
        else
        {
          sl.setPattern(i, StackLight::SOLID, 0);
        }
      }
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Main Functions ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(57600);

  // Initialize to unknown build status
  buildStatus = 0;
  buildStatus |= BUILDSTATUS_UNKNOWN;

  // Check if persistent memory appears to contain data for this application
  if(PERSISTENT_MEMORY_VERSION != EEPROM.read(0))
  {
    SetEEPROMWord(PERSISTENT_MEMORY_DOMAIN_ADDRESS, 0); // Indicates no domain
    EEPROM.write(0, PERSISTENT_MEMORY_VERSION);
  }

  if(0 == ether.begin(sizeof Ethernet::buffer, mymac, 8))
  {
    Serial.println(F("Failed to access Ethernet controller"));
  }
#if STATIC
  ether.staticSetup(myip, gwip);
#else
  if(false == ether.dhcpSetup())
  {
    Serial.println(F("DHCP failed"));
  }
#endif

  ether.printIp(F("IP:  "), ether.myip);
  ether.printIp(F("GW:  "), ether.gwip);
  ether.printIp(F("DNS: "), ether.dnsip);

  stackLight.setPattern(RED,
                        StackLight::PULSE,
                        255,
                        1000);
  stackLight.setPattern(YELLOW,
                        StackLight::SOLID,
                        0);
  stackLight.setPattern(GREEN,
                        StackLight::SOLID,
                        0);
}

void loop()
{
  static bool pendingPut = false;
  bool generalMemCopy = true;

  static bool haveDNS = false;
  static long lastDNSLookup = 0;
  static long lastServerPoll = 0;

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
      pendingPut = false;

      Serial.println(F("GET /:"));
      Serial.println(data);
      sendData = const_cast<char*>(config_html);
      if(sizeof(config_html) < sz)
      {
        sz = sizeof(config_html);
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
      pendingPut = false;

      Serial.println(F("GET /favicon.ico:"));
      Serial.println(data);
      sendData = const_cast<char*>(favicon_ico);
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
      pendingPut = false;

      Serial.println(F("GET /apiURL:"));
      Serial.println(data);
      generalMemCopy = false; // Doing memcopy here to build response
      sz = sizeof(http_OK);
      memcpy_P(data, http_OK, sz);
      uint16_t urlSize = 0;
      urlSize = LoadURL(data+sz-1); // Start at null character from header string
      sz += (urlSize-1); // Don't send null characters
    }
    else if (strncmp("PUT /apiURL", data, 10) == 0)
    {
      pendingPut = false;

      Serial.println(F("PUT /apiURL:"));
      Serial.println(data);
      char* headerEnd = strstr(data, "\r\n\r\n");
      Serial.print(F("After Header: "));
      Serial.println(headerEnd + 4);
      if(SaveURL(headerEnd + 4))
      {
        sendData = const_cast<char*>(http_OK);
        sz = sizeof(http_OK);
        haveDNS = false;
        Serial.println(F("Pending DNS"));
        stackLight.setPattern(YELLOW,
                              StackLight::PULSE,
                              255,
                              1000);
        stackLight.setPattern(RED,
                              StackLight::SOLID,
                              0);
        stackLight.setPattern(GREEN,
                              StackLight::SOLID,
                              0);
      }
      else if('\0' == *(headerEnd + 4))
      {
        // 2-part put message
        Serial.println(F("Pending PUT"));
        sendData = const_cast<char*>(http_OK);
        sz = sizeof(http_OK);
        pendingPut = true;
      }
      else
      {
        sendData = const_cast<char*>(http_BadRequest);
        sz = sizeof(http_BadRequest);
      }
    }
    else if(true == pendingPut)
    {
      pendingPut = false;

      Serial.println(F("PUT pt2"));
      Serial.println(data);
      if(SaveURL(data))
      {
        sendData = const_cast<char*>(http_OK);
        sz = sizeof(http_OK);
        haveDNS = false;
        Serial.println(F("Pending DNS"));
        stackLight.setPattern(YELLOW,
                              StackLight::PULSE,
                              255,
                              1000);
        stackLight.setPattern(RED,
                              StackLight::SOLID,
                              0);
        stackLight.setPattern(GREEN,
                              StackLight::SOLID,
                              0);
      }
      else
      {
        Serial.println(F("Not PUT pt2!"));
        sendData = const_cast<char*>(http_Unauthorized);
        if(sizeof(http_Unauthorized) < sz)
        {
          sz = sizeof(http_Unauthorized);
        }
      }
    }
    else
    {
      // Page not found
      Serial.println(F("???:"));
      Serial.println(data);
      sendData = const_cast<char*>(http_Unauthorized);
      if(sizeof(http_Unauthorized) < sz)
      {
        sz = sizeof(http_Unauthorized);
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

  if( !haveDNS
      && HaveURL()
      && (millis() - lastDNSLookup) > DNS_RETRY_INTERVAL_MS
      && ether.isLinkUp() )
  {
    lastDNSLookup = millis();
    LoadDomain(Ethernet::buffer+60);
    if(!ether.dnsLookup(Ethernet::buffer+60,true))
    {
      Serial.println(F("DNS lookup failed"));
    }
    else
    {
      ether.printIp(F("Server: "), ether.hisip);
      haveDNS = true;
      stackLight.setPattern(GREEN,
                            StackLight::PULSE,
                            255,
                            1000);
      stackLight.setPattern(YELLOW,
                            StackLight::SOLID,
                            0);
      stackLight.setPattern(RED,
                            StackLight::SOLID,
                            0);
    }
  }

  if( haveDNS
      && HaveURL()
      && (millis() - lastServerPoll) > SERVER_POLL_INTERVAL_MS )
  {
    ether.clientTcpReq(ReceiveServerResponse, FillServerQuery, LoadPort());
    updatePaterns(stackLight, buildStatus);
    lastServerPoll = millis();
  }

  // put your main code here, to run repeatedly:
  stackLight.update();
}
