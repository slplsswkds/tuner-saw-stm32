#include "tuner.h"

void blinkTimesWithDelay(int times, int delay)
{
    for (int i = 0; i < times * 2; i++)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        HAL_Delay(delay);
    }
}

inline bool isWakeupFromStandby()
{
    return __HAL_PWR_GET_FLAG(PWR_FLAG_WU);
}

int main(void)
{
    HAL_Init();
    SystemClockConfig();
    MxGpioInit();
    HAL_Delay(100);

    if (isWakeupFromStandby())
    {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
        blinkTimesWithDelay(2, 500);
    }
    else
    {
        blinkTimesWithDelay(5, 100);
    }

    while (1)
    {

    }
}
