#pragma once
#include <math.h>

enum IndicatorState
{
    OFF,
    ON,
    PULSING,
    BLINKING
};

class IndicatorLight
{
public:
    const int BITS;
    const int limit = (1 << BITS) - 1; 
private:
    IndicatorState m_state;
    TaskHandle_t m_taskHandle;

    int pulseMS = 2000;
    const int stepDelayMS = 20; // 50 Hz update frequency
    int steps;
    float stepAngle;
    bool inversePWM;
    // current step angle
    float angle = 0;
    int maxBrightness = limit;

    
    float duty = M_PI; // how long is the "on" part of the pulseMS length blinking cycle in as angle (M_PI = 50% duty).

    void updateAnimation()
    {

        steps = pulseMS / stepDelayMS; 
        stepAngle = (2 * M_PI) / steps;

        // adjust to next integer step according to new step width, avoids
        // wrong animations
        angle = ceilf(angle / stepAngle) * stepAngle;
    }

public:
    /**
     * @brief Construct a new Indicator Light object
     *
     * @param gpio which GPIO the led is connected to
     * @param inversePWM set to true if LED is active at LOW output level
     * @param _BITS number of bits for pwm controller, up to 16 is permitted, 12 is default
     */
    IndicatorLight(int gpio, bool inversePWM = false, int _BITS = 12);

    void setState(IndicatorState state) 
    {
        m_state = state;
        xTaskNotify(m_taskHandle, 1, eSetBits);
    }


    IndicatorState getState() 
    {
        return m_state;
    }

    void setPulseTime(int pulseMS)
    {
        this->pulseMS = pulseMS;
        updateAnimation();
    }

    float getStepAngle() { return stepAngle; }
    int getStepDelayMS() { return stepDelayMS; };
    bool getInversePWM() { return inversePWM; }

    /**
     * @brief Get the current animation angle step (1 animation cycle = 2 * PI), each call
     * increments returned angle  by stepAngle until a value > 2 * PI is reached, then it will start again.
     * 
     * @return float 
     */
    float getAngle();
    int getMaxBrightness() { return maxBrightness; }
    void setMaxBrightness(int mb) { maxBrightness=mb;}
    void setDutyPercent(float percentOn = 50) { duty = percentOn * (2*M_PI) /100.0; }
    int getDutyAngle() { return duty; }
};