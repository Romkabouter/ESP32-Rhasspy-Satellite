#include <Arduino.h>
#include "IndicatorLight.h"

// This task does all the heavy lifting for our application
void indicatorLedTask(void *param)
{
    IndicatorLight *indicator_light = static_cast<IndicatorLight *>(param);
    const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
    while (true)
    {
        // wait for someone to trigger us
        uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
        if (ulNotificationValue > 0)
        {
            switch (indicator_light->getState())
            {
            case OFF:
            {
                ledcWrite(0, indicator_light->getInversePWM() ? indicator_light->limit : 0);
                break;
            }
            case ON:
            {
                ledcWrite(0, indicator_light->getInversePWM() ? indicator_light->limit - indicator_light->getMaxBrightness() : indicator_light->getMaxBrightness());
                break;
            }
            case PULSING:
            {
                // do a nice pulsing effect
                while (indicator_light->getState() == PULSING)
                {
                    int brightness = indicator_light->getMaxBrightness() * (0.5 * cos(indicator_light->getAngle()) + 0.5);
                    int pwm = indicator_light->getInversePWM() ? indicator_light->limit - brightness : brightness;
                    ledcWrite(0, pwm);
                    vTaskDelay(indicator_light->getStepDelayMS() / portTICK_PERIOD_MS);
                }
                break;
            }
            case BLINKING:
            {
                // do a nice pulsing effect
                while (indicator_light->getState() == BLINKING)
                {
                    int brightness = (indicator_light->getAngle() > indicator_light->getDutyAngle()) ? 0 : indicator_light->getMaxBrightness();
                    int pwm = indicator_light->getInversePWM() ? indicator_light->limit - brightness : brightness;
                    ledcWrite(0, pwm);
                    vTaskDelay(indicator_light->getStepDelayMS() / portTICK_PERIOD_MS);
                }
                break;
            }
            }
        }
    }
}

IndicatorLight::IndicatorLight(int gpio, bool _inversePWM, int _BITS) : BITS(_BITS), inversePWM(_inversePWM)
{
    // use the build in LED as an indicator - we'll set it up as a pwm output so we can make it glow nicely
    ledcSetup(0, 10000, BITS);
    ledcAttachPin(gpio, 0);

    // start off with the light off
    ledcWrite(0, inversePWM? limit : 0);
    
    m_state = OFF;
    updateAnimation();
    // set up the task for controlling the light
    xTaskCreate(indicatorLedTask, "Indicator LED Task", 4096, this, 1, &m_taskHandle);
}


float IndicatorLight::getAngle()
{
    float retval = angle;
    angle += getStepAngle();
    // keep angle from growing outside 2*M_PI
    // as our steps are always integer divides of 2 * M_PI
    // we can set the first step above 2 * M_PI to stepAngle
    angle = (angle > 2 * M_PI) ? (angle - 2 * M_PI) : angle;
    return retval;
}