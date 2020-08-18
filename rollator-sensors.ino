
#define _DEBUG
#include <YogiDebug.h>

// Control the macros below to enable which relay is used.
//#define RELAY_HFD2
#define RELAY_SONGLE
#include <YogiRelay.h>

#include <YogiDelay.h>
#include <YogiPitches.h>
#include <YogiSleep.h>
#include <YogiSonic.h>

#include <ADXL345_setup.h>  // includes SparkFun ADXL345 Library
#include <WatchDog.h>

#ifndef __asm__
#    ifdef _MSC_VER
#        define __asm__ __asm
#    endif
#endif

// uncomment following line to build in the logic that
// attempts to cancel out the affect of the vibrators
// on the motion sensor.
//#define ANTIVIBE


const uint8_t k_pinINT = 2;  // INT0
const uint8_t k_pinRelayOn = 3;
const uint8_t k_pinRelayOff = 4;


const uint8_t kPinVibeLeft = 5;   // PWM
const uint8_t kPinVibeRight = 6;  // PWM


const uint8_t kPinSonicEchoLeft = 7;
const uint8_t kPinSonicTrigLeft = 7;

const uint8_t kPinSonicEchoFront = 8;
const uint8_t kPinSonicTrigFront = 8;

const uint8_t kPinSonicEchoRight = 9;
const uint8_t kPinSonicTrigRight = 9;


const uint8_t kPinPotDist = A0;
const uint8_t kPinSDA = A4;
const uint8_t kPinSCL = A5;


// distance below which we ignore
const unsigned int k_minDistance = 6;  // cm


typedef enum orientation_t
        : uint8_t
{
    OR_UNKNOWN,
    OR_VERTICAL,
    OR_HORIZONTAL
} orientation_t;

orientation_t g_eOrientation = OR_UNKNOWN;

bool g_bActiveLaydown = false;


unsigned long g_uTimeCurrent = 0;
unsigned long g_uTimePrevious = 0;
unsigned long g_uTimeInterrupt = 0;
unsigned long g_uTimeLaying = 0;

const uint8_t       k_uAdxlDelaySleep = 45;
const unsigned long k_uDelaySleep = 600 * k_uAdxlDelaySleep;


YogiDelay g_tSonicDelay;
int       g_nSonicCycle = 0;
long      g_nSonicCount = 0;


YogiSleep g_tSleep;


//============== RELAY ===============

YogiRelay g_tRelay( k_pinRelayOn, k_pinRelayOff );


//================ ADXL =================

void
adxlAttachInterrupt()
{
    pinMode( k_pinINT, INPUT );
    attachInterrupt(
            digitalPinToInterrupt( k_pinINT ), adxlIntHandler, RISING );
}

void
adxlDetachInterrupt()
{
    detachInterrupt( digitalPinToInterrupt( k_pinINT ) );
}


void
adxlDrowsy()
{
    g_uCountInterrupt = 0;
    adxl.setInterruptMask( 0 );
    adxl.getInterruptSource();  // clear mInterrupts

    adxl.setInterruptMask( k_maskActivity );
    adxl.setLowPower( true );
}


void
adxlSleep()
{
    adxlDetachInterrupt();
    g_uCountInterrupt = 0;
    adxl.getInterruptSource();  // clear mInterrupts

    adxl.setInterruptMask( 0 );
    adxl.setLowPower( true );
}


void
adxlWakeup()
{
    adxl.setLowPower( false );
    adxl.setInterruptMask( k_maskAll );
    g_uCountInterrupt = 0;
    adxl.getInterruptSource();
    adxlAttachInterrupt();
    adxlIntHandler();
    g_nActivity = 0;
}


//================= WATCHDOG =================

volatile bool g_bWatchDogInterrupt = false;

void
watchdogIntHandler()
{
    g_bWatchDogInterrupt = true;
}


void
watchdogSleep()
{
    DEBUG_PRINTLN( "Watchdog Sleep" );
    DEBUG_DELAY( 300 );
    g_tRelay.reset();  // power down sensors
    adxlSleep();
    WatchDog::setPeriod( OVF_4000MS );
    WatchDog::start();
    g_tSleep.powerDown();
}


void
watchdogWakeup()
{
    WatchDog::stop();
    adxlWakeup();
    g_tRelay.set();
    g_uTimeInterrupt = millis();
    DEBUG_PRINTLN( "Watchdog Wakeup" );
}


