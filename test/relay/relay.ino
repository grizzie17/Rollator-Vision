// main purpose of this sketch is to test
// the 'confirm' circuit.


const uint8_t k_pinRelayOn = 3;
const uint8_t k_pinRelayOff = 4;
const uint8_t k_pinRelayGate = 10;
const uint8_t k_pinRelayConfirm = 11;
const uint8_t k_pinStatus = 12;

const uint8_t k_pinVibeLeft = 5;
const uint8_t k_pinVibeRight = 6;


unsigned long g_uTimeCurrent = 0;
unsigned long g_uTimePrevious = 0;

bool g_bRelayOn = false;


void
setup()
{
    Serial.begin( 115200 );
    while ( ! Serial )
        delay( 10 );

    pinMode( k_pinRelayOn, OUTPUT );
    pinMode( k_pinRelayOff, OUTPUT );
    pinMode( k_pinRelayGate, OUTPUT );
    pinMode( k_pinRelayConfirm, INPUT );

    pinMode( k_pinStatus, OUTPUT );
    pinMode( k_pinVibeLeft, OUTPUT );
    pinMode( k_pinVibeRight, OUTPUT );


    digitalWrite( k_pinRelayOff, HIGH );
    delay( 100 );
    digitalWrite( k_pinRelayOff, LOW );
    g_bRelayOn = false;

    digitalWrite( k_pinVibeLeft, HIGH );
    digitalWrite( k_pinVibeRight, HIGH );
}


void
relayTrigger( uint8_t pin, uint8_t state )
{
    if ( 0 < pin )
    {
        digitalWrite( k_pinRelayGate, HIGH );
        digitalWrite( pin, HIGH );
        for ( int i = 0; i < 20; ++i )
        {
            delay( 5 );
            if ( state == digitalRead( k_pinRelayConfirm ) )
                break;
            digitalWrite( pin, LOW );
            digitalWrite( k_pinRelayGate, LOW );
        }
        delay( 25 );
    }
}


void
loop()
{
    g_uTimeCurrent = millis();
    if ( 2000 < g_uTimeCurrent - g_uTimePrevious )
    {
        g_uTimePrevious = g_uTimeCurrent;

        if ( g_bRelayOn )
        {
            relayTrigger( k_pinRelayOff, HIGH );
            g_bRelayOn = false;
            Serial.println( "relay off" );
        }
        else
        {
            relayTrigger( k_pinRelayOn, LOW );
            g_bRelayOn = true;
            Serial.println( "relay on" );
        }
    }
}
