
#define _DEBUG
#include <YogiDebug.h>

#include <ADXL345_setup.h>  // includes SparkFun ADXL345 Library
#include <WatchDog.h>
// #include <Wire.h>

#include <YogiDelay.h>
#include <YogiPitches.h>
#include <YogiSleep.h>
#include <YogiSonic.h>

const uint8_t k_pinINT = 2;  // INT0


const uint8_t kPinVibeRight = 3;
const uint8_t kPinVibeFrontRight = 5;
const uint8_t kPinVibeFrontLeft = 4;
const uint8_t kPinVibeLeft = 6;

const uint8_t kPinSonicEchoRight = 7;
const uint8_t kPinSonicTrigRight = 7;

const uint8_t kPinSonicEchoFrontRight = 8;
const uint8_t kPinSonicTrigFrontRight = 8;

const uint8_t kPinSonicEchoFrontLeft = 9;
const uint8_t kPinSonicTrigFrontLeft = 9;

const uint8_t kPinSonicEchoLeft = 10;
const uint8_t kPinSonicTrigLeft = 10;

const uint8_t k_pinRelay = 11;

const uint8_t kPinPotDist = A0;
const uint8_t kPinSDA = A4;
const uint8_t kPinSCL = A5;


#ifndef __asm__
#    ifdef _MSC_VER
#        define __asm__ __asm
#    endif
#endif


typedef enum orientation_t
        : uint8_t
{
    OR_UNKNOWN,
    OR_VERTICAL,
    OR_HORIZONTAL
} orientation_t;

orientation_t g_eOrientation = OR_UNKNOWN;

bool          g_bSleepy = false;
bool          g_bActiveLaydown = false;
unsigned long g_uLaying = 0;


unsigned long g_uTimeCurrent = 0;
unsigned long g_uTimePrevious = 0;
unsigned long g_uTimeInterrupt = 0;

const uint8_t       k_uAdxlDelaySleep = 45;
const unsigned long k_uDelaySleep = 600 * k_uAdxlDelaySleep;


YogiDelay g_tSonicDelay;
int       g_nSonicCycle = 0;
long      g_nSonicCount = 0;


YogiSleep g_tSleep;


//============== RELAY ===============

inline void
relaySetup()
{
    pinMode( k_pinRelay, OUTPUT );
}


void
relayEnable()
{
    relaySetup();
    digitalWrite( k_pinRelay, HIGH );
    delay( 400 );
}


void
relayDisable()
{
    relaySetup();
    digitalWrite( k_pinRelay, LOW );
}


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
    // adxlAttachInterrupt();
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
    relayDisable();
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
    relayEnable();
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

        g_uLaying = 0;
        g_bActiveLaydown = false;
        g_eOrientation = OR_VERTICAL;
    }
}


//============== LED ===============

class LedControl
{
public:
    LedControl( uint8_t pin )
            : m_pin( pin )
            , m_val( 0 )
            , m_on( false )
    {}

    void
    init()
    {
        pinMode( m_pin, OUTPUT );
        m_val = 0;
    }

    void
    on( uint8_t value = HIGH )
    {
        if ( m_val != value )
        {
            m_val = value;
            analogWrite( m_pin, m_val );
            m_on = true;
        }
    }

    void
    off()
    {
        if ( 0 < m_val )
        {
            m_val = 0;
            analogWrite( m_pin, m_val );
            m_on = false;
        }
    }

protected:
    bool    m_on;
    uint8_t m_val;
    uint8_t m_pin;
};


//=========== VIBE ===========

class VibeControl
{
public:
    VibeControl()
            : m_pin( 0 )
    {}

public:
    void
    init( uint8_t pin )
    {
        m_pin = pin;
    }

    void
    start()
    {}

    void
    stop()
    {}


protected:
    uint8_t m_pin;
};


LedControl g_tVibeLeft( kPinVibeLeft );
LedControl g_tVibeFrontLeft( kPinVibeFrontLeft );
LedControl g_tVibeFrontRight( kPinVibeFrontRight );
LedControl g_tVibeRight( kPinVibeRight );

//============== SLEEP ===============