void
watchdogHandler()
{
    if ( isHorizontal() )
    {
        // go back to sleep
        g_eOrientation = OR_HORIZONTAL;
        adxlSleep();
        WatchDog::start();
        g_tSleep.sleep();
    }
    else
    {
        // cancel watchdog
        watchdogWakeup();

        g_bActiveLaydown = false;
        g_eOrientation = OR_VERTICAL;
    }
}


//=========== VIBE ===========

typedef struct VibeItem
{
    int  nDuration;
    bool bOn;
} VibeItem;

// following arrays contain millisecond times for on..off
const VibeItem g_aVibeFront[] = { { 1000, true }, { 1000, false } };
const VibeItem g_aVibeSide[] = { { 200, true }, { 500, false } };
const VibeItem g_aVibeBoth[] = { { 1000, true }, { 500, false }, { 200, true },
    { 500, false }, { 200, true }, { 500, false } };

const int g_nVibeCountFront
        = sizeof( g_aVibeFront ) / sizeof( g_aVibeFront[0] );
const int g_nVibeCountSide = sizeof( g_aVibeSide ) / sizeof( g_aVibeSide[0] );
const int g_nVibeCountBoth = sizeof( g_aVibeBoth ) / sizeof( g_aVibeBoth[0] );


class VibeControl
{
public:
    VibeControl( uint8_t pin )
            : m_pin( pin )
            , m_val( 0 )
            , m_buzzing( false )
            , m_tDelay()
            , m_aVibeList( nullptr )
            , m_nCountVibe( 0 )
    {}

public:
    void
    init()
    {
        pinMode( m_pin, OUTPUT );
        analogWrite( m_pin, 0 );
        m_val = 0;
        m_buzzing = false;
        m_tDelay.init( 500 );
    }


    void
    reset()
    {
        pinMode( m_pin, OUTPUT );
        analogWrite( m_pin, 0 );
        m_val = 0;
        m_buzzing = false;
        m_tDelay.reset();
    }

    void
    setPattern( int nVibeCount, const VibeItem* pVibeList )
    {
        if ( pVibeList != m_aVibeList || nVibeCount != m_nCountVibe )
        {
            m_aVibeList = pVibeList;
            m_nCountVibe = nVibeCount;
            m_index = 0;
            m_tDelay.reset();
        }
    }

    void
    on( uint8_t value )
    {
        if ( m_val != value )
        {
            m_val = value;
        }
    }

    void
    cycle( unsigned long nTimeCurrent = 0 )
    {
        if ( 0 < m_val )
        {
            if ( m_tDelay.timesUp( nTimeCurrent ) )
            {
                if ( m_aVibeList[m_index].bOn )
                {
                    m_buzzing = true;
                    analogWrite( m_pin, m_val );
                }
                else
                {
                    m_buzzing = false;
                    analogWrite( m_pin, 0 );
                }
                m_tDelay.newDelay( m_aVibeList[m_index].nDuration );
                ++m_index;
                if ( m_nCountVibe <= m_index )
                    m_index = 0;
            }
        }
    }

    void
    off()
    {
        if ( 0 < m_val )
        {
            m_val = 0;
            m_index = 0;
            m_buzzing = false;
            m_aVibeList = nullptr;
            m_nCountVibe = 0;
            analogWrite( m_pin, m_val );
        }
    }

    bool
    isBuzzing()
    {
        return m_buzzing;
    }

    void
    sync( VibeControl& other )
    {
        if ( m_aVibeList == other.m_aVibeList
                && m_nCountVibe == other.m_nCountVibe )
        {
            if ( m_index != other.m_index )
            {
                m_index = other.m_index;
                int nDelay = other.m_tDelay.getDelay();
                m_tDelay.newDelay( nDelay );
                other.m_tDelay.newDelay( nDelay );
            }
        }
    }

protected:
    uint8_t         m_pin;
    uint8_t         m_val;
    bool            m_buzzing = false;
    YogiDelay       m_tDelay;
    int             m_nCountVibe = 0;
    const VibeItem* m_aVibeList = nullptr;
    int             m_index = 0;
};


VibeControl g_tVibeLeft( kPinVibeLeft );
VibeControl g_tVibeRight( kPinVibeRight );

//============== SLEEP ===============

