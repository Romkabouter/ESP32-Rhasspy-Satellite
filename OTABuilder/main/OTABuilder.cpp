/* ************************************************************************* *
 * OTABuilder
 * 
 * This program does nothing. It can be used to generate the OTA partittions
 * for the MatrixVoiceAudioServerArduino 
 * 
 * ************************************************************************ */
int cpp_loop(void)
{
    while(true) {
        //Do nothing
    }
}

extern "C" {
   void app_main(void) {cpp_loop();}
}