void
enterSleep()
{
    DEBUG_PRINTLN( "Sleepy" );
    DEBUG_DELAY( 300 );

    g_tVibeLeft.off();
    g_tVibeFrontLeft.off();
    g_tVibeFrontRight.off();
    g_tVibeRight.off();

    relayDisable();  // power-down sensors

    // don't generate inactivity interrupts during sleep
    adxlDrowsy();

    g_bSleepy = true;
    g_tSleep.prepareSleep();
    adxlAttachInterrupt();
    g_tSleep.sleep();
    g_tSleep.postSleep();

    // we wakeup here
    g_bSleepy = false;
    relayEnable();
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
    ~CAvgSonic()
    {}

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
        m_nDistPrevious = m_nDistCurrent;
        m_nDistCurrent = distNew;
        if ( distPrev != distNew )
        {
            bDirty = true;
        }
        // else if ( distNew < 2 && m_nDistCurrent != m_nDistPrevious )
        // {
        //     bDirty = true;
        //     m_nDistCurrent = m_nDistPrevious;
        // }
        // else
        // {
        //     bDirty = true;
        //     m_nDistPrevious = distNew;
        //     m_nDistCurrent = distNew;
        // }
        return bDirty;
    }


    inline long
    getDistance()
    {
        // if ( m_nDistCurrent < 2 )
        // {
        //     m_nDistCurrent = this->getDistanceCm();
        // }
        return m_nDistCurrent;
    }


    long
    getDistanceCm()
    {
#if true
        return m_rSonic.getDistanceCm();
#elif false
        int nIndexPrevious = m_nIndex;
        if ( 0 == nIndexPrevious )
            nIndexPrevious = k_nSize - 1;
        long distPrevious = m_aDist[nIndexPrevious];
        if ( 1 < distPrevious && abs( dist - distPrevious ) <= 2 )
            dist = distPrevious;
        m_aDist[m_nIndex] = dist;
        if ( k_nSize <= ++m_nIndex )
        {
            m_nIndex = 0;
        }
        dist = 0;
        int j = 0;
        for ( int i = 0; i < k_nSize; ++i )
        {
            if ( 1 < m_aDist[i] )
            {
                dist += m_aDist[i];
                ++j;
            }
        }

        if ( 0 < j )
            return dist / j;
        else
            return 0;
#else
        long distPrevious = m_nDistPrevious;
        if ( 1 < distPrevious && abs( dist - distPrevious ) <= 4 )
            dist = distPrevious;
        m_nDistPrevious = dist;
        return dist;
#endif
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
YogiSonic g_tSonicFrontLeft( kPinSonicTrigFrontLeft, kPinSonicEchoFrontLeft );
YogiSonic g_tSonicFrontRight(
        kPinSonicTrigFrontRight, kPinSonicEchoFrontRight );
YogiSonic g_tSonicRight( kPinSonicTrigRight, kPinSonicEchoRight );

CAvgSonic g_tAvgLeft( g_tSonicLeft, "Left" );
CAvgSonic g_tAvgFrontLeft( g_tSonicFrontLeft, "FrontLeft" );
CAvgSonic g_tAvgFrontRight( g_tSonicFrontRight, "FrontRight" );
CAvgSonic g_tAvgRight( g_tSonicRight, "Right" );


int g_nPotDistValue = 20;  // distance in cm to alarm
int g_nPotValueFront = g_nPotDistValue * 1.5;


void
sonicSetup()
{
    g_tSonicLeft.init();
    g_tSonicFrontLeft.init();
    g_tSonicFrontRight.init();
    g_tSonicRight.init();

    g_nSonicCount = 0;
    g_nSonicCycle = 0;
    g_tSonicDelay.init( 1000 / 20 );  // 20 Hz
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
    // g_nPotSensValue = potentiometerRead( kPinPotSens, 200 );
    g_nPotDistValue = potentiometerRead( kPinPotDist, 100 );
    g_nPotValueFront = g_nPotDistValue * 1.5;

    DEBUG_VPRINT( "Pot Values: Side=", g_nPotDistValue );
    DEBUG_VPRINTLN( "; Front=", g_nPotValueFront );
    // DEBUG_VPRINTLN( "; Sensitivity=", g_nPotSensValue );

    g_tSonicLeft.setMaxDistance( g_nPotDistValue );
    g_tSonicFrontLeft.setMaxDistance( g_nPotValueFront );
    g_tSonicFrontRight.setMaxDistance( g_nPotValueFront );
    g_tSonicRight.setMaxDistance( g_nPotDistValue );
}


bool
isHorizontal()
{
    int x, y, z;
    adxl.readAccel( &x, &y, &z );
    return abs( z ) < 100 ? true : false;
}


bool g_bLayingDown = false;

bool
isLayingdown()
{
    if ( isHorizontal() )
    {
        ++g_uLaying;
        return 5 < g_uLaying ? true : false;
    }
    else
    {
        g_uLaying = 0;
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
            mInterrupts = adxlGetInterrupts();
            g_uTimeInterrupt = millis();
        }
        else
        {
            if ( k_uDelaySleep < abs( g_uTimeCurrent - g_uTimeInterrupt ) )
            {
                adxl.getInterruptSource();
                g_uTimeInterrupt = g_uTimeCurrent;
                mInterrupts = ADXL_M_INACTIVITY;
            }
        }
        // g_uCountInterrupt = 0;

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
                g_tAvgFrontLeft.reset();
                g_tAvgFrontRight.reset();
                g_tAvgRight.reset();
                g_nSonicCycle = 0;
                g_nSonicCount = 0;
            }
            g_uTimeInterrupt = millis();
        }
        else  //if ( ! g_bSleepy )
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
                    bDirty = g_tAvgFrontLeft.isDirty();
                    break;
                case 3:
                    bDirty = g_tAvgFrontRight.isDirty();
                    break;
                default:
                    g_nSonicCycle = 0;
                    break;
                }
            }

            if ( bDirty )
            {
                long nL = g_tAvgLeft.getDistance();
                long nFL = g_tAvgFrontLeft.getDistance();
                long nFR = g_tAvgFrontRight.getDistance();
                long nR = g_tAvgRight.getDistance();

                ++g_nSonicCount;
                DEBUG_VPRINT( "L = ", nL );
                DEBUG_VPRINT( ";  FL = ", nFL );
                DEBUG_VPRINT( ";  FR = ", nFR );
                DEBUG_VPRINT( ";  R = ", nR );
                DEBUG_VPRINT( "; pS = ", g_nPotDistValue );
                DEBUG_VPRINT( "; pF = ", g_nPotValueFront );
                DEBUG_VPRINTLN( "; #", g_nSonicCount );

                // both sides are within range
                // probably going through door
                if ( 0 < nL && 0 < nR )
                {
                    nL = 0;
                    nFL = 0;
                    nFR = 0;
                    nR = 0;
                }

                // bDirty = false;
                bool    bVibeLeft = false;
                bool    bVibeFrontLeft = false;
                bool    bVibeFrontRight = false;
                bool    bVibeRight = false;
                uint8_t nVibeLeftValue = 0;
                uint8_t nVibeFrontLeftValue = 0;
                uint8_t nVibeFrontRightValue = 0;
                uint8_t nVibeRightValue = 0;

                if ( 1 < nL && nL <= g_nPotDistValue )
                {
                    bVibeLeft = true;
                    nVibeLeftValue = map( nL, g_nPotDistValue, 1, 1, 255 );
                }
                if ( 1 < nFL && nFL <= g_nPotValueFront )
                {
                    bVibeFrontLeft = true;
                    nVibeFrontLeftValue
                            = map( nFL, g_nPotValueFront, 1, 1, 255 );
                }

                if ( 1 < nFR && nFR <= g_nPotValueFront )
                {
                    bVibeFrontRight = true;
                    nVibeFrontRightValue
                            = map( nFR, g_nPotValueFront, 1, 1, 255 );
                }

                if ( 1 < nR && nR <= g_nPotDistValue )
                {
                    bVibeRight = true;
                    nVibeRightValue = map( nR, g_nPotDistValue, 1, 1, 255 );
                }
                // DEBUG_VPRINT( "vL=", nVibeLeftValue );
                // DEBUG_VPRINTLN( "; vR=", nVibeRightValue );


                if ( bVibeLeft )
                    g_tVibeLeft.on( nVibeLeftValue );
                else
                    g_tVibeLeft.off();

                if ( bVibeFrontLeft )
                    g_tVibeFrontLeft.on( nVibeFrontLeftValue );
                else
                    g_tVibeFrontLeft.off();

                if ( bVibeFrontRight )
                    g_tVibeFrontRight.on( nVibeFrontRightValue );
                else
                    g_tVibeFrontRight.off();

                if ( bVibeRight )
                    g_tVibeRight.on( nVibeRightValue );
                else
                    g_tVibeRight.off();
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

    relayEnable();  // power up sensors

    updatePotValues();

    // adxl = ADXL345();

    adxlSetup( k_uAdxlDelaySleep );
    adxlAttachInterrupt();


    WatchDog::init( watchdogIntHandler, OVF_4000MS );
    WatchDog::stop();


    sonicSetup();

    g_tVibeLeft.init();
    g_tVibeFrontLeft.init();
    g_tVibeFrontRight.init();
    g_tVibeRight.init();
    //pinMode( kPinPotDist, INPUT );
    // DEBUG_PRINT( "pot=" );
    // DEBUG_PRINTLN( g_nPotValue );

    g_bLayingDown = false;


    g_uTimeInterrupt = millis();
    g_uTimeCurrent = millis();
    g_uTimePrevious = 0;

    g_eOrientation = isHorizontal() ? OR_HORIZONTAL : OR_VERTICAL;
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

#if false
    g_uTimeCurrent = millis();
    uint8_t mInterrupts = adxlGetInterrupts();

    if ( 0 != mInterrupts )
    {
#    if defined( _DEBUG )
        adxlDataHandler( mInterrupts );
#    endif
        if ( 0 != ( mInterrupts & ADXL_M_INACTIVITY ) )
        {
            enterSleep();
            // stuff to do when we wake up
            digitalWrite( k_pinRelay, HIGH );  // turn on relay
            delay( 400 );                      // wait for relay
            g_nActivity = 0;
            updatePotValues();
            g_tAvgLeft.reset();
            g_tAvgFrontLeft.reset();
            g_tAvgFrontRight.reset();
            g_tAvgRight.reset();
            g_nSonicCycle = 0;
            g_nSonicCount = 0;
        }
        else
        {
            int x, y, z;
            adxl.readAccel( &x, &y, &z );
            int axy = abs( x ) + abs( y );
            int az = abs( z );
            if ( az < axy )  // laying down
            {
                if ( ! g_bLayingDown )
                {
                    g_bLayingDown = true;
                    digitalWrite( k_pinRelay, LOW );
                }
                // NEEDS_WORK: should set timer and go to sleep
            }
            else
            {
                if ( g_bLayingDown )
                {
                    g_bLayingDown = false;
                    digitalWrite( k_pinRelay, HIGH );
                    delay( 400 );  // wait for relay
                }
            }
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
                bDirty = g_tAvgFrontLeft.isDirty();
                break;
            case 3:
                bDirty = g_tAvgFrontRight.isDirty();
                break;
            default:
                g_nSonicCycle = 0;
                break;
            }
        }

        if ( bDirty )
        {
            long nL = g_tAvgLeft.getDistance();
            long nFL = g_tAvgFrontLeft.getDistance();
            long nFR = g_tAvgFrontRight.getDistance();
            long nR = g_tAvgRight.getDistance();

            // both sides are within range
            // probably going through door
            if ( 0 < nL && 0 < nR )
            {
                nL = 0;
                nR = 0;
            }

            // ++g_nSonicCount;
            // DEBUG_VPRINT( "L = ", nL );
            // DEBUG_VPRINT( ";  FL = ", nFL );
            // DEBUG_VPRINT( ";  FR = ", nFR );
            // DEBUG_VPRINT( ";  R = ", nR );
            // DEBUG_VPRINT( "; pot = ", g_nPotValue );
            // DEBUG_VPRINTLN( "; #", g_nSonicCount );

            // bDirty = false;
            bool    bVibeLeft = false;
            bool    bVibeRight = false;
            uint8_t nVibeLeftValue = 0;
            uint8_t nVibeRightValue = 0;
            if ( 1 < nL && nL < g_nPotDistValue )
            {
                bVibeLeft = true;
                nVibeLeftValue = map( nL, g_nPotDistValue, 1, 1, 255 );
            }
            if ( 1 < nFL && nFL < g_nPotValueFront )
            {
                bVibeLeft = true;
                nVibeLeftValue = map( nFL, g_nPotValueFront, 1, 1, 255 );
            }

            if ( 1 < nR && nR < g_nPotDistValue )
            {
                bVibeRight = true;
                nVibeRightValue = map( nR, g_nPotDistValue, 1, 1, 255 );
            }
            if ( 1 < nFR && nFR < g_nPotValueFront )
            {
                bVibeRight = true;
                nVibeRightValue = map( nFR, g_nPotValueFront, 1, 1, 255 );
            }

            // DEBUG_VPRINT( "vL=", nVibeLeftValue );
            // DEBUG_VPRINTLN( "; vR=", nVibeRightValue );


            if ( bVibeLeft )
                g_tVibeLeft.on( nVibeLeftValue );
            else
                g_tVibeLeft.off();

            if ( bVibeRight )
                g_tVibeRight.on( nVibeRightValue );
            else
                g_tVibeRight.off();
        }
    }
#endif
}
