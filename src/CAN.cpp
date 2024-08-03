#include <Arduino.h>
#include "driver/twai.h"

int8_t tx = 5;  //pin для tx
int8_t rx = 4;  //pin для rx
uint16_t txQueueSize = 10; //очередь tx
uint16_t rxQueueSize = 20;  //очередь rx
twai_status_info_t status;  //структура для получения состояния CAN


//инициализация контроллера CAN шины
bool can_init(){
    bool rez=false;

    twai_general_config_t g_config = {.mode = TWAI_MODE_NORMAL, .tx_io = (gpio_num_t) tx, .rx_io = (gpio_num_t) rx, \
                                        .clkout_io = TWAI_IO_UNUSED, .bus_off_io = TWAI_IO_UNUSED,      \
                                        .tx_queue_len = txQueueSize, .rx_queue_len = rxQueueSize,       \
                                        .alerts_enabled = TWAI_ALERT_NONE,  .clkout_divider = 0,        \
                                        .intr_flags = ESP_INTR_FLAG_LEVEL1};
    twai_timing_config_t  t_config = TWAI_TIMING_CONFIG_500KBITS();  //500 kb/s
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    //Install TWAI driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        Serial.println("Driver installed");
    } else {
        Serial.println("Failed to install driver");
    }

    //Start TWAI driver
    if (twai_start() == ESP_OK) {
        Serial.println("Driver started");
        rez = true;
    } else {
        Serial.println("Failed to start driver");
    }
    return rez;
}

//передача пакета по CAN шине
bool can_write(twai_message_t* frame){
    bool rez = false;
    if((frame) && twai_transmit(frame, pdMS_TO_TICKS(1)) == ESP_OK) {
        rez = true;
    }
    return rez;
}

//прием пакета по CAN шине
bool can_read(twai_message_t* frame){
    bool rez = false;
    if((frame) && twai_receive(frame, pdMS_TO_TICKS(1000)) == ESP_OK) {
        rez = true;
    }
    return rez;
}
//заполнение структуры status
bool getStatusInfo() {
    return ESP_OK == twai_get_status_info(&status);
}

//состояние очереди CAN на передачу
uint32_t can_tx_queue() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.msgs_to_tx;
    }
    return ret;
};

//состояние очереди CAN на прием
uint32_t can_rx_queue() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.msgs_to_rx;
    }
    return ret;
};