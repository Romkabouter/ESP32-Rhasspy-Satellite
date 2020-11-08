#ifndef _application_h_
#define _application_h_

#include "state_machine/States.h"

//class I2SSampler;
//class I2SOutput;
class State;
//class IndicatorLight;
//class Speaker;
//class IntentProcessor;

class Application
{
private:
    State *everloopState;
//    State *m_detect_wake_word_state;
    // State *m_recognise_command_state;
   State *currentState;

public:
//    Application(I2SSampler *sample_provider, IntentProcessor *intent_processor, Speaker *speaker, IndicatorLight *indicator_light);
    Application();
    ~Application();
    void run();
};

#endif