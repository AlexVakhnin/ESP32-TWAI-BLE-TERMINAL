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


twai_message_t rxFrame; //CanFrame = twai_message_t (для приема фреймов)

//Для UpTime
Ticker hTicker;

//Function Declaration
extern void t_1s_job();
extern void disp_setup();
extern void disp_show();
extern void ble_setup();
//extern void ble_handle();
extern void ble_handle_tx();
void sendObdFrame(uint8_t obdId);
void handle_rx_message(twai_message_t& message);

//Global Variables
unsigned long previousMillis = 0;
unsigned long interval = 5000;  //5 sec.

unsigned long sFreeMem; //хранит колич. свободной RAM (куча)
unsigned long sUpTime = 0; //счетчик UpTime, в начале = 0 sec
unsigned long isec = 0; //uptime: sec
unsigned long imin = 0; //uptime: min
unsigned long ihour = 0; //uptime: hour
unsigned long iday = 0; //uptime: day
boolean led_state = false;
//boolean can_state = false;  //DEBUG

String ds1 = ""; //дисплей-строка 1
String ds2 = ""; //дисплей-строка 2
String formatted_time = "--:--:--"; // "hh:mm:ss"



void setup() {

  Serial.begin(115200);

  //internal led
  pinMode(8, OUTPUT);
  digitalWrite(8, LOW);


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
  if (currentMillis - previousMillis >=interval) {
    if(can_tx_queue()<3){
      sendObdFrame(5); // Передача запроса по CAN шине.
    } else {
      Serial.println("CAN BUS DOWN..");
    }
    //ble_handle_tx();
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
void sendObdFrame(uint8_t obdId) {
	twai_message_t obdFrame = { 0 };  //структура twai_messae_t инициализируем нулями !!!
	obdFrame.identifier = 0x7DF; // Общий адрес всех ECU;
	obdFrame.extd = 0; //формат 11-бит
	obdFrame.data_length_code = 8; //OBD2 frame - всегда 8 байт !

  //obdFrame.ss = 1; //DEBUG Single Shot Transmission (UDP type, без подтверждения..)
  //obdFrame.self = 1; //DEBUG..??????????

	obdFrame.data[0] = 2; //количество значимых байт во фрейме
	obdFrame.data[1] = 1;     //Service OBD2
	obdFrame.data[2] = obdId; //PID OBD2
	obdFrame.data[3] = 0xAA;    // Best to use 0xAA (0b10101010) instead of 0
	obdFrame.data[4] = 0xAA;    // CAN works better this way as it needs
	obdFrame.data[5] = 0xAA;    // to avoid bit-stuffing
	obdFrame.data[6] = 0xAA;
	obdFrame.data[7] = 0xAA;
    // Accepts both pointers and references 
    if(can_write(&obdFrame)){  // timeout defaults to 1 ms
      Serial.printf("%s --Frame sent: %03X tx_queue: %d\r\n",formatted_time ,obdFrame.identifier,can_tx_queue());
    } else {
      Serial.printf("%s tx_queue: %d\r\n",formatted_time,can_tx_queue());
    }
}

//Распечатка содержимого принятого пакета CAN
void handle_rx_message(twai_message_t& message) {

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