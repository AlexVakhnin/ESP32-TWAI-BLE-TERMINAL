#include <Arduino.h>
#include "driver/twai.h"

// Default for ESP32 CAN
#define CAN_TX		5
#define CAN_RX		4

extern bool can_init();
extern bool can_write(twai_message_t* frame);
extern bool can_read(twai_message_t* frame);
extern uint32_t can_tx_queue();
extern uint32_t can_rx_queue();

//extern void disp_setup();
//extern void disp_show();
extern void ble_setup();
extern String hex_arr8(uint8_t arr[]);
extern void ble_handle_tx(String str);

void sendObdFrame();
String str_obd2_ext(twai_message_t& message);
String str_obd2_min(twai_message_t& message);

unsigned long previousMillis = 0;
unsigned long interval = 1500;  //интервал цикла запросов obd2
bool flag_cycle = false;    //в цикле посылать obd2 запрос
bool flag_ext_format = false;   //формат отображения входящего пакета obd2
int collant = 0; //температура двигателя
int can_counter = 0; //счетчик принятых CAN пакетов
int sent_counter = 0; //счетчик при передаче CAN пакетов
//bool flag_cycle_err = false; //нет ответа CAN в цикле
int filter = 0x7E8; //аппаратный фильтр(0x7E8),  0 - ACCEPT_ALL

String dev_name = "ODB2-BLE-GATE"; //name of BLE service
uint8_t send_content[] = { //OBD2 пакет (всегда 8 байт)
  2,      //количество значимых байт во фрейме
  1,      //Service OBD2
  5,      //PID OBD2
  0xAA,0xAA,0xAA,0xAA,0xAA  //Best to use 0xAA (0b10101010) instead of 0
};
//String ds1 = ""; //дисплей-строка 1
//String ds2 = ""; //дисплей-строка 2

twai_message_t rxFrame; //CanFrame = twai_message_t (для приема фреймов)


void setup() {

  Serial.begin(115200);

  //internal led
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH); //OFF

  delay(10000);  //10 sec

  ble_setup(); //start BLE server + load from NVRAM

    //Старт CAN без параметров
    if(can_init()) {
        Serial.println("CAN bus started!");
    } else {
        Serial.println("CAN bus failed!");
    }

  Serial.println("----------------Start Info-----------------");
  Serial.printf("Total heap:\t%d \r\n", ESP.getHeapSize());
  Serial.printf("Free heap:\t%d \r\n", ESP.getFreeHeap());
  //Serial.println("I2C_SDA= "+String(SDA));
  //Serial.println("I2C_SCL= "+String(SCL));
  Serial.println("-------------------------------------------");
 
}

void loop() {

//Циклическая передача пакетов CAN..
unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >=interval && flag_cycle) {
    sent_counter++; //считаем переданные пакеты "01 05"
    if(sent_counter>2){ble_handle_tx("CYCLE ERROR..\r\n");} //DEBUG
    if(can_tx_queue()<3){
      sendObdFrame(); // Передача запроса по CAN шине.(obd2 "01 05")
    } else {
      Serial.println("CAN BUS DOWN..");
    }
    previousMillis = currentMillis;
  }

  // Принимаем все пакеты CAN..
  if(can_read(&rxFrame)) {
    can_counter++;
    String str_min = str_obd2_min(rxFrame)+"\r\n"; //min format
    String str_ext = str_obd2_ext(rxFrame)+"\r\n"; //ext format
    //Serial.print("--["+String(ESP.getFreeHeap())+"] ["+String(can_counter)+"] "+str_ext); //debug

    if (flag_cycle){  //одна постоянная команда в своем цикле
      if(rxFrame.identifier == 0x7E8 && rxFrame.data[1]==0x41 && rxFrame.data[2]==5) {   //ID=0x7E8
          sent_counter = 0; //подтверждение приема пакета "01 05"
          collant = rxFrame.data[3] - 40;  //расчет
          if(!flag_ext_format) ble_handle_tx(String(collant)+"°C\r\n");
          else ble_handle_tx(str_ext); //to BLE ext format
      }
    } else { //сниффер и обработка команд BLE

      if(!flag_ext_format) ble_handle_tx(str_min); //to BLE min format
      else ble_handle_tx(str_ext); //to BLE ext format
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
      Serial.printf("--Frame sent: ERROR.. tx_queue: %d\r\n",can_tx_queue());
    }
}

//принятый пакет CAN в строку в расширенном формате (sniffer)
String str_obd2_ext(twai_message_t& message){
  String rez = "";
  char buf[3];
  if (message.extd) rez+="CAX: 0x";
  else rez+="CAN: 0x";
  rez+=String(message.identifier,HEX);
  int n=message.data_length_code; //длина пакета CAN (DLC)
  rez+=" [";rez+=String(n);rez+="] ";
  for (int i=0; i < n; i++){
    if (i != 0)  rez+=" ";
    sprintf( buf , "%.2X", message.data[i] );
    rez+=String(buf);
  } 
  return rez;
}
//принятый пакет OBD2 (8 байт!) в строку в минимальном формате
String str_obd2_min(twai_message_t& message){
    String rez = "";
    char buf[4]; //буфер для перевода в HEX
    int dlc=message.data_length_code;  //длина пакета
    int n= message.data[0];  //количество значащих байт в пакете(3)
    if(dlc!=8 || n>7) return "???"; //фильтр
    //if(n>7) n=7; //фильтр по длине
    for(int i=1;i<n+1;i++){ //[1,2,3]
        if(i != n) sprintf( buf , "%.2X ", message.data[i] ); //кроме последнего(3)
        else sprintf( buf , "%.2X", message.data[i] );  //последний , без пробела
        rez+=String(buf);  //добавляем в результат
    }
    return rez;
}
/*
String str_obd2_min(uint8_t arr[]){
    String rez = "";
    char buf[4]; //буфер для перевода в HEX
    int n= arr[0];  //количество значащих байт в пакете(3)
    if(n>7) n=7; //фильтр по длине
    for(int i=1;i<n+1;i++){ //[1,2,3]
        if(i != n) sprintf( buf , "%.2X ", arr[i] ); //кроме последнего(3)
        else sprintf( buf , "%.2X", arr[i] );  //последний , без пробела
        rez+=String(buf);  //добавляем в результат
    }
    return rez;
}
*/