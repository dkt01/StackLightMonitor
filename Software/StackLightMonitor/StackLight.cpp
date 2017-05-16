#include "StackLight.h"

StackLight::StackLight(const uint8_t nModules, const uint8_t* modulePins):
  m_numModules(nModules)
{
  // Allocate dynamic memory
  m_modulePins =       new uint8_t [m_numModules];
  m_modulePatterns =   new Pattern [m_numModules];
  m_moduleBrightness = new uint8_t [m_numModules];
  m_modulePeriods =    new uint16_t [m_numModules];
  m_moduleParams =     new uint8_t [m_numModules];

  // Initialize dynamic memory
  for(uint8_t i = 0; i < m_numModules; i++)
  {
    m_modulePins[i] =       modulePins[i];
    m_modulePatterns[i] =   SOLID;
    m_moduleBrightness[i] = 0;
    m_modulePeriods[i] =    0;
    m_moduleParams[i] =     0;

    // Initialize pin modes
    pinMode(m_modulePins[i], OUTPUT);
    analogWrite(modulePins[i], 64);
  }

  // Populate Gamma correction lookup table
  // Gamma correction for x is:
  // ( x / MAXVAL )^2.5 * MAXVAL
  for(int i = 0; i < 256; i++)
  {
    float x = i;
    x /= 255;
    x = pow(x, 2.5);
    x *= 255;

    GAMMA[i] = x;
  }

  // Start test pattern at initialization
  m_test = -1;
}


StackLight::~StackLight()
{
  // Deallocate dynamic memory
  if(NULL != m_modulePins)
  {
    delete [] m_modulePins;
  }
  if(NULL != m_modulePatterns)
  {
    delete [] m_modulePatterns;
  }
  if(NULL != m_moduleBrightness)
  {
    delete [] m_moduleBrightness;
  }
  if(NULL != m_modulePeriods)
  {
    delete [] m_modulePeriods;
  }
  if(NULL != m_moduleParams)
  {
    delete [] m_moduleParams;
  }
}


void StackLight::update()
{
  m_now = millis();
  // Start test pattern on first update call
  if(m_test == -1)
  {
    m_test = m_now;
  }
  if(m_test > 0)
  {
    // Run test sequence
    SetTest();
  }
  else
  {
    // Run set patterns
    for(uint8_t i = 0; i < m_numModules; i++)
    {
      switch(m_modulePatterns[i])
      {
        case SOLID:
          SetSolid(m_modulePins[i], m_moduleBrightness[i]);
          break;
        case FLASH:
          SetFlash(m_modulePins[i], m_moduleBrightness[i], m_modulePeriods[i]);
          break;
        case PULSE:
          SetPulse(m_modulePins[i], m_moduleBrightness[i], m_modulePeriods[i]);
          break;
        default:
          SetSolid(m_modulePins[i], 0);
      }
    }
  }
}


void StackLight::setPattern(uint8_t moduleNum,
                            Pattern p,
                            uint8_t brightness,
                            uint16_t period,
                            uint8_t param)
{
  if(moduleNum < m_numModules)
  {
    m_modulePatterns[moduleNum]   = p;
    m_moduleBrightness[moduleNum] = brightness;
    m_modulePeriods[moduleNum]    = period;
    m_moduleParams[moduleNum]     = param;
  }
}


void StackLight::setBrightness(uint8_t moduleNum,
                               uint8_t brightness)
{
  if(moduleNum < m_numModules)
  {
    m_moduleBrightness[moduleNum] = brightness;
  }
}


void StackLight::setPeriod(uint8_t moduleNum,
                           uint16_t period)
{
  if(moduleNum < m_numModules)
  {
    m_modulePeriods[moduleNum] = period;
  }
}


void StackLight::setParam(uint8_t moduleNum,
                          uint8_t param)
{
  if(moduleNum < m_numModules)
  {
    m_moduleParams[moduleNum] = param;
  }
}


void StackLight::SetSolid(uint8_t pinNum, uint8_t brightness)
{
  analogWrite(pinNum, GAMMA[brightness]);
}


void StackLight::SetFlash(uint8_t pinNum, uint8_t brightness, uint16_t period)
{
  period /= PERIODDIVISOR;
  unsigned long condensedNow = m_now / PERIODDIVISOR;
  bool invert = (condensedNow % (2 * period)) !=
                (condensedNow % period);

  // Turn on for period ms then off for period ms
  analogWrite(pinNum,
              invert ? GAMMA[0] : GAMMA[brightness]);
}


void StackLight::SetPulse(uint8_t pinNum, uint8_t brightness, uint16_t period)
{
  analogWrite(pinNum,
              GAMMA[CalcPulseBrightness(brightness, period, true)]);
}

void StackLight::SetTest()
{
  // Get time since test sequence started
  unsigned long refTime = m_now - m_test;
  // Total test sequence time is dependent on number of modules.
  // If no modules are active, skip test sequence
  uint16_t maxTime = ( m_numModules > 0 ? (((m_numModules - 1) * (TESTPERIOD)) + (TESTPERIOD * 2))
                                        : 0 );
  if(refTime > maxTime)
  {
    m_test = 0;
  }
  else
  {
    uint8_t sequenceNum = refTime / TESTPERIOD;
    uint16_t phase = (refTime % TESTPERIOD);
    uint8_t curValue = ((float)phase / float(TESTPERIOD)) * 255;

    for(uint8_t i = 0; i < m_numModules; i++)
    {
      if(i == sequenceNum)
      {
        analogWrite(m_modulePins[i], GAMMA[curValue]);
      }
      else if(i == sequenceNum - 1)
      {
        analogWrite(m_modulePins[i], GAMMA[255 - curValue]);
      }
      else
      {
        analogWrite(m_modulePins[i], 0);
      }
    }
  }
}


uint8_t StackLight::CalcPulseBrightness(uint8_t brightness, uint16_t period, bool smooth,
                                        uint8_t offBrightness, uint16_t offset)
{
  bool invert = false;
  period /= PERIODDIVISOR;
  offset /= PERIODDIVISOR;
  unsigned long condensedNow = m_now / PERIODDIVISOR;
  if(smooth)
  {
    invert = ((condensedNow + offset) % (2 * period)) !=
             ((condensedNow + offset) % period);
  }
  uint16_t curPhase = ((condensedNow + offset) % period);
  if(invert)
  {
    curPhase = period - curPhase;
  }
  uint8_t brightnessRange = brightness - offBrightness;
  uint32_t outBrightness = curPhase * brightnessRange;
  outBrightness /= period;
  outBrightness += offBrightness;
  if(outBrightness > brightness)
  {
    outBrightness = brightness;
  }
  return static_cast<uint8_t>(outBrightness);
}

