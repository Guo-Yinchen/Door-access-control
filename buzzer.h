#ifndef __BUZZER_H__
#define __BUZZER_H__

#include "main.h"

/**
 * @brief STM32 蜂鸣器类
 *
 * 支持低电平触发和高电平触发：
 * - 低电平触发：输出 RESET 时蜂鸣器响
 * - 高电平触发：输出 SET 时蜂鸣器响
 */
class Buzzer
{
public:
    /**
     * @brief 构造函数
     * @param port GPIO 端口，例如 GPIOB
     * @param pin GPIO 引脚，例如 GPIO_PIN_8
     * @param lowTrigger true=低电平触发，false=高电平触发
     */
    Buzzer(GPIO_TypeDef* port, uint16_t pin, bool lowTrigger = true);

    /**
     * @brief 初始化蜂鸣器，默认关闭
     */
    void init();

    /**
     * @brief 打开蜂鸣器
     */
    void on();

    /**
     * @brief 关闭蜂鸣器
     */
    void off();

    /**
     * @brief 切换蜂鸣器状态
     */
    void toggle();

    /**
     * @brief 设置蜂鸣器状态
     * @param enable true=打开，false=关闭
     */
    void set(bool enable);

    /**
     * @brief 获取蜂鸣器当前状态
     * @return true=打开，false=关闭
     */
    bool isOn() const;

private:
    GPIO_TypeDef* m_port;
    uint16_t m_pin;
    bool m_lowTrigger;
    bool m_isOn;

    void writePin(GPIO_PinState state);
};

#endif /* __BUZZER_H__ */
