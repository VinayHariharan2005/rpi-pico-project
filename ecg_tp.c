#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include <stdio.h>

#define WIFI_SSID     "OnePlus 12R"
#define WIFI_PASSWORD "p3rtw59w"
#define SERVER_IP     "192.168.50.25"
#define SERVER_PORT   5005
#define BUFFER_SIZE   256
#define SAMPLE_INTERVAL_US 100  // 10 kHz
#define BATCH_SIZE 20

#define SPI_PORT spi0
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_SCK  2
#define PIN_MOSI 3

volatile uint16_t adc_buffer[BUFFER_SIZE];
volatile uint i_adc = 0;
volatile uint i1=0;
int dma_chan;
ip_addr_t server_ip;
struct udp_pcb *udp_socket;

uint8_t spi_buf[2];
uint16_t sample_batch[BATCH_SIZE];
int batch_index = 0;

void dac_write(uint16_t value) {
    uint16_t packet = 0x3000 | (value & 0x0FFF);
    spi_buf[0] = (packet >> 8) & 0xFF;
    spi_buf[1] = packet & 0xFF;
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, spi_buf, 2);
    gpio_put(PIN_CS, 1);
}

void dma_handler() {
    i_adc = (i_adc + 1) % BUFFER_SIZE;
    dma_channel_acknowledge_irq0(dma_chan);
    dma_channel_set_read_addr(dma_chan, &adc_hw->fifo, false);
    dma_channel_set_write_addr(dma_chan, &adc_buffer[i_adc], false);
    dma_channel_set_trans_count(dma_chan, 1, true);
}

void setup_adc_dma() {
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(50.0f); // ~5kHz sampling (adjust if needed)
    adc_run(true);

    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(
        dma_chan, &cfg,
        &adc_buffer[0],
        &adc_hw->fifo,
        1,
        false
    );

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_start(dma_chan);
}

int64_t send_udp_callback(alarm_id_t id, void *user_data) {
    uint16_t val = adc_buffer[i1];
    i1 = (i1 + 1) % BUFFER_SIZE;
    dac_write(val);
    sample_batch[batch_index++] = val;

    if (batch_index >= BATCH_SIZE) {
        char payload[BATCH_SIZE * 2];
        for (int i = 0; i < BATCH_SIZE; ++i) {
            payload[2 * i] = sample_batch[i] >> 8;
            payload[2 * i + 1] = sample_batch[i] & 0xFF;
        }

        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(payload), PBUF_RAM);
        if (p != NULL) {
            memcpy(p->payload, payload, sizeof(payload));
            udp_sendto(udp_socket, p, &server_ip, SERVER_PORT);
            pbuf_free(p);
        }

        batch_index = 0;
    }

    return SAMPLE_INTERVAL_US;
}

void setup_spi_dac() {
    spi_init(SPI_PORT, 10 * 1000 * 1000);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
}

int main() {
    stdio_init_all();
    uart_init(uart0, 115200);
    gpio_set_function(12, GPIO_FUNC_UART);
    gpio_set_function(13, GPIO_FUNC_UART);

    if (cyw43_arch_init()) {
        uart_puts(uart0, "Wi-Fi init failed\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        uart_puts(uart0, "Wi-Fi connect failed\n");
        return 1;
    }
    uart_puts(uart0, "Wi-Fi connected\n");

    if (!ipaddr_aton(SERVER_IP, &server_ip)) {
        uart_puts(uart0, "Invalid IP address\n");
        return 1;
    }

    udp_socket = udp_new();
    if (!udp_socket) {
        uart_puts(uart0, "Failed to create UDP socket\n");
        return 1;
    }

    setup_spi_dac();
    setup_adc_dma();
    add_alarm_in_us(SAMPLE_INTERVAL_US, send_udp_callback, NULL, true);
    uart_puts(uart0, "Started UDP transmission...\n");

    while (true) {
        tight_loop_contents();
    }
}
