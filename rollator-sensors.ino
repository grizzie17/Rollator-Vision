#include <MPU9250.h>
#include <Sleep_n0m1.h>

#include <YogiDelay.h>
#include <YogiPitches.h>
#include <YogiSonic.h>

#include "debug.h"

const uint8_t kPinRelay = 13;

const uint8_t kPinSonicTrigLeft = 12;
const uint8_t kPinSonicEchoLeft = 11;       // PWM
const uint8_t kPinSonicTrigFrontLeft = 10;  // PWM
const uint8_t kPinSonicEchoFrontLeft = 9;   // PWM
const uint8_t kPinSonicTrigFrontRight = 8;
const uint8_t kPinSonicEchoFrontRight = 7;
const uint8_t kPinVibeLeft = 6;   // PWM
const uint8_t kPinVibeRight = 5;  // PWM
const uint8_t kPinSonicTrigRight = 4;
const uint8_t kPinSonicEchoRight = 3;  // INT1 [PWM]
const uint8_t kPinAccel = 2;           // INT0
const int     kPinPot = A0;

const double kDistanceRed = 10.0;


class LedControl
{
public:
    LedControl( uint8_t pin )
            : m_pin( pin )
            , m_on( false )
    {}

    void
    init()
    {
        pinMode( m_pin, OUTPUT );
    }

    void
    on()
    {
        if ( ! m_on )
        {
            digitalWrite( m_pin, HIGH );
            m_on = true;
        }
    }

    void
    off()
    {
        if ( m_on )
        {
            digitalWrite( m_pin, LOW );
            m_on = false;
        }
    }

protected:
    bool    m_on;
    uint8_t m_pin;
};


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


#if false
class ToneControl
{
public:
    ToneControl()
            : m_pin( 0 )
            , m_pMelody( 0 )
            , m_nNoteCount( 0 )
            , m_nIndex( 0 )
            , m_tDelay( 1000 )
            , m_bPlaying( false )
    {}
    ~ToneControl()
    {
        delete[] m_pMelody;
    }

    void
    init( uint8_t pin, int nCount, int* pMelody, unsigned int nDuration )
    {
        m_pin = pin;
        pinMode( m_pin, OUTPUT );

        delete[] m_pMelody;

        m_nNoteCount = nCount;
        if ( 0 < m_nNoteCount )
        {
            m_pMelody = new int[m_nNoteCount];
            for ( int i = 0; i < m_nNoteCount; ++i )
                m_pMelody[i] = pMelody[i];
        }
        m_tDelay.init( 1000 );
        m_nDuration = nDuration;
    }

    void
    start( unsigned long nTime )
    {
        if ( m_bPlaying )
            m_nIndex = 0;
        m_bPlaying = true;
        play( nTime );
    }

    void
    play( unsigned long nTime )
    {
        if ( m_bPlaying )
        {
            if ( m_tDelay.timesUp( nTime ) )
            {
                tone( m_pin, m_pMelody[m_nIndex], m_nDuration );
                ++m_nIndex;
                if ( m_nNoteCount <= m_nIndex )
                {
                    m_nIndex = 0;
                    m_tDelay.reset();
                }
            }
        }
    }

    void
    stop()
    {
        m_nIndex = 0;
        m_bPlaying = false;
        m_tDelay.reset();
        tone( m_pin, 0, 0 );
    }

protected:
    uint8_t       m_pin;
    int           m_nNoteCount;
    int*          m_pMelody;
    int           m_nIndex;
    unsigned long m_nDuration;
    YogiDelay     m_tDelay;
    bool          m_bPlaying;
};
#endif  // false


class CAvgSonic
{
public:
    CAvgSonic( YogiSonic& sonic, char* sName )
            : m_rSonic( sonic )
            , m_tDelay( 500 )
            , m_nDelay( 500 )
            , m_nDelayCount( 0 )
            , m_nDistance( 0 )
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
        m_nDelayCount = 0;
        m_nDistance = 0;
    }


    bool
    isDirty()
    {
        bool bDirty = false;
        long dist = this->getDistanceCm();
        if ( dist != m_nDistance )
        {
            bDirty = true;
            m_nDistance = dist;
        }
        return bDirty;
    }


    long
    getDistance()
    {
        return m_nDistance;
    }


    long
    getDistanceCm()
    {
        long dist = m_rSonic.getDistanceCm();
        int  nIndexPrevious = m_nIndex;
        if ( 0 == nIndexPrevious )
            nIndexPrevious = k_nSize - 1;
        long distPrevious = m_aDist[nIndexPrevious];
        if ( 0 < distPrevious && abs( dist - distPrevious ) <= 2 )
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
            if ( 0 < m_aDist[i] )
            {
                dist += m_aDist[i];
                ++j;
            }
        }

        if ( 0 < j )
            return dist / j;
        else
            return 0;
    }


protected:
    char             m_sName[24];
    YogiSonic&       m_rSonic;
    YogiDelay        m_tDelay;
    unsigned long    m_nDelay;       // milliseconds to delay
    int              m_nDelayCount;  // idle period
    long             m_nDistance;
    long             m_nDistPrevious;
    static const int k_nSize = 3;
    int              m_nIndex;
    long             m_aDist[k_nSize];
};


LedControl g_tVibeLeft( kPinVibeLeft );
LedControl g_tVibeRight( kPinVibeRight );


YogiSonic g_tSonicLeft;
YogiSonic g_tSonicFrontLeft;
YogiSonic g_tSonicFrontRight;
YogiSonic g_tSonicRight;

