#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

//extern String ds1;
//extern String ds2;
extern String dev_name;
extern bool flag_cycle;
extern int collant;
extern bool flag_ext_format;
extern uint8_t send_content[];
extern int can_counter;
extern int filter;

extern uint32_t can_tx_queue();
extern void sendObdFrame();

void storage_dev_name(String dname);
void storage_filter(String sfilter);
void storage_send_content();
void help_print();
void reset_nvram();
String hex_arr8(uint8_t arr[]);
void obd2_01_05_handle();

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
      digitalWrite(8, LOW); //internal led = ON (DEBUG..)
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Event-Disconnect..");
      delay(300); // give the bluetooth stack the chance to get things ready
      BLEDevice::startAdvertising();  // restart advertising
      digitalWrite(8, HIGH); //internal led = OFF (DEBUG..)
    }
};

//ловим события write от BLE сервиса
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
              String s ="DeviceName: "+dev_name
                  +"\r\nFilter: 0x"+String(filter,HEX)+" (0x7e8)"
                  +"\r\nFreeMem: "+String(ESP.getFreeHeap())
                  +"\r\nCollant: "+String(collant)
                  +"\r\n--------------state---------------"
                  +"\r\nobd2_recv="+hex_arr8(send_content)
                  +"\r\nflag_cycle="+String(flag_cycle)
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
                can_counter = 0;
                ble_handle_tx("cycle=on\r\n");
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
                obd2_01_05_handle();
            }                        
            else if (pstr.substring(0,4)=="atn=") { //atn= - dev_name save NVRAM
                storage_dev_name(pstr.substring(4)); //dev_name
            }
            else if (pstr.substring(0,4)=="atf=") { //atf= - can filter save NVRAM
                storage_filter(pstr.substring(4)); //filter
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
  filter = preferences.getInt("filter", 0x7E8);
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

//отправляем строку клиенту BLE на терминал.. 
void ble_handle_tx(String str){
    if (deviceConnected) { //проверка, что подключен клиент BLE
        if(str.length()==0) str="none..\r\n";
        pCharacteristic->setValue(str.c_str());
        pCharacteristic->notify(false); //false=indicate; true=wait confirmation
    }
}
//команда at+m - сохранить тек. параметры obd2 в NVRAM
void storage_send_content(){ 
    preferences.begin("hiveMon", false);
    preferences.putBytes("send_content", send_content, 8);
    preferences.putBool("flag_cycle", flag_cycle);
    preferences.putBool("flag_ext_format", flag_ext_format);
    preferences.end();
    ble_handle_tx("current content saved..\r\n");   
}
//сохранить имя BLE устройства в NVRAM
void storage_dev_name(String dname){ 
  int n = dname.length(); 
  dev_name = dname.substring(0,n-2); //удалить \r\n в конце строки..
  ble_handle_tx("new dev_name="+dev_name+"\r\n"); //ответ на BLE
  preferences.begin("hiveMon", false);
  preferences.putString("dev_name", dev_name);
  preferences.end();
  delay(3000);
  ESP.restart();
}

//сохранить фильтр в NVRAM
void storage_filter(String sfilter){ 
  int n = sfilter.length(); 
  String fstr = sfilter.substring(2,n-2); //удалить 0x в начале и \r\n в конце строки..
  int number = (int) strtol( &fstr[0], NULL, 16);
  if (number>=0 && number<=0x7FF && sfilter[0]==0x30 && sfilter[1]==0x78) { //test
    ble_handle_tx("new filter= 0x"+String(number,HEX)+"\r\n"); //ответ на BLE
    filter = number;
    preferences.begin("hiveMon", false);
    preferences.putInt("filter", filter);
    preferences.end();
    delay(3000);
    ESP.restart();
  } else ble_handle_tx("ERROR NUMBER..\r\n");
}

//команда at?
void help_print(){
  String  shelp="ati - list current state";
        shelp+="\r\natn=[name] - BLE device name";
        shelp+="\r\natc=1/0 - cycle on/off";
        shelp+="\r\nate=1/0 - ext format on/off";
        shelp+="\r\nat+m - save current state";
        shelp+="\r\natz - set to default";
  ble_handle_tx(shelp+"\r\n");
}
//команда atz - очистить NVRAM
void reset_nvram(){
  preferences.begin("hiveMon", false); //пространство имен (чтение-запись)
  preferences.clear(); //удаляем все ключи в пространстве имен
  preferences.end();
  ble_handle_tx("NVRAM Key Reset..\r\n"); //ответ на BLE
  delay(3000);
  ESP.restart();
}
//команда "01 05" - запрос температуры ОЖ в obd2
void obd2_01_05_handle(){
    flag_cycle = false; //остановить циклический запрос obd2 
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
//преобразовать массив 8 байт в HEX строку: xx:xx:xx:xx:xx:xx:xx:xx
String hex_arr8(uint8_t arr[]){
    char buf[24]; //буфер для перевода в HEX
    sprintf( buf , "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
      arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7] );
    return buf;
}