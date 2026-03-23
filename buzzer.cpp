#include "buzzer.h"

Buzzer::Buzzer(GPIO_TypeDef* port, uint16_t pin, bool lowTrigger)
    : m_port(port), m_pin(pin), m_lowTrigger(lowTrigger), m_isOn(false)
{
}

void Buzzer::writePin(GPIO_PinState state)
{
    HAL_GPIO_WritePin(m_port, m_pin, state);
}

void Buzzer::init()
{
    off();
}

void Buzzer::on()
{
    writePin(m_lowTrigger ? GPIO_PIN_RESET : GPIO_PIN_SET);
    m_isOn = true;
}

void Buzzer::off()
{
    writePin(m_lowTrigger ? GPIO_PIN_SET : GPIO_PIN_RESET);
    m_isOn = false;
}

void Buzzer::toggle()
{
    if (m_isOn)
    {
        off();
    }
    else
    {
        on();
    }
}

void Buzzer::set(bool enable)
{
    if (enable)
    {
        on();
    }
    else
    {
        off();
    }
}

bool Buzzer::isOn() const
{
    return m_isOn;
}
