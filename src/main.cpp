#include <Arduino.h>
#include <Ticker.h>
#include "driver/twai.h"

// Default for ESP32 CAN
#define CAN_TX		5
#define CAN_RX		4

extern bool can_init();
extern bool can_write(twai_message_t* frame);
extern bool can_read(twai_message_t* frame);
extern uint32_t can_tx_queue();
extern uint32_t can_rx_queue();

extern void t_1s_job();
extern void disp_setup();
extern void disp_show();
extern void ble_setup();
extern void str_hex_arr8 ();
//extern void ble_handle_tx();

void sendObdFrame();
void handle_rx_message(twai_message_t& message);

unsigned long previousMillis = 0;
unsigned long interval = 5000;  //5 sec.
bool flag_cycle = false;

String dev_name = "ODB2-BLE-GATE"; //name of BLE service
uint8_t send_content[] = { //OBD2 пакет (всегда 8 байт)
  2,      //количество значимых байт во фрейме
  1,      //Service OBD2
  5,      //PID OBD2
  0xAA,0xAA,0xAA,0xAA,0xAA  //Best to use 0xAA (0b10101010) instead of 0
};
char str_hex[24]; //буфер для перевода в HEX
String formatted_time = "00:00:00"; // "hh:mm:ss"
String ds1 = ""; //дисплей-строка 1
String ds2 = ""; //дисплей-строка 2

Ticker hTicker;  //Для UpTime
twai_message_t rxFrame; //CanFrame = twai_message_t (для приема фреймов)


void setup() {

  Serial.begin(115200);

  //internal led
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH); //OFF


  delay(10000);  //10 sec

    //Старт CAN без параметров
    if(can_init()) {
        Serial.println("CAN bus started!");
    } else {
        Serial.println("CAN bus failed!");
    }

  disp_setup(); //OLED SSD1306 63X32
  ble_setup(); //start BLE server

  //инициализация прерывания 1 sec. (дисплей..)
  hTicker.attach_ms(1000, t_1s_job); 

  Serial.println("----------------Start Info-----------------");
  Serial.printf("Total heap:\t%d \r\n", ESP.getHeapSize());
  Serial.printf("Free heap:\t%d \r\n", ESP.getFreeHeap());
 
//Serial.println("I2C_SDA= "+String(SDA));
//Serial.println("I2C_SCL= "+String(SCL));
Serial.println("-----------------------------------------");
 
}

void loop() {

//Передаем пакеты CAN..
unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >=interval && flag_cycle) {
    if(can_tx_queue()<3){
      sendObdFrame(); // Передача запроса по CAN шине.(obd2 pid=5)
    } else {
      Serial.println("CAN BUS DOWN..");
    }
    previousMillis = currentMillis;
  }

  // Принимаем пакеты CAN..
  if(can_read(&rxFrame)) {
      // Comment out if too many frames
      handle_rx_message(rxFrame);
      if(rxFrame.identifier == 0x7E8 && rxFrame.data[1]==0x41 && rxFrame.data[2]==5) {   // Standard OBD2 frame responce ID=0x7E8
          Serial.printf("Collant temp: %3d°C \r\n", rxFrame.data[3] - 40); // Convert to °C
      }
  }
}

//Посылаем запрос по шине CAN для Service=1 (параметр=PID)
void sendObdFrame() {
	twai_message_t obdFrame = { 0 };  //структура twai_messae_t инициализируем нулями !!!
	obdFrame.identifier = 0x7DF; // Общий адрес всех ECU;
	obdFrame.extd = 0; //формат 11-бит
	obdFrame.data_length_code = 8; //OBD2 frame - всегда 8 байт !
  for(int i=0;i<8;i++)
	        obdFrame.data[i] = send_content[i];  //формируем пакет obd2
    //передача пакета
    if(can_write(&obdFrame)){  // timeout defaults to 1 ms
      str_hex_arr8(); //конвертация в HEX из send_content[]
      Serial.printf("%s --Frame sent: %03X tx_queue: %d %s\r\n",
                formatted_time ,obdFrame.identifier,can_tx_queue(),str_hex);
    } else {
      Serial.printf("%s tx_queue: %d\r\n",formatted_time,can_tx_queue());
    }
}

//Распечатка содержимого принятого пакета CAN
void handle_rx_message(twai_message_t& message) {
  Serial.print(formatted_time+" --Frame received: ");
  if (message.extd) {
    Serial.print("CAX: 0x");
  } else {
    Serial.print("CAN: 0x");
  }
  Serial.print(message.identifier, HEX);
  //Serial.print(" (");
  Serial.print(" rx_queue: "+String(can_rx_queue()));
  //Serial.print(")");
  Serial.print(" [");
  Serial.print(message.data_length_code, DEC);
  Serial.print("] <");
  for (int i = 0; i < message.data_length_code; i++) {
    if (i != 0) Serial.print(":");
    Serial.print(message.data[i], HEX);
  }
  Serial.println(">");
}