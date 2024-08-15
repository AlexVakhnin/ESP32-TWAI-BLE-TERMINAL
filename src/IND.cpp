#include <Arduino.h>
#include <Ticker.h>
#define temp_cooler_on 98  //температура включения вентилятора (98)

extern bool flag_error;
extern int collant; //температура двигателя

//звуковой режим 0,1,2,3,4
// 0-нет звука
// 1-одинарный звуковой сигнал (t-включения вентилятора)
// 2-двойной звуковой сигнал 
// 3-непрерывный звуковой сигнал (перегрев)
// 4-одинарный частый сигнал (ошибка по шине CAN)
int sound_state = 0;

int blink_counter =1; //счетчик звуковых сигналов в периоде
int s_counter = 0; //для измерения времени (тик=40мс)

Ticker hTicker;

//следим за температурой двигателя и ошибками
void temp_watch(){
    if (flag_error) {
        sound_state = 4; //индикация ошибки
    } else {
        //sound_state = 0;  //глушим звук
        if(collant == temp_cooler_on ) sound_state = 1;
        else if(collant == temp_cooler_on + 1 ) sound_state = 2;
        else if(collant > temp_cooler_on + 1 ) sound_state = 3; //перегрев (непрерывный сигн.)
        else /*if(collant < temp_cooler_on )*/ sound_state = 0;
    }
}

void handle_2_40(){
    if (s_counter>0) s_counter--;  //считаем тики (пауза..)
    else { //логика переходов
        if (digitalRead(8)) {   //__/--
            digitalWrite(8, LOW);
            s_counter=2;
        } else {                //--\__
            digitalWrite(8, HIGH);
            s_counter=40;
       } 
    }
}

void  handle_2_1_2_37(){
    if (s_counter>0) s_counter--;  //считаем тики (пауза..)
    else { //логика переходов
        if (digitalRead(8) && blink_counter==1) {           //  __/-- 1
            digitalWrite(8, LOW);
            s_counter=2;
        } else if(!digitalRead(8) && blink_counter==1){     //  --\__ 1
            digitalWrite(8, HIGH);
            s_counter=1;blink_counter=2;
        } else if(digitalRead(8) && blink_counter==2){      //  __/-- 2
            digitalWrite(8, LOW);
            s_counter=2;
        } else /*if(!digitalRead(8) && blink_counter==2)*/{     //  --\__ 2
            digitalWrite(8, HIGH);
            s_counter=37;blink_counter=1;
        }
    }
}

void handle_2_8(){
    if (s_counter>0) s_counter--;  //считаем тики (пауза..)
    else { //логика переходов
        if (digitalRead(8)) {   //__/--
            digitalWrite(8, LOW);
            s_counter=2;
        } else {                //--\__
            digitalWrite(8, HIGH);
            s_counter=8;
       } 
    }
}

void t_40ms_job(){
    temp_watch();  //следим за температурой двигателя и ошибками
    
    if (sound_state==1){ //2-40
        handle_2_40();
    }
    else if (sound_state==2){ //2-1-2-37
        handle_2_1_2_37();
    }
    else if (sound_state==3){ //перегрев !!!
        digitalWrite(8, LOW);
    }
    else if (sound_state==4){ //2-8 (ошибка)
        handle_2_8();
    }
    else {
        digitalWrite(8, HIGH); //глушим все..режим 0
    } 
}

//индикация и звук
void ticker_init() {
hTicker.attach_ms(40, t_40ms_job);
}