void
enterSleep()
{
    DEBUG_PRINTLN( "Sleepy" );
    DEBUG_DELAY( 300 );

    g_tVibeLeft.off();
    g_tVibeRight.off();

    g_tRelay.reset();  // powerdown sensors

    // don't generate inactivity interrupts during sleep
    adxlDrowsy();

    g_tSleep.prepareSleep();
    adxlAttachInterrupt();
    g_tSleep.sleep();
    g_tSleep.postSleep();

    // we wakeup here
    g_tRelay.set();
    adxlWakeup();
    DEBUG_PRINTLN( "Wake Up" );

    // ? do we need to reintialize any of the sensors ?
}


//=============== SONIC ==================

class CAvgSonic
{
public:
    CAvgSonic( YogiSonic& sonic, char* sName )
            : m_rSonic( sonic )
            , m_tDelay( 200 )
            , m_nDistCurrent( 0 )
            , m_nDistPrevious( 0 )
            , m_nIndex( 0 )
    {
        reset();
        strcpy( m_sName, sName );
    }

public:
    bool
    timesUp( unsigned long uTimeCurrent )
    {
        return m_tDelay.timesUp( uTimeCurrent );
    }


    void
    reset()
    {
        for ( int i = 0; i < k_nSize; ++i )
            m_aDist[i] = 0;
        m_nIndex = 0;
        m_nDistCurrent = 0;
        m_nDistPrevious = 0;
    }


    bool
    isDirty()
    {
        bool bDirty = false;
        long distNew = this->getDistanceCm();
        long distPrev = m_nDistPrevious;

        if ( 0 == distNew )
        {
            if ( 0 < distPrev )
            {
                long distRetry = this->getDistanceCm();
                if ( 0 < distRetry )
                    distNew = distRetry;
            }
        }
        else
        {}


        m_nDistPrevious = m_nDistCurrent;
        m_nDistCurrent = distNew;
        if ( distPrev != distNew )
        {
            bDirty = true;
        }
        return bDirty;
    }


    inline long
    getDistance()
    {
        return m_nDistCurrent;
    }


    long
    getDistanceCm()
    {
        long dist = m_rSonic.getDistanceCm();
        if ( 0 < dist && dist < k_minDistance )
            dist = 0;
        return dist;
    }


protected:
    char             m_sName[24];
    YogiSonic&       m_rSonic;
    YogiDelay        m_tDelay;
    long             m_nDistCurrent;
    long             m_nDistPrevious;
    static const int k_nSize = 3;
    int              m_nIndex;
    long             m_aDist[k_nSize];
};


YogiSonic g_tSonicLeft( kPinSonicTrigLeft, kPinSonicEchoLeft );
YogiSonic g_tSonicFront( kPinSonicTrigFront, kPinSonicEchoFront );
YogiSonic g_tSonicRight( kPinSonicTrigRight, kPinSonicEchoRight );

CAvgSonic g_tAvgLeft( g_tSonicLeft, "Left" );
CAvgSonic g_tAvgFront( g_tSonicFront, "Front" );
CAvgSonic g_tAvgRight( g_tSonicRight, "Right" );


int g_nPotValueSide = 20;  // distance in cm to alarm
int g_nPotValueFront = g_nPotValueSide * 2;


void
sonicSetup()
{
    g_tSonicLeft.init();
    g_tSonicFront.init();
    g_tSonicRight.init();

#define SONIC_SENSOR_TOTAL 3  // count of ultrasonic sensors
#define SONIC_SENSOR_HZ    4  // Hz rate per sensor
#define SONIC_SENSORS_HZ   ( SONIC_SENSOR_TOTAL * SONIC_SENSOR_HZ )

    g_nSonicCount = 0;
    g_nSonicCycle = 0;
    g_tSonicDelay.init( 1000 / SONIC_SENSORS_HZ );
}


int
potentiometerRead( uint8_t pin, long nRange )
{
    return max( 10, abs( (long)analogRead( pin ) * nRange / 1024 ) );
}

