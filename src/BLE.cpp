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
extern uint8_t send_content[];
extern char str_hex[24];

void storage_dev_name(String dname);
void storage_send_content();
void help_print();
void reset_nvram();
void str_hex_arr8();

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
uint32_t value = 0;

Preferences preferences; //for NVRAM

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "f0717496-ab77-4ebc-9705-eff9b74e5c39"
#define CHARACTERISTIC_UUID "916cc1fc-7111-48e7-a511-80b76a4df530"

//Declaration
void ble_handle_tx(String str );

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
            Serial.print("BLE received Value: ");Serial.print(pstr);
            
            // Do stuff based on the command received from the app
            if (pstr=="at"||pstr=="at\r\n") {     //at
                ble_handle_tx("OK"); //sensor number
            }
            else if (pstr=="at?"||pstr=="at?\r\n") { //at? - help
                help_print();
            }            
            else if (pstr=="atz"||pstr=="atz\r\n") { //atz - reset NVRAM
                reset_nvram();
            }            
            else if (pstr=="ati"||pstr=="ati\r\n") { //ati - information
                str_hex_arr8(); //конвертация в HEX из send_content[]
              String s ="name="+dev_name
                  +"\r\nsend_cont="+String(str_hex)
                  //+"\r\nstatus="+dispstatus
                  +"\r\nflag_cycle="+String(flag_cycle);
                ble_handle_tx(s); //send string to BLE client
            }
            else if (pstr=="atc0"||pstr=="atc0\r\n") { //cycle = on
                flag_cycle = false;
                ble_handle_tx("cycle=off");
            }            
            else if (pstr=="atc1"||pstr=="atc1\r\n") { //cycle = off
                flag_cycle = true;
                ble_handle_tx("cycle=on");
            }            
            else if (pstr=="ats"||pstr=="ats\r\n") { //storage send_content
                storage_send_content();
            }            
            
            else if (pstr.substring(0,4)=="atn=") { //atn= - dev_name save NVRAM
                storage_dev_name(pstr.substring(4)); //dev_name
            }
            
            else {ble_handle_tx("???");}
            
        }
    }

    //FOR DEBUG
    /*
    void onRead(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) { //Read
        Serial.println("Event-Read..");
    }
    void onNotify(BLECharacteristic* pCharacteristic) { //Notify
        Serial.println("Event-Notify..");
    }
    void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) { //Status
        Serial.println("Event-Status..");
    }
    */
   
 };

//Init BLE Service
void ble_setup(){

  //читаем все параметры NVRAM
  preferences.begin("hiveMon", true); //открываем пространство имен NVRAM read only
  //factor = preferences.getDouble("att_factor", 5.00);
  dev_name = preferences.getString("dev_name", "ODB2-BLE-GATE");
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

//ответ клиенту
void ble_handle_tx(String str){

    if (deviceConnected) { //проверка, что подключен клиент BLE
        if(str.length()==0) str="none..\r\n";

        str = str+"\r\n";
        //Serial.println("str="+str);
        pCharacteristic->setValue(str.c_str());
        //pCharacteristic->indicate();//для работы с BLE терминалом !!!!!!!
        pCharacteristic->notify(false); //false=indicate; true=wait confirmation

        Serial.print("--Send to BLE client: "+str);
    }

}
void storage_send_content(){
    preferences.begin("hiveMon", false);
    preferences.putBytes("send_content", send_content, 8);
    preferences.end();
    ble_handle_tx("send_content saved..");   
}
void storage_dev_name(String dname){
  //Нужно удалить \r\n в конце строки..
  int n = dname.length(); 
  dev_name = dname.substring(0,n-2);
  ble_handle_tx("new dev_name="+dev_name); //ответ на BLE
  preferences.begin("hiveMon", false);
  preferences.putString("dev_name", dev_name);
  preferences.end();
  delay(3000);
  ESP.restart();
}

void help_print(){
  String  shelp="ati - parameter list";
        shelp+="\r\natn=[name] - BLE device name";
        shelp+="\r\natc0 - cycle off";
        shelp+="\r\natc1 - cycle on";
        shelp+="\r\nats - save last send_content";
  ble_handle_tx(shelp);
}
void reset_nvram(){
  preferences.begin("hiveMon", false); //пространство имен (чтение-запись)
  preferences.clear(); //удаляем все ключи в пространстве имен
  preferences.end();
  ble_handle_tx("NVRAM Key Reset.."); //ответ на BLE
  delay(3000);
  ESP.restart();
}
void str_hex_arr8 (){
      sprintf( str_hex , "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
      send_content[0], send_content[1], send_content[2], send_content[3],
      send_content[4], send_content[5], send_content[6], send_content[7] );
}