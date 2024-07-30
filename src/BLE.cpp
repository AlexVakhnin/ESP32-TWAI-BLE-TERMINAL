#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

//const int orange_pin = 20;
//extern int orange_pin;

//Declaration
//void storage_factor_u(String su);
//void storage_factor1_u(String su);
//void storage_adc_u(String su);
//void storage_alarm_h(String su);
//void storage_alarm_l(String su);
void storage_dev_name(String dname);
void help_print();
void reset_nvram();
//extern void disp_show();
//extern void disp_main();
//Global Variables
extern String ds1;
extern String ds2;
extern String dev_name;
//extern int sens_value;
//extern int sens1_value;
//extern float sens_voltage;
//extern float sens1_voltage;
//extern float real_voltage;
//extern float real1_voltage;
//extern double factor; //(nvram)
//extern double factor1; //(nvram)
//extern double adc_calibr; //(nvram)
//extern float alarm_h; //(nvram)
//extern float alarm_l; //(nvram)
//extern boolean ble_indicate;
//extern long ble_pcounter;
//extern long ble_period;
//extern long pause_counter;
//extern boolean doShutdown;
//extern boolean doPowerOn;
//extern boolean doPause;
//extern String dispstatus;
//extern int zone_flag;
//extern boolean ac220v_flag;

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
uint32_t value = 0;

Preferences preferences; //for NVRAM



// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "450475bb-56a2-4c97-9973-301831e65a30"
#define CHARACTERISTIC_UUID "d8182a40-7316-4cbf-9c6e-be507a76d775"

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
      //ble_indicate = true; //захват дисплея
      //ds1="R:";ds2="S:";disp_show(); //вывод на дисплей
      deviceConnected = true;
      digitalWrite(8, LOW); //led = ON (DEBUG..)
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Event-Disconnect..");
      //ble_period=ble_pcounter; //время между BLE опросами
      //ble_pcounter=0;
      //disp_main(); //обмен BLE закончен, показать основной экран
      //ble_indicate = false; //отпускаем дисплей
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
            //ds1="R:"+pstr; disp_show();//вывод на дисплей
            
            // Do stuff based on the command received from the app
            if (pstr=="at"||pstr=="at\r\n") {     //at
                ble_handle_tx("OK"); //sensor number
                //ds2="S:OK";disp_show(); //вывод на дисплей
            }
            else if (pstr=="at?"||pstr=="at?\r\n") { //at? - help
                help_print();
            }            
            else if (pstr=="atz"||pstr=="atz\r\n") { //atz - reset NVRAM
                reset_nvram();
            }            
            else if (pstr=="ati"||pstr=="ati\r\n") { //ati - information
              //String zone="";String ac220="";
              //if(zone_flag==1) {zone ="HIGH";} else if(zone_flag==2) {zone ="LOW";} else {zone ="MIDDLE";}
              //if(ac220v_flag) {ac220="ON";} else {ac220="OFF";}  
              String s ="name="+dev_name;
                  //+"\r\ntimeout="+String(pause_counter)
                  //+"\r\nstatus="+dispstatus
                  //+"\r\nzone="+zone
                  //+"\r\nac220v="+ac220
                  //+"\r\nrelay="+String(digitalRead(orange_pin))
                  //+"\r\nalarm_h="+String(alarm_h)
                  //+"\r\nalarm_l="+String(alarm_l) 
                  //+"\r\nreal_voltage="+String(real_voltage);
                ble_handle_tx(s); //information for debug
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
  //factor1 = preferences.getDouble("att_factor1", 5.00);
  //adc_calibr = preferences.getDouble("adc_calibr", 3.00);//default adc_calibr=3.00 Volt !!!
 // alarm_h = preferences.getFloat("alarm_h", 11.5);
  //alarm_l = preferences.getFloat("alarm_l", 10.0);// BMS-3S-1 отключает при уровне 9.3 вольта..
  dev_name = preferences.getString("dev_name", "ODB2-BLE-GATE");
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
/*
//калибровка делителя 0 через U (вводим напряжение)
void storage_factor_u(String su){
  float test_volt=su.toFloat(); //округляет до 2-х знаков после дес. точки...???
  factor = test_volt/sens_voltage;
  Serial.println("new factor="+String(factor));
  ble_handle_tx("new factor="+String(factor)); //ответ на BLE

  preferences.begin("hiveMon", false);
  preferences.putDouble("att_factor", factor);
  preferences.end();
}
//калибровка делителя 1 через U (вводим напряжение)
void storage_factor1_u(String su){
  float test_volt=su.toFloat(); //округляет до 2-х знаков после дес. точки...???
  factor1 = test_volt/sens1_voltage;
  Serial.println("new factor1="+String(factor1));
  ble_handle_tx("new factor1="+String(factor1)); //ответ на BLE

  preferences.begin("hiveMon", false);
  preferences.putDouble("att_factor1", factor1);
  preferences.end();
}

//калибровка ADC через U (вводим напряжение)
void storage_adc_u(String su){
  float test_volt=su.toFloat();
  adc_calibr = test_volt*4096/sens_value;
  Serial.println("new adc_calibr="+String(adc_calibr));
  ble_handle_tx("new adc_calibr="+String(adc_calibr)); //ответ на BLE

  preferences.begin("hiveMon", false);
  preferences.putDouble("adc_calibr", adc_calibr);
  preferences.end();
}
//
void storage_alarm_h(String su){
  alarm_h = su.toFloat();
  ble_handle_tx("new alarm_h="+String(alarm_h)); //ответ на BLE
  preferences.begin("hiveMon", false);
  preferences.putFloat("alarm_h", alarm_h);
  preferences.end();
}
void storage_alarm_l(String su){
  alarm_l = su.toFloat();
  ble_handle_tx("new alarm_l="+String(alarm_l)); //ответ на BLE
  preferences.begin("hiveMon", false);
  preferences.putFloat("alarm_l", alarm_l);
  preferences.end();
}
*/
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
  String  shelp="ati -parameter list";
          shelp+="\r\natс -calibration parameter list";
          shelp+="\r\natv -resulting voltage";
          shelp+="\r\natz -set default parameters";
          shelp+="\r\nata=[U_ADC_in] -ADC calibration";
          shelp+="\r\natu=[U_in] -attenuator 0 calibration";
          shelp+="\r\natu1=[U1_in] -attenuator 1 calibration";
          shelp+="\r\nath=[U] -alarm H Voltage";
          shelp+="\r\natl=[U] -alarm L Voltage";
          shelp+="\r\natn=[name] -BLE device name";
          shelp+="\r\ndhutdown -orange pi shutdown";
          shelp+="\r\npoweron -orange pi power on";
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