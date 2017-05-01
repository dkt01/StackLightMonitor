#include <Arduino.h>

// Divisor to prevent overflows in period calculations
#define PERIODDIVISOR 16

// Milliseconds for each test pattern fade
#define TESTPERIOD 2000

class StackLight
{
  public:
    enum Pattern
    {
      SOLID = 0,
      FLASH,
      PULSE
    };

    // Constructors/destructors
    StackLight(const uint8_t nModules, const uint8_t* modulePins);
    ~StackLight();

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Updates all LEDs in accordance with the set parameters for each
    ///        module.  All patterns are time-based, so the rate at which update()
    ///        is called will not change the speed of patterns.
    ////////////////////////////////////////////////////////////////////////////
    void update();

    // Pattern control

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Updates the displayed pattern for a given module.  These changes
    ///        will be reflected on the next call to update().
    /// @param moduleNum Index of the module to update parameters.  If index is
    ///                  invalid, no update will occur.
    /// @param p Pattern to display on the module.
    /// @param brightness Max brightness to use when generating pattern.  This
    ///                   may not be used for all patterns.
    /// @param period Length of time for one complete pattern display loop in
    ///               milliseconds.  This may not be used for all patterns.
    /// @param param Miscellaneous parameter for pattern.  This may not be used
    ///              for all patterns.
    ////////////////////////////////////////////////////////////////////////////
    void setPattern(uint8_t moduleNum,
                    Pattern p,
                    uint8_t brightness = 0,
                    uint16_t period = 2000, // 0.5 Hz
                    uint8_t param = 0);

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Changes only the max brightness of one module.  These changes
    ///        will be reflected on the next call to update().
    /// @param moduleNum Index of the module to update parameters.  If index is
    ///                  invalid, no update will occur.
    /// @param brightness Max brightness to use when generating pattern.  This
    ///                   may not be used for all patterns.
    ////////////////////////////////////////////////////////////////////////////
    void setBrightness(uint8_t moduleNum,
                       uint8_t brightness);

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Changes only the period of one module.  These changes will be
    ///        reflected on the next call to update().
    /// @param moduleNum Index of the module to update parameters.  If index is
    ///                  invalid, no update will occur.
    /// @param period Length of time for one complete pattern display loop in
    ///               milliseconds.  This may not be used for all patterns.
    ////////////////////////////////////////////////////////////////////////////
    void setPeriod(uint8_t moduleNum,
                   uint16_t period);

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Changes only the miscellaneous parameter for one module.  These
    ///        changes will be reflected on the next call to update().
    /// @param moduleNum Index of the module to update parameters.  If index is
    ///                  invalid, no update will occur.
    /// @param param Miscellaneous parameter for pattern.  This may not be used
    ///              for all patterns.
    ////////////////////////////////////////////////////////////////////////////
    void setParam(uint8_t moduleNum,
                  uint8_t param);

  private:
    ////////////////////////////////////////////////////////////////////////////
    /// @brief Sets a module to a constant brightness.
    /// @param pinNum Pin number to set output
    /// @param brightness Brightness to set all pixels to.  This value will be
    ///                   gamma corrected before displaying.
    ////////////////////////////////////////////////////////////////////////////
    void SetSolid(uint8_t pinNum, uint8_t brightness);

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Sets a module to flash a given brightness and turn off.
    /// @param pinNum Pin number to set output
    /// @param brightness Brightness to flash module.  This value will be
    ///                   gamma corrected before displaying.
    /// @param period Milliseconds from off to the given color.  Full flash
    ///               period is actually double this value.
    ////////////////////////////////////////////////////////////////////////////
    void SetFlash(uint8_t pinNum, uint8_t brightness, uint16_t period);

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Sets a module to fade in to a given brightness and out to off.
    /// @param pinNum Pin number to set output
    /// @param brightness Brightness to fade module to.  This value will be
    ///                   gamma corrected before displaying.
    /// @param period Milliseconds from off to the given color.  Full pulse
    ///               period is actually double this value.
    ////////////////////////////////////////////////////////////////////////////
    void SetPulse(uint8_t pinNum, uint8_t brightness, uint16_t period);

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Runs a test pattern through all modules
    ////////////////////////////////////////////////////////////////////////////
    void SetTest();

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Calculate a brightness to use in the pulse sequence.  The pulse
    ///        is a linear fade between two brightness values.
    /// @param brightness Brightness used as the 'on' level in the fade.
    /// @param period Milliseconds from 'on' to 'off' level.  Full sequence may
    ///               be double this value if smooth is set to true.
    /// @param smooth When true, there is first a linear fade from 'off' to 'on'
    ///               then a linear fade from 'on' to 'off'.  When false, there
    ///               is a linear fade from 'off' to 'on', then this repeats
    ///               without a transition.
    /// @param offBrightness Brightness used as the 'off' level in the fade.
    /// @param offset Time offset in milliseconds for the pattern.  Each pattern
    ///               starts when
    ///               ((ms since startup) + offset) % period == 0
    /// @return Brightness value matching parameters and current system time
    ////////////////////////////////////////////////////////////////////////////
    uint8_t CalcPulseBrightness(uint8_t brightness, uint16_t period, bool smooth = false,
                                uint8_t offBrightness = 0, uint16_t offset = 0);

    const uint8_t   m_numModules;
    uint8_t*        m_modulePins;
    Pattern*        m_modulePatterns;
    uint8_t*        m_moduleBrightness;
    uint16_t*       m_modulePeriods;
    uint8_t*        m_moduleParams;

    unsigned long m_now;  ///< Millisecond value for patterns
    unsigned long m_test; ///< Millisecond start time for test pattern generation

    uint8_t GAMMA[256]; ///< This is used to transform raw brightnesses to gamma-corrected
                        ///  brightnesses to compensate for brightness perception
};

