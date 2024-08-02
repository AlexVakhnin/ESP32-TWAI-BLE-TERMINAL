#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

extern String ds1;
extern String ds2;
extern String dev_name;
extern bool flag_cycle;
extern bool flag_ble_term;
extern bool flag_ext_format;
extern uint8_t send_content[];

extern uint32_t can_tx_queue();
extern void sendObdFrame();

void storage_dev_name(String dname);
void storage_send_content();
void help_print();
void reset_nvram();
String hex_arr8(uint8_t arr[]);
String hex_elm_arr8(uint8_t arr[]);
void elm327_01_05_handle();

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
uint32_t value = 0;

Preferences preferences; //for NVRAM

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "f0717496-ab77-4ebc-9705-eff9b74e5c39"
#define CHARACTERISTIC_UUID "916cc1fc-7111-48e7-a511-80b76a4df530"

void ble_handle_tx(String str);

//ловим события connect/disconnect от BLE сервера
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
      //формируем адрес клиента
      char remoteAddress[18];
      sprintf( remoteAddress , "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
      param->connect.remote_bda[0],
      param->connect.remote_bda[1],
      param->connect.remote_bda[2],
      param->connect.remote_bda[3],
      param->connect.remote_bda[4],
      param->connect.remote_bda[5]
      );
      //фильтруем mac адреса..
      //if (param->connect.remote_bda[0]==0x34){
      //    pServer->disconnect(param->connect.conn_id) ;//force disconnect client..
      //}
      Serial.print("Event-Connect..");Serial.println(remoteAddress);
      deviceConnected = true;
      digitalWrite(8, LOW); //led = ON (DEBUG..)
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Event-Disconnect..");
      delay(300); // give the bluetooth stack the chance to get things ready
      BLEDevice::startAdvertising();  // restart advertising
      digitalWrite(8, HIGH); //led = OFF (DEBUG..)
    }
};

//ловим события от BLE сервиса read/write
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) { //запись в сервис..
        std::string rxValue = pCharacteristic->getValue(); //строка от BLE

        if (rxValue.length() > 0) {
            //печатаем строку от BLE
            String pstr = String(rxValue.c_str());
            Serial.print("--BLE: received: ");Serial.print(pstr);
            
            // Do stuff based on the command received from the app
            if (pstr=="at"||pstr=="at\r\n") {     //at
                ble_handle_tx("OK\r\n"); //sensor number
            }
            else if (pstr=="at?"||pstr=="at?\r\n") { //at? - help
                help_print();
            }            
            else if (pstr=="atz"||pstr=="atz\r\n") { //atz - reset NVRAM
                reset_nvram();
            }            
            else if (pstr=="ati"||pstr=="ati\r\n") { //ati - information
                //str_hex_arr8(); //конвертация в HEX из send_content[]
              String s ="device_name: "+dev_name
                  +"\r\n--------------state---------------"
                  +"\r\nobd2_recv="+hex_arr8(send_content)
                  +"\r\nflag_cycle="+String(flag_cycle)
                  +"\r\nflag_ble_term="+String(flag_ble_term)
                  +"\r\nflag_ext_format="+String(flag_ext_format)
                  +"\r\n----------------------------------"
                  +"\r\nat+m - save current state";
                ble_handle_tx(s+"\r\n"); //send string to BLE client
            }
            else if (pstr=="atc=0"||pstr=="atc=0\r\n") { //flag_cycle = off
                flag_cycle = false;
                ble_handle_tx("cycle=off\r\n");
            }            
            else if (pstr=="atc=1"||pstr=="atc=1\r\n") { //flag_cycle = on
                flag_cycle = true;
                ble_handle_tx("cycle=on\r\n");
            }            
            else if (pstr=="atb=0"||pstr=="atb=0\r\n") { //flag_ble_term = off
                flag_ble_term = false;
                ble_handle_tx("ble_term=off\r\n");
            }            
            else if (pstr=="atb=1"||pstr=="atb=1\r\n") { //flag_ble_term = on
                flag_ble_term = true;
                ble_handle_tx("ble_term=on\r\n");
            }            
            else if (pstr=="ate=0"||pstr=="ate=0\r\n") { //flag_ext_format = off
                flag_ext_format = false;
                ble_handle_tx("ext_format=off\r\n");
            }            
            else if (pstr=="ate=1"||pstr=="ate=1\r\n") { //flag_ext_format = on
                flag_ext_format = true;
                ble_handle_tx("ext_format=on\r\n");
            }            
            else if (pstr=="at+m"||pstr=="at+m\r\n") { //storage send_content
                storage_send_content();
            }            
            else if (pstr=="01 05"||pstr=="01 05\r\n") { //elm327 imitation
                elm327_01_05_handle();
            }                        
            else if (pstr.substring(0,4)=="atn=") { //atn= - dev_name save NVRAM
                storage_dev_name(pstr.substring(4)); //dev_name
            }
            
            else {ble_handle_tx("???\r\n");}            
        }
    }
 };

