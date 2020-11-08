#include <Arduino.h>
#include "Application.h"
#include "state_machine/EverloopState.h"
#include "wishbone_bus.h"

//#include "state_machine/DetectWakeWordState.h"
// #include "state_machine/RecogniseCommandState.h"
// #include "IndicatorLight.h"
// #include "Speaker.h"
// #include "IntentProcessor.h"

//Application::Application(I2SSampler *sample_provider, IntentProcessor *intent_processor, Speaker *speaker, IndicatorLight *indicator_light)
Application::Application()
{
    // // detect wake word state - waits for the wake word to be detected
    // m_detect_wake_word_state = new DetectWakeWordState(sample_provider);
    // // command recongiser - streams audio to the server for recognition
    // m_recognise_command_state = new RecogniseCommandState(sample_provider, indicator_light, speaker, intent_processor);
    // // start off in the detecting wakeword state
    // m_current_state = m_detect_wake_word_state;
    // m_current_state->enterState();
    matrix_hal::WishboneBus wb;
    wb.Init();
    everloopState = new EverloopState(&wb);
    currentState = everloopState;
    //currentState->enterState();
}

void Application::run()
{
    bool state_done = currentState->run();
    currentState->enterState();
    if (state_done)
    {
         currentState->exitState();
         // switch to the next state
         if (currentState == everloopState)
         {
            //Swith state
             currentState = everloopState;
         }
         else
         {
             currentState = everloopState;
         }
         currentState->enterState();
    }
    vTaskDelay(10);
}