void
updatePotValues()
{
    pinMode( kPinPotDist, INPUT );
    g_nPotValueSide = potentiometerRead( kPinPotDist, 100 );
    g_nPotValueFront = g_nPotValueSide * 3 / 2;

    DEBUG_VPRINT( "Pot Values: Side=", g_nPotValueSide );
    DEBUG_VPRINTLN( "; Front=", g_nPotValueFront );

    g_tSonicLeft.setMaxDistance( g_nPotValueSide );
    g_tSonicFront.setMaxDistance( g_nPotValueFront );
    g_tSonicRight.setMaxDistance( g_nPotValueSide );
}


bool
isHorizontal()
{
    int x, y, z;
    adxl.readAccel( &x, &y, &z );
    return abs( z ) < 100 ? true : false;
}


bool
isLayingdown()
{
    if ( isHorizontal() )
    {
        g_uTimeCurrent = millis();
        if ( 0 == g_uTimeLaying )
            g_uTimeLaying = g_uTimeCurrent;
        if ( 1000 < g_uTimeCurrent - g_uTimeLaying )
            return true;
        else
            return false;
    }
    else
    {
        g_uTimeLaying = 0;
        return false;
    }
}


//=============== STATES ==================

void
orientationUnknown()
{
    g_eOrientation = isHorizontal() ? OR_HORIZONTAL : OR_VERTICAL;
}

void
orientationVertical()
{
    if ( isLayingdown() )
    {
        g_eOrientation = OR_HORIZONTAL;
    }
    else
    {
        uint8_t mInterrupts = 0;
        g_uTimeCurrent = millis();
        if ( 0 < g_uCountInterrupt )
        {
#if ! defined( ANTIVIBE )
            mInterrupts = adxlGetInterrupts();
            g_uTimeInterrupt = millis();
#else
            if ( ! g_tVibeLeft.isBuzzing() && ! g_tVibeRight.isBuzzing() )
            {
                mInterrupts = adxlGetInterrupts();
                g_uTimeInterrupt = millis();
            }
            else
            {
                DEBUG_PRINTLN( "Ignoring Interrupt" );
                adxl.getInterruptSource();
                mInterrupts = 0;
            }
#endif
        }
        else
        {
            if ( k_uDelaySleep < g_uTimeCurrent - g_uTimeInterrupt )
            {
                adxl.getInterruptSource();
                g_uTimeInterrupt = g_uTimeCurrent;
                mInterrupts = ADXL_M_INACTIVITY;
            }
        }

        if ( 0 != mInterrupts )
        {
            adxlDataHandler( mInterrupts );
            if ( 0 != ( mInterrupts & ADXL_M_INACTIVITY ) )
            {
                WatchDog::stop();
                enterSleep();

                // stuff to do when we wake up
                g_nActivity = 0;
                updatePotValues();
                g_tAvgLeft.reset();
                g_tAvgFront.reset();
                g_tAvgRight.reset();
                g_tVibeLeft.reset();
                g_tVibeRight.reset();
                g_nSonicCycle = 0;
                g_nSonicCount = 0;
            }
            g_uTimeInterrupt = millis();
        }
        else
        {
            bool bDirty = false;
            if ( g_tSonicDelay.timesUp( g_uTimeCurrent ) )
            {
                switch ( g_nSonicCycle++ )
                {
                case 0:
                    bDirty = g_tAvgLeft.isDirty();
                    break;
                case 1:
                    bDirty = g_tAvgRight.isDirty();
                    break;
                case 2:
                    bDirty = g_tAvgFront.isDirty();
                default:
                    g_nSonicCycle = 0;
                    break;
                }
            }

            if ( bDirty )
            {
                long nL = g_tAvgLeft.getDistance();   // Left
                long nF = g_tAvgFront.getDistance();  // Front
                long nR = g_tAvgRight.getDistance();  // Right

                // both sides are within range
                // probably going through door
                if ( 0 < nL && 0 < nR )
                {
                    nL = 0;
                    nF = 0;
                    nR = 0;
                }

                DEBUG_STATEMENT( ++g_nSonicCount );
                DEBUG_VPRINT( "L = ", nL );
                DEBUG_VPRINT( ";  F = ", nF );
                DEBUG_VPRINT( ";  R = ", nR );
                DEBUG_VPRINT( "; pS = ", g_nPotValueSide );
                DEBUG_VPRINT( "; pF = ", g_nPotValueFront );
                DEBUG_VPRINTLN( "; #", g_nSonicCount );

                bool    bVibeLeft = false;
                bool    bVibeRight = false;
                uint8_t nVibeValueLeft = 0;
                uint8_t nVibeValueFront = 0;
                uint8_t nVibeValueRight = 0;

                if ( 1 < nL )
                {
                    nVibeValueLeft
                            = map( nL, g_nPotValueSide, k_minDistance, 1, 255 );
                }

                if ( 1 < nF )
                {
                    nVibeValueFront = map(
                            nF, g_nPotValueFront, k_minDistance, 1, 255 );
                }

                if ( 1 < nR )
                {
                    nVibeValueRight
                            = map( nR, g_nPotValueSide, k_minDistance, 1, 255 );
                }

                if ( 1 < nF )
                {
                    if ( 1 < nL )
                    {
                        bVibeLeft = true;
                        nVibeValueLeft = max( nVibeValueLeft, nVibeValueFront );
                        g_tVibeLeft.setPattern( g_nVibeCountBoth, g_aVibeBoth );
                    }
                    else if ( 1 < nR )
                    {
                        bVibeRight = true;
                        nVibeValueRight
                                = max( nVibeValueRight, nVibeValueFront );
                        g_tVibeRight.setPattern(
                                g_nVibeCountBoth, g_aVibeBoth );
                    }
                    else  // front
                    {
                        bVibeLeft = true;
                        bVibeRight = true;
                        nVibeValueLeft = nVibeValueFront;
                        nVibeValueRight = nVibeValueFront;
                        g_tVibeLeft.setPattern(
                                g_nVibeCountFront, g_aVibeFront );
                        g_tVibeRight.setPattern(
                                g_nVibeCountFront, g_aVibeFront );
                    }
                }
                else if ( 1 < nL )
                {
                    bVibeLeft = true;
                    g_tVibeLeft.setPattern( g_nVibeCountSide, g_aVibeSide );
                }
                else if ( 1 < nR )
                {
                    bVibeRight = true;
                    g_tVibeRight.setPattern( g_nVibeCountSide, g_aVibeSide );
                }


                if ( bVibeLeft )
                    g_tVibeLeft.on( nVibeValueLeft );
                else
                    g_tVibeLeft.off();


                if ( bVibeRight )
                    g_tVibeRight.on( nVibeValueRight );
                else
                    g_tVibeRight.off();


                g_tVibeLeft.sync( g_tVibeRight );
                g_tVibeLeft.cycle( g_uTimeCurrent );
                g_tVibeRight.cycle( g_uTimeCurrent );
            }
            else
            {
                g_tVibeLeft.cycle( g_uTimeCurrent );
                g_tVibeRight.cycle( g_uTimeCurrent );
            }
        }
    }
}


