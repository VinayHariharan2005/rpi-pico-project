#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"


#define UART_ID uart0
#define BAUD_RATE 115200

#define UART_TX_PIN 0
#define UART_RX_PIN 1
uint a,b;
// Buffer to hold the character to echo back
volatile char tx_char,tx_char1;
volatile bool tx_ready = false;
bool t1=true;
bool t0=true;
#define divcalc(f) ((133000000/f)/10012)
#define LOL_PIN 0

bool tp1 = true ;

// UART interrupt handler
void on_uart_rx() {
    // While RX FIFO has data
    if (uart_is_readable(UART_ID)) {
        // Read one character
        char ch = uart_getc(UART_ID);

        // Store to buffer and enable TX interrupt
        tx_char = ch;
        tx_ready = true;

        // Enable TX interrupt to send this character
        uart_set_irq_enables(UART_ID, false, false);
    }
    
    
}


// Interrupt Service Routine (ISR) for Timer Alarm 0

void on_alarm_0() {
    // Clear the alarm 0 interrupt
    hw_clear_bits(&timer_hw->intr, 1u << 0);
    tp1=false;
    pwm_set_enabled(a,false);
    hw_clear_bits(&timer_hw->inte, 1u << 0);
}

char recieve_char(){
    char lol;
    uart_set_irq_enables(UART_ID, true, false);
    while (!tx_ready) {
        // Main loop does nothing; all UART is handled in ISR
        tight_loop_contents();
    }
    tx_ready=false;
    lol=tx_char;
    return lol;
}
int main() {
    // Initialize UART
    
    
    timer_hardware_alarm_claim(timer_hw,0);
  
    
    // Set GPIO function
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_fifo_enabled(UART_ID, true);
    // Enable FIFO
    

    // Set up the IRQ handlers
    irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
    irq_set_enabled(UART0_IRQ, true);

    gpio_set_function(3,GPIO_FUNC_PWM);
    a=pwm_gpio_to_slice_num(3);
    b=pwm_gpio_to_channel(3);
    pwm_set_wrap(a,10011);
    pwm_set_chan_level(a,b,5006);

    // Enable RX interrupt only at first
   
    // Print welcome message
    uart_puts(UART_ID, "Press on for 1 for x and 2 for y");
    int f;
    while(true){
    tx_char1=recieve_char();

    if(tx_char1=='1'){
        f=240;
        pwm_set_clkdiv(a,divcalc(f));
        pwm_set_enabled(a,true);
        timer_hw->alarm[0]=timer_hw->timerawl+3000000;
        irq_set_exclusive_handler(TIMER_IRQ_0,on_alarm_0);
        irq_set_enabled(TIMER_IRQ_0,true);
        hw_set_bits(&timer_hw->inte,1u<<0);
        hw_set_bits(&timer_hw->inte,1u<<1);
        while(tp1){
            tight_loop_contents(); 
        }
        tp1=true;
        uart_puts(UART_ID,"playing Sa"); 

    }
    else if(tx_char1=='2'){
        f=450;
        pwm_set_clkdiv(a,divcalc(f));
        pwm_set_enabled(a,true);
        timer_hw->alarm[0]=timer_hw->timerawl+3000000;
        irq_set_exclusive_handler(TIMER_IRQ_0,on_alarm_0);
        irq_set_enabled(TIMER_IRQ_0,true);
        hw_set_bits(&timer_hw->inte,1u<<0);
       
        while(tp1){
            tight_loop_contents(); 
        }
        tp1=true;
        uart_puts(UART_ID,"playing Dha"); 
    }
}
}

