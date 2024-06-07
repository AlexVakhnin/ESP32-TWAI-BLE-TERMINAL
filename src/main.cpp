#include <Arduino.h>
#include <Ticker.h>

//#include <BLEDevice.h>
//#include <BLEServer.h>
//#include <BLEUtils.h>
//#include <BLE2902.h>


//#define LOG_TWAI log_e
//#define LOG_TWAI_TX log_e
//#define LOG_TWAI_RX log_e

#include <ESP32-TWAI-CAN.hpp>

// Default for ESP32 CAN
#define CAN_TX		5
#define CAN_RX		4
CanFrame rxFrame; //CanFrame = twai_message_t (для приема фреймов)

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

  //Драйвер CAN шины
	ESP32Can.setPins(CAN_TX, CAN_RX);	
  ESP32Can.setRxQueueSize(10);
	ESP32Can.setTxQueueSize(10);
  ESP32Can.setSpeed(ESP32Can.convertSpeed(500));

    //Старт CAN без параметров
    if(ESP32Can.begin()) {
        Serial.println("CAN bus started!");
    } else {
        Serial.println("CAN bus failed!");
    }

/* 
    //Старт CAN с параметрами
    if(ESP32Can.begin(ESP32Can.convertSpeed(500), CAN_TX, CAN_RX, 10, 10)) {
        Serial.println("CAN bus started!");
    } else {
        Serial.println("CAN bus failed!");
    }
*/

  disp_setup(); //OLED SSD1306 63X32
  ble_setup(); //start BLE server

  //инициализация прерывания 1 sec. (дисплей..)
  hTicker.attach_ms(1000, t_1s_job); 

  Serial.println("----------------Start Info-----------------");
  Serial.printf("Total heap:\t%d \r\n", ESP.getHeapSize());
  Serial.printf("Free heap:\t%d \r\n", ESP.getFreeHeap());
 
//Serial.println("I2C_SDA= "+String(SDA));
//Serial.println("I2C_SCL= "+String(SCL));
//Serial.println("SPI_SCK= "+String(SCK));
//Serial.println("SPI_MOSI= "+String(MOSI));
//Serial.println("SPI_MISO= "+String(MISO));
//Serial.println("SPI_SS= "+String(SS));
Serial.println("-----------------------------------------");
 
}

void loop() {

//Передаем пакеты CAN..
unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >=interval) {

    sendObdFrame(5); // Передача запроса по CAN шине.
    //ble_handle_tx();
    previousMillis = currentMillis;
  }

  // Принимаем пакеты CAN..
  if(ESP32Can.readFrame(rxFrame, 1000)) {
      // Comment out if too many frames
      Serial.printf("Received frame: %03X  \r\n", rxFrame.identifier);
      if(rxFrame.identifier == 0x7E8) {   // Standard OBD2 frame responce ID
          //Serial.printf("Collant temp: %3d°C \r\n", rxFrame.data[3] - 40); // Convert to °C
          handle_rx_message(rxFrame);

      }
  }
  //ble_handle_tx();
  //ble_handle();  //обработка сообщений от BLE connect/disconnect
}

//Посылаем запрос по шине CAN для Service=1 (параметр=PID)
void sendObdFrame(uint8_t obdId) {
	CanFrame obdFrame = { 0 };  //структура twai_messae_t инициализируем нулями !!!
	obdFrame.identifier = 0x7DF; // Общий адрес всех ECU;
	obdFrame.extd = 0; //формат 11-бит
	obdFrame.data_length_code = 8; //OBD2 frame - всегда 8 байт !

  //obdFrame.ss = 1; //DEBUG Single Shot Transmission (UDP type, без подтверждения..)
  //obdFrame.self = 1; //DEBUG..??????????

	obdFrame.data[0] = 2; //количество значимых байт во фрейме
	obdFrame.data[1] = 1; //Service / Mode OBD2
	obdFrame.data[2] = obdId; //PID OBD2
	obdFrame.data[3] = 0xAA;    // Best to use 0xAA (0b10101010) instead of 0
	obdFrame.data[4] = 0xAA;    // CAN works better this way as it needs
	obdFrame.data[5] = 0xAA;    // to avoid bit-stuffing
	obdFrame.data[6] = 0xAA;
	obdFrame.data[7] = 0xAA;
    // Accepts both pointers and references 
    if(ESP32Can.writeFrame(obdFrame)){  // timeout defaults to 1 ms
      Serial.printf("%s Frame sent: %03X tx_queue: %d\n",formatted_time ,obdFrame.identifier,ESP32Can.inTxQueue());
    } else {
      Serial.printf("%s tx_queue: %d\n",formatted_time,ESP32Can.inTxQueue());
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
  Serial.print(" (");
  Serial.print(message.identifier, DEC);
  Serial.print(")");
  Serial.print(" [");
  Serial.print(message.data_length_code, DEC);
  Serial.print("] <");
  for (int i = 0; i < message.data_length_code; i++) {
    if (i != 0) Serial.print(":");
    Serial.print(message.data[i], HEX);
  }
  Serial.println(">");
}