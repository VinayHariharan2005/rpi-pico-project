#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
uint a,b;
int64_t alarm_callback(alarm_id_t id, void *user_data) {
    // Put your timeout handler code in here
    return 0;
}
//playing Sa,Ga,Pa and Ni
int freq[4][2]={{240,500},{300,1000},{360,1000},{450,1000}};
// Use onboard LED on GPIO 25
//time in micro seconds
#define divcalc(f) ((133000000/f)/10012)
#define LOL_PIN 0

bool tp1 = true ;

// Interrupt Service Routine (ISR) for Timer Alarm 0

void on_alarm_0() {
    // Clear the alarm 0 interrupt
    hw_clear_bits(&timer_hw->intr, 1u << 0);
    tp1=false;
    pwm_set_enabled(a,false);
    hw_clear_bits(&timer_hw->inte, 1u << 0);
}

int main() {
    int f;
    uint32_t now;
    stdio_init_all();
    gpio_set_function(3,GPIO_FUNC_PWM);
    a=pwm_gpio_to_slice_num(3);
    b=pwm_gpio_to_channel(3);
    pwm_set_wrap(a,10011);
    pwm_set_chan_level(a,b,5006);
    // Claim alarm 0 and alarm1 to prevent conflicts
    timer_hardware_alarm_claim(timer_hw,0);
    while(true){
    for (int i = 0; i < 4; i++){
    
    // Set the alarm 0 to fire 2 seconds from now
    now = timer_hw->timerawl;
    timer_hw->alarm[0] = now + (freq[i][1]*1000);  // 2,000,000 us = 2 s

    // Enable interrupt for alarm 0
    hw_set_bits(&timer_hw->inte, 1u << 0);
    
    // Set the handler and enable IRQ
    irq_set_exclusive_handler(TIMER_IRQ_0, on_alarm_0);
    irq_set_enabled(TIMER_IRQ_0, true);
    f=freq[i][0];
    pwm_set_clkdiv(a,divcalc(f));
    pwm_set_enabled(a,true);
    // Main loop: do nothing, wait for interrupt
    while (tp1) {
        tight_loop_contents();  // Low-power idle
    }
    tp1=true;
    }
}
}