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
extern String hex_arr8(uint8_t arr[]);
extern void ble_handle_tx(String str);

void sendObdFrame();
String str_obd2_ext(twai_message_t& message);
String str_obd2_min(uint8_t arr[]);

unsigned long previousMillis = 0;
unsigned long interval = 2000;  //интервал цикла запросов obd2
bool flag_cycle = false;    //в цикле посылать obd2 запрос
bool flag_ext_format = false;   //формат отображения входящего пакета obd2
int collant = 0; //температура двигателя
int can_counter = 0; //счетчик принятых CAN пакетов

String dev_name = "ODB2-BLE-GATE"; //name of BLE service
uint8_t send_content[] = { //OBD2 пакет (всегда 8 байт)
  2,      //количество значимых байт во фрейме
  1,      //Service OBD2
  5,      //PID OBD2
  0xAA,0xAA,0xAA,0xAA,0xAA  //Best to use 0xAA (0b10101010) instead of 0
};
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
  Serial.println("-------------------------------------------");
 
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
      can_counter++;
      String str_min = str_obd2_min(rxFrame.data)+"\r\n"; //min format
      String str_ext = str_obd2_ext(rxFrame)+"\r\n"; //ext format
      Serial.print("--["+String(ESP.getFreeHeap())+"] ["+String(can_counter)+"] "+str_ext); //debug

      if(!flag_ext_format) ble_handle_tx(str_min); //to BLE min format
      else ble_handle_tx(str_ext); //to BLE ext format
      if(rxFrame.identifier == 0x7E8 && rxFrame.data[1]==0x41 && rxFrame.data[2]==5) {   //ID=0x7E8
            //Serial.printf("Collant temp: %3d°C \r\n", rxFrame.data[3] - 40); // Convert to °C
            collant = rxFrame.data[3] - 40;
      }     
  }

}

//Посылаем запрос obd2 по шине CAN, пакет берем из send_content[]
void sendObdFrame() {
  twai_clear_receive_queue();  //обнуляем очередь на прием пакетов CAN
	twai_message_t obdFrame = { 0 };  //структура twai_messae_t инициализируем нулями !!!
	obdFrame.identifier = 0x7DF; // Общий адрес всех ECU;
	obdFrame.extd = 0; //формат 11-бит
	obdFrame.data_length_code = 8; //OBD2 frame - всегда 8 байт !
  for(int i=0;i<8;i++)
	        obdFrame.data[i] = send_content[i];  //формируем пакет obd2
    //передача пакета
    if(can_write(&obdFrame)){  // timeout defaults to 1 ms
        //Serial.printf("%s --Frame sent: %03X tx_queue: %d %s\r\n",
        //        formatted_time ,obdFrame.identifier,can_tx_queue(),hex_arr8(send_content).c_str());
    } else {
      Serial.printf("%s --Frame sent: ERROR.. tx_queue: %d\r\n",formatted_time,can_tx_queue());
    }
}

//принятый пакет CAN в строку в расширенном формате
String str_obd2_ext(twai_message_t& message){
  String rez = "";
  char buf[3];
  if (message.extd) rez+="CAX: 0x";
  else rez+="CAN: 0x";
  rez+=String(message.identifier,HEX);
  int n=message.data_length_code;
  rez+=" [";rez+=String(n);rez+="] ";
  for (int i=0; i < n; i++){
    if (i != 0)  rez+=" ";
    sprintf( buf , "%.2X", message.data[i] );
    rez+=String(buf);
  } 
  return rez;
}
//принятый пакет CAN в строку в минимальном формате
String str_obd2_min(uint8_t arr[]){
    String rez = "";
    int n= arr[0];  //количество значащих байт в пакете(3)
    if(n>7) n=7; //фильтр по длине
    for(int i=1;i<n+1;i++){ //[1,2,3]
        char buf[4+3]; //буфер для перевода в HEX
        if(i != n) sprintf( buf , "%.2X ", arr[i] ); //кроме последнего(3)
        //else sprintf( buf , "%.2X \r\r>", arr[i] );  //последний c добавкой 0D:0D:3E
        else sprintf( buf , "%.2X", arr[i] );  //последний , без пробела
        rez+=String(buf);  //добавляем в результат
    }
    return rez;
}
