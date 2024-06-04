#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
uint32_t value = 0;

extern String ds1;

char hexChar[150]; //массив для sprintf() функции (150 - на строку)
char mcalibr[24] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //буфер передачи BLE
//char strble[20];

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

//Declaration
void ble_handle_tx();
String _hex_calibr();

//ловим события connect/disconnect от BLE сервера
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Event-Connect..");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Event-Disconnect..");

      delay(500); // give the bluetooth stack the chance to get things ready
      pServer->startAdvertising(); // restart advertising

    }
};

//ловим события от сервиса read/write
 class MyCallbacks: public BLECharacteristicCallbacks {
     void onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) { //запись в сервис..
       std::string rxValue = pCharacteristic->getValue(); //строка от BLE

       if (rxValue.length() > 0) {
        //выводим на индикатор
        ds1 = rxValue.c_str();
        //печатаем строку от BLE
         Serial.print("Received Value: ");
         for (int i = 0; i < rxValue.length(); i++) {
           Serial.print(rxValue[i]);
         }
         // Do stuff based on the command received from the app
         if (rxValue.find("A") != -1) { 
           ble_handle_tx(); //ответ для терминального клиента
         }
         else if (rxValue.find("B") != -1) {
           Serial.println("Turning OFF!");
         }
       }
     }

     void onRead(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) { //чтение
        Serial.println("Event-Read..");
     }

 };




void ble_setup(){

  // Create the BLE Device
  BLEDevice::init("ESP32-C3-MINI-CAN");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks()); //Обработчик событий connect/disconnect от BLE

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                    /*  BLECharacteristic::PROPERTY_READ   |*/
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                    /*  BLECharacteristic::PROPERTY_BROADCAST|*/
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // Create a BLE Descriptor !!!
  pCharacteristic->addDescriptor(new BLE2902()); //без дискритора не подключается..

  pCharacteristic->setCallbacks(new MyCallbacks()); //обработчик событий read/write

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();

  Serial.print("BLE Server address: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());
}

void ble_handle_tx(){

    if (deviceConnected) { //проверка, что подключен клиент BLE

//      delay(300);
        String strble= "Hallo ESP32-BLE !\r\n";
        //int len = strble.length();
        if(strble.length()>20) strble="resp length error..";
        //sprintf(mcalibr, "%s\r\n",strble.c_str()); //добавляем параметры
        //Serial.println(_hex_calibr()); //for debug only

        //pCharacteristic->setValue((uint8_t*)mcalibr,len+2); //worked..)
        //pCharacteristic->setValue(String(mcalibr).c_str()); //worked..)
        //pCharacteristic->setValue(mcalibr); //worked..)
        pCharacteristic->setValue(strble.c_str());

        pCharacteristic->indicate();//для работы с BLE терминалом !!!!!!!

        Serial.print("Responce: "+strble);
        //delay(3000); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }


}

String _hex_calibr(){  //hex mcalibr[] string
      sprintf(hexChar,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X ",
        mcalibr[0],mcalibr[1],mcalibr[2],mcalibr[3],mcalibr[4],mcalibr[5],mcalibr[6],mcalibr[7],
        mcalibr[8],mcalibr[9],mcalibr[10],mcalibr[11],mcalibr[12],mcalibr[13],mcalibr[14],mcalibr[15],
        mcalibr[16],mcalibr[17],mcalibr[18],mcalibr[19],mcalibr[20],mcalibr[21],mcalibr[22],mcalibr[23]);
      return hexChar;
}