CAvgSonic g_tAvgLeft( g_tSonicLeft, "Left" );
CAvgSonic g_tAvgFrontLeft( g_tSonicFrontLeft, "FrontLeft" );
CAvgSonic g_tAvgFrontRight( g_tSonicFrontRight, "FrontRight" );
CAvgSonic g_tAvgRight( g_tSonicRight, "Right" );

MPU9250 g_tIMU( Wire, 0x68 );
Sleep   g_tSleep;

YogiDelay g_tDelay;


unsigned long g_uTimeIdle = 1000 * 10;  // idle time before sleep
long          g_nPotValue = 20;         // distance in cm to alarm


volatile bool g_bAccelInterrupt = false;  // did the accel cause interrupt
unsigned long g_uCountInterrupt = 0;      // number of interrupts
unsigned long g_uTimeInterrupt = 0;
unsigned long g_uTimeCurrent = 0;


void
interruptHandler()
{
    g_bAccelInterrupt = true;
}


void
interruptAttach()
{
    attachInterrupt(
            digitalPinToInterrupt( kPinAccel ), interruptHandler, RISING );
}

void
enterSleep()
{
    Serial.println( "Entering Sleep Mode" );
    delay( 500 );
    g_tSleep.pwrDownMode();
    g_tSleep.sleepPinInterrupt( kPinAccel, RISING );

    // we wakeup here
    Serial.println( "Wake Up" );
    interruptHandler();
    interruptAttach();
    g_uTimeCurrent = millis();
}


void
setup()
{
    Serial.begin( 115200 );
    while ( ! Serial )
        ;  // wait for Serial to startup

    g_tSonicLeft.init( kPinSonicTrigLeft, kPinSonicEchoLeft );
    g_tSonicFrontLeft.init( kPinSonicTrigFrontLeft, kPinSonicEchoFrontLeft );
    g_tSonicFrontRight.init( kPinSonicTrigFrontRight, kPinSonicEchoFrontRight );
    g_tSonicRight.init( kPinSonicTrigRight, kPinSonicEchoRight );
    g_tVibeLeft.init();
    g_tVibeRight.init();
    g_nPotValue = (long)analogRead( kPinPot ) * 400 / 1023;


    int nStatus = g_tIMU.begin();
    if ( nStatus < 0 )
    {
        Serial.println( "IMU initialization unsuccessful" );
        Serial.print( "status=" );
        Serial.println( nStatus, HEX );
    }
    nStatus = g_tIMU.enableWakeOnMotion( 300, MPU9250::LP_ACCEL_ODR_250HZ );
    if ( nStatus )
    {
        Serial.println( "IMU enable wake on motion failure" );
        Serial.print( "status=" );
        Serial.println( nStatus, HEX );
    }

    g_tDelay.init( 1000 * 2 );

    g_bAccelInterrupt = false;
    pinMode( kPinAccel, INPUT );
    digitalWrite( kPinAccel, LOW );
    interruptAttach();
    g_uTimeCurrent = millis();
    g_uTimeInterrupt = millis();
}


void
loop()
{
    g_uTimeCurrent = millis();
    if ( g_bAccelInterrupt )
    {
        g_bAccelInterrupt = false;
        g_uTimeInterrupt = millis();
        if ( g_tDelay.timesUp( g_uTimeCurrent ) )
        {
            Serial.print( "Interrupt: " );
            Serial.println( ++g_uCountInterrupt );
        }
    }
    else
    {
        bool bDirty = false;
        if ( g_uTimeIdle < g_uTimeCurrent - g_uTimeInterrupt )
        {
            enterSleep();
            g_uTimeInterrupt = millis();
            g_uCountInterrupt = 0;
        }
        else
        {
            if ( g_tAvgFrontLeft.timesUp( g_uTimeCurrent ) )
            {
                bDirty = g_tAvgFrontLeft.isDirty();
            }
            else if ( g_tAvgFrontRight.timesUp( g_uTimeCurrent ) )
            {
                bDirty = g_tAvgFrontRight.isDirty();
            }
            else if ( g_tAvgLeft.timesUp( g_uTimeCurrent ) )
            {
                bDirty = g_tAvgLeft.isDirty();
            }
            else if ( g_tAvgRight.timesUp( g_uTimeCurrent ) )
            {
                bDirty = g_tAvgRight.isDirty();
            }

            if ( bDirty )
            {
                long nL = g_tAvgLeft.getDistance();
                long nFL = g_tAvgFrontLeft.getDistance();
                long nFR = g_tAvgFrontRight.getDistance();
                long nR = g_tAvgRight.getDistance();
                Serial.print( "L = " );
                Serial.print( nL );
                Serial.print( ";  FL = " );
                Serial.print( nFL );
                Serial.print( ";  FR = " );
                Serial.print( nFR );
                Serial.print( ";  R = " );
                Serial.print( nR );
                Serial.print( "; pot = " );
                Serial.println( g_nPotValue );

                bDirty = false;
                if ( ( 1 < nL && nL < g_nPotValue )  //
                        || ( 1 < nFL && nFL < g_nPotValue ) )
                {
                    g_tVibeLeft.on();
                    bDirty = true;
                }
                else
                {
                    g_tVibeLeft.off();
                }

                if ( ( 1 < nFR && nFR < g_nPotValue )  //
                        || ( 1 < nR && nR < g_nPotValue ) )
                {
                    g_tVibeRight.on();
                    bDirty = true;
                }
                else
                {
                    g_tVibeRight.off();
                }
            }
        }
    }
}
