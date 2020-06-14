#include <YogiDelay.h>
#include <YogiPitches.h>
#include <YogiSonic.h>


#include "debug.h"


const uint8_t kPinLedRed = 11;
const uint8_t kPinLedYellow = 10;
const uint8_t kPinSonicTrig1 = 9;
const uint8_t kPinSonicEcho1 = 8;
const uint8_t kPinSonicTrig2 = 7;
const uint8_t kPinSonicEcho2 = 6;
const uint8_t kPinBuzzer = 5;
const int     kPinPot = A0;

const double kDistanceRed = 10.0;
const double kDistanceYellow = 20.0;

YogiSonic g_tSonic1;
YogiSonic g_tSonic2;

YogiDelay     g_tDelay;
unsigned long g_uTimeCurrent;


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

LedControl  g_tLedRed( kPinLedRed );
LedControl  g_tLedYellow( kPinLedYellow );
ToneControl g_tBuzzer;

int melody[] = { NOTE_C5, NOTE_D5, NOTE_E5, NOTE_F5, NOTE_G5, NOTE_A5, NOTE_B5,
    NOTE_C6 };
int duration = 500;


void
setup()
{
    Serial.begin( 9600 );
    g_tDelay.init( 1000 / 4 );  // denominator is hz value
    g_tSonic1.init( kPinSonicTrig1, kPinSonicEcho1 );
    g_tSonic2.init( kPinSonicTrig2, kPinSonicEcho2 );
    g_tLedRed.init();
    g_tLedYellow.init();
    g_tBuzzer.init( kPinBuzzer, sizeof( melody ) / sizeof( melody[0] ), melody,
            duration );
}


void
loop()
{
    g_uTimeCurrent = millis();
    if ( g_tDelay.timesUp( g_uTimeCurrent ) )
    {
        long nPotValue = (long)analogRead( kPinPot ) * 400 / 1023;
        // appendDist( g_tSonic1.getDistanceCm() );
        // double d1 = getAvgDist();
        double d1 = g_tSonic1.getDistanceCm();
        double d2 = g_tSonic2.getDistanceCm();

        Serial.print( "d1 = " );
        Serial.print( d1 );
        Serial.print( ";  d2 = " );
        Serial.print( d2 );
        Serial.print( "; pot = " );
        Serial.println( nPotValue );

        if ( ( 0.5 < d1 && d1 < nPotValue ) || ( 0.5 < d2 && d2 < nPotValue ) )
        {
            g_tLedRed.on();
            g_tLedYellow.off();
            g_tBuzzer.start( g_uTimeCurrent );
        }
        // else if ( ( 0.5 < d1 && d1 < kDistanceYellow )
        //         || ( 0.5 < d2 && d2 < kDistanceYellow ) )
        // {
        //     g_tLedRed.off();
        //     g_tLedYellow.on();
        //     g_tBuzzer.stop();
        // }
        else
        {
            g_tLedRed.off();
            g_tLedYellow.off();
            g_tBuzzer.stop();
        }
    }
    g_tBuzzer.play( g_uTimeCurrent );
}
