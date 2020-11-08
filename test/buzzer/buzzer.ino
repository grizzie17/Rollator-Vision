

// turn on the relay
// flash the vibe/buzzers on/off

const uint8_t k_pinRelaySet = 3;
const uint8_t k_pinRelayReset = 4;
const uint8_t k_pinVibeLeft = 5;
const uint8_t k_pinVibeRight = 6;

unsigned long g_uTimeCurrent = 0;
unsigned long g_uTimePrevious = 0;

uint8_t g_uVibe = LOW;

void
setup()
{
    Serial.begin( 115200 );
    while ( ! Serial )
        delay( 10 );

    delay( 500 );
    Serial.println( "TEST: vibrator!" );


    pinMode( k_pinVibeLeft, OUTPUT );
    pinMode( k_pinVibeRight, OUTPUT );

    // enable relay
    pinMode( k_pinRelaySet, OUTPUT );
    pinMode( k_pinRelayReset, OUTPUT );
    digitalWrite( k_pinRelaySet, HIGH );
    delay( 100 );
    digitalWrite( k_pinRelaySet, LOW );

    g_uTimeCurrent = 0;
    g_uTimePrevious = 0;
}


void
loop()
{
    g_uTimeCurrent = millis();
    if ( 1000 < g_uTimeCurrent - g_uTimePrevious )
    {
        g_uTimePrevious = g_uTimeCurrent;

        // for now toggle vibe
        // after successful test change to analogWrite
        digitalWrite( k_pinVibeLeft, g_uVibe );
        g_uVibe = g_uVibe ? LOW : HIGH;
        digitalWrite( k_pinVibeRight, g_uVibe );
    }
}
