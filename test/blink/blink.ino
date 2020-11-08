// similar to the normal blink sketch
// but blinking the status pin.

const uint8_t k_pinStatus = 12;

unsigned long g_uTimeCurrent = 0;
unsigned long g_uTimePrevious = 0;


void
setup()
{
    Serial.begin( 115200 );
    while ( ! Serial )
        delay( 10 );

    pinMode( k_pinStatus, OUTPUT );
    digitalWrite( k_pinStatus, LOW );

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

        digitalWrite( k_pinStatus, digitalRead( k_pinStatus ) ? LOW : HIGH );
    }
}
