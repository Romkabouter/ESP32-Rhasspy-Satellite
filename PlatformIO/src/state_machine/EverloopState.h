#ifndef _everloop_state_h_
#define _everloop_state_h_

#include "States.h"
#include "wishbone_bus.h"


class EverloopState : public State
{
public:
    int colors[4] = {0, 0, 255, 0};
    int brightness = 15;
    bool ran = false;

public:
    EverloopState(matrix_hal::WishboneBus *wb);
    void enterState();
    bool run();
    void exitState();
    void writeLeds();
};

#endif
