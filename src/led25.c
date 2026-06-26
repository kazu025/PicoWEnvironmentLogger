#include "led25.h"

#include <stdio.h>
#include "pico/stdlib.h"

#if defined(CYW43_WL_GPIO_LED_PIN)
#include "pico/cyw43_arch.h"
#endif
/* --- LED 初期化 --- */
int led_init(void){
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
	return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
	/*
	 * CYW43_WL_GPIO_LED_PIN は cyw43_arch_init() 内で初期化されるので
	 * ここでは何もしない
	 */
	return PICO_OK;
#else
	printf("!!! LED pin not defined\n");
	return -1;
#endif
}
/* toggle on/off */
void led_sw(void){
	static bool flag = true;
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, flag);
#elif defined(CYW43_WL_GPIO_LED_PIN)
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, flag);
#endif
	flag = !flag;
}
/* led on/off */
void led_onoff(bool flag){
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, flag);
#elif defined(CYW43_WL_GPIO_LED_PIN)
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, flag);
#endif
}