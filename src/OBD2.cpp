#include <Arduino.h>

extern bool flag_cycle;
extern uint8_t send_content[];

extern uint32_t can_tx_queue();
extern void sendObdFrame();
extern void ble_handle_tx(String str);




bool test5(String str){
    //Serial.println(str);
    if(str[0]!=0x30) return false;
    if(!((str[1]>=0x30 && str[1]<=0x39)||(str[1]>=0x61 && str[1]<=0x66))) return false;
    if(str[2]!=0x20) return false;
    if(!((str[3]>=0x30 && str[3]<=0x39)||(str[3]>=0x61 && str[3]<=0x66))) return false;
    if(!((str[4]>=0x30 && str[4]<=0x39)||(str[4]>=0x61 && str[4]<=0x66))) return false;
    return true;
}

//послать CAN запрос
void obd2_send(int serv, int pid){
    flag_cycle = false; //остановить циклический запрос obd2 
    if(can_tx_queue()==0){
        for(int i=0;i<8;i++) send_content[i] = 0xAA;
        send_content[0]=2;
        send_content[1]=serv;
        send_content[2]=pid;
        sendObdFrame(); // Передача запроса по CAN шине.
    } else {
        ble_handle_tx("CAN BUS DOWN..\r\n");
    }
}

//обработка команд OBD2 принятых через терминал BLE ("01 00")
void obd2_ble_handle(String str){
    if(test5(str)){
        //вычисляем serv, pid
        String fstr ="";
        int serv=0;int pid=0;
        fstr = str.substring(1,2); //2 позиция
        serv = (int) strtol( &fstr[0], NULL, 16);
        fstr = str.substring(3,5); //4,5 позиции
        pid = (int) strtol( &fstr[0], NULL, 16);
        //Serial.println(String(serv)+" "+String(pid));
        //запрос
        obd2_send(serv,pid);
    } else {
        ble_handle_tx("???\r\n");
    }
}