//Init BLE Service
void ble_setup(){

  //читаем все параметры NVRAM
  preferences.begin("hiveMon", true); //открываем пространство имен NVRAM read only
  dev_name = preferences.getString("dev_name", "ODB2-BLE-GATE");
  flag_ble_term = preferences.getBool("flag_ble_term", true);
  flag_cycle = preferences.getBool("flag_cycle", false);
  flag_ext_format = preferences.getBool("flag_ext_format", false);
  int len = preferences.getBytes("send_content", send_content, 8);
  Serial.println("send_content Len = "+String(len));
  preferences.end(); //закрываем NVRAM

  // Create the BLE Device
  BLEDevice::init(dev_name.c_str());

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks()); //Обработчик событий connect/disconnect от BLE

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  // все свойства характеристики (READ,WRITE..)- относительно клиента !!!
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ      |
                      BLECharacteristic::PROPERTY_WRITE     |
                      BLECharacteristic::PROPERTY_NOTIFY    |
                      BLECharacteristic::PROPERTY_INDICATE  |
                      BLECharacteristic::PROPERTY_WRITE_NR  
                    );

  // Create a BLE Descriptor !!!
  pCharacteristic->addDescriptor(new BLE2902()); //без дескриптора не подключается..!!!

  pCharacteristic->setCallbacks(new MyCallbacks()); //обработчик событий read/write

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising(); 
  pAdvertising->addServiceUUID(SERVICE_UUID);  //рекламируем свой SERVICE_UUID(0x07)
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);//Conn.Intrval(0x12) is:0x06 [5 0x12 0x06004000] iOS..
  //pAdvertising->setMinPreferred(0x12);//Conn.Intrval(0x12) is:0x12 [5 0x12 0x12004000] Default
  //pAdvertising->setMinPreferred(0x0); // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();  
  
  Serial.print("BLE Server address: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());
  Serial.println("BLE Device name: "+ dev_name);
}

//ответ клиенту BLE , параметр - String
void ble_handle_tx(String str){

    if (deviceConnected) { //проверка, что подключен клиент BLE
        if(str.length()==0) str="none..\r\n";

        //str = str+"\r\n";
        //Serial.println("str="+str);
        pCharacteristic->setValue(str.c_str());
        //pCharacteristic->indicate();//для работы с BLE терминалом !!!!!!!
        pCharacteristic->notify(false); //false=indicate; true=wait confirmation

        Serial.print("--BLE: send: "+str);
    }

}
void storage_send_content(){ //команда at+m
    preferences.begin("hiveMon", false);
    preferences.putBytes("send_content", send_content, 8);
    preferences.putBool("flag_cycle", flag_cycle);
    preferences.putBool("flag_ble_term", flag_ble_term);
    preferences.putBool("flag_ext_format", flag_ext_format);
    preferences.end();
    ble_handle_tx("current content saved..\r\n");   
}
void storage_dev_name(String dname){
  //Нужно удалить \r\n в конце строки..
  int n = dname.length(); 
  dev_name = dname.substring(0,n-2);
  ble_handle_tx("new dev_name="+dev_name+"\r\n"); //ответ на BLE
  preferences.begin("hiveMon", false);
  preferences.putString("dev_name", dev_name);
  preferences.end();
  delay(3000);
  ESP.restart();
}

void help_print(){
  String  shelp="ati - list current state";
        shelp+="\r\natn=[name] - BLE device name";
        shelp+="\r\natc=1/0 - cycle on/off";
        shelp+="\r\natb=1/0 - show odb2 packets on/off";
        shelp+="\r\nate=1/0 - ext format on/off";
        shelp+="\r\nat+m - save current state";
        shelp+="\r\natz - set to default";
  ble_handle_tx(shelp+"\r\n");
}
void reset_nvram(){
  preferences.begin("hiveMon", false); //пространство имен (чтение-запись)
  preferences.clear(); //удаляем все ключи в пространстве имен
  preferences.end();
  ble_handle_tx("NVRAM Key Reset..\r\n"); //ответ на BLE
  delay(3000);
  ESP.restart();
}
//посылаем пакет obd2 в стиле elm327
void elm327_01_05_handle(){
    flag_cycle = false; //остановить циклический запрос obd2 
    flag_ble_term = true; //принимаемые пакеты печатать на BLE
    if(can_tx_queue()==0){
        for(int i=0;i<8;i++) send_content[i] = 0xAA;
        send_content[0]=2;
        send_content[1]=1;
        send_content[2]=5;
        sendObdFrame(); // Передача запроса по CAN шине.(obd2 pid=5)
    } else {
        ble_handle_tx("CAN BUS DOWN..\r\n");
    }


}
//преобразовать пакет obd2 в HEX строку: xx xx xx..
//в стиле ELM327
String hex_elm_arr8(uint8_t arr[]){
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
//преобразовать массив 8 байт в HEX строку: xx:xx:xx:xx:xx:xx:xx:xx
String hex_arr8(uint8_t arr[]){
    char buf[24]; //буфер для перевода в HEX
    sprintf( buf , "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
      arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7] );
    return buf;
}