void
orientationHorizontal()
{
    // Check if we are laying-down. If we are not
    // laying-down then switch modes to OR_VERTICAL.
    if ( isLayingdown() )
    {
        if ( ! g_bActiveLaydown )
        {
            g_bActiveLaydown = true;

            watchdogSleep();
        }
    }
    else
    {
        if ( g_bActiveLaydown )
        {
            g_bActiveLaydown = false;
            watchdogWakeup();
            g_eOrientation = OR_VERTICAL;
        }
    }
}


//================== SETUP =====================
void
setup()
{
    DEBUG_OPEN();

    g_tRelay.init();
    g_tRelay.set();  // power up sensors

    updatePotValues();

    adxlSetup( k_uAdxlDelaySleep );
    adxlAttachInterrupt();

    sonicSetup();

    g_tVibeLeft.init();
    g_tVibeRight.init();

    g_bWatchDogInterrupt = false;
    WatchDog::init( watchdogIntHandler, OVF_8000MS );
    WatchDog::stop();

    g_eOrientation = isHorizontal() ? OR_HORIZONTAL : OR_VERTICAL;

    g_uTimeInterrupt = millis();
    g_uTimeCurrent = millis();
    g_uTimePrevious = 0;
    g_uTimeLaying = 0;
}


//================= LOOP =======================
void
loop()
{
    if ( g_bWatchDogInterrupt )
    {
        g_bWatchDogInterrupt = false;
        watchdogHandler();
    }
    else
    {
        switch ( g_eOrientation )
        {
        case OR_VERTICAL:
            orientationVertical();
            break;
        case OR_HORIZONTAL:
            orientationHorizontal();
            break;
        case OR_UNKNOWN:
        default:
            orientationUnknown();
            break;
        }
    }
}
