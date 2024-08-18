#include <Arduino.h>
#include <Ticker.h>

#define TEMP_COOLER_ON 98  //температура включения вентилятора (98)
#define TEMP_WINTER_OK 35  //температура зимнего прогрева двигателя

#define SOUND_ON 0  //состояние порта, когда звук включен (пассивный зуммер)
#define SOUND_OFF 1 //состояние порта, когда звук выключен (пассивный зуммер)

#define LED_ON 0  //состояние порта, когда светодиод включен
#define LED_OFF 1 //состояние порта, когда светодиод выключен
//свободны только 0,1,3,10 (20,21 UART1 - используется во время прошивки)
#define BUZZER_PIN 1  //активный зуммер - порт
#define TONE_PIN 0   //пассивный зуммер - порт
#define LED_PIN 8  //светодиод - порт (10)

//входные параметры для индикации
extern bool flag_error; //ошибка в цикле опроса OBD2
extern int collant; //текущая температура двигателя

//звуковой режим 0,1,2,3,4
// 0-нет звука
// 1-одиночный периодический звуковой сигнал (t-включения вентилятора)
// 2-двойной периодический звуковой сигнал 
// 3-непрерывный звуковой сигнал (перегрев)
// 4-одиночный частый периодический сигнал (ошибка по шине CAN)
// 5-одиночный однократный сигнал (внимание..)
int sound_state = 0;

//режим световой индикации
// 0-светодиод постоянно выключен
// 1-светодиод мигает
// 2-светодиод постоянно включен
int led_state = 0;

int blink_counter =1; //счетчик звуковых сигналов в периоде
int s_counter = 0; //для измерения времени (звук)
int l_counter = 0; //для измерения времени (светодиод)

Ticker hTicker;

//следим за температурой двигателя и ошибками
void temp_watch(){
    if (flag_error) {
        sound_state = 4; //индикация ошибки
    }
    else if(sound_state==5){
        //однократный звуковой сигнал - просто пропускаем..
    }
    else {
        //проходим весь диапазон температур
        /*
        if(collant == TEMP_COOLER_ON ) sound_state = 1;
        else if(collant == TEMP_COOLER_ON + 1 ) sound_state = 2;
        else if(collant > TEMP_COOLER_ON + 1 ) sound_state = 3; //перегрев (непрерывный сигн.)
        else { sound_state = 0;}
        */
        
        if(collant<TEMP_WINTER_OK-10){  //<25
            sound_state=0;led_state=2;  //только светим
        }
        else if(collant>=TEMP_WINTER_OK-10 && collant<TEMP_WINTER_OK){  //25-34
            sound_state=0;led_state=1;  //только мигаем
        }
        else if(collant>=TEMP_WINTER_OK && collant<TEMP_COOLER_ON-6){  //35-91
            sound_state=0;led_state=0;  //нет сигналов
        }
        else if(collant>=TEMP_COOLER_ON-6 && collant<TEMP_COOLER_ON){  //92-97
            sound_state=0;led_state=1;  //только мигаем
        }
        else if(collant == TEMP_COOLER_ON ){  //=98
            sound_state = 1;led_state=1;  //мигаем + одинарный звуковой сигнал
        }
        else if(collant == TEMP_COOLER_ON + 1 ){  //=99
            sound_state = 2;led_state=1;  //мигаем + двойной звуковой сигнал
        }
        else if(collant > TEMP_COOLER_ON + 1 ){  //>99
            sound_state = 3;led_state=2;  //непрерывный свет и сигнал (перегрев)
        }
        else { sound_state = 0;led_state=0;} //заглушка
        
    }
    
}

//звуковой сигнал=2 тика, пауза=40 тиков
void handle_2_40(){
    if (s_counter>0) s_counter--;  //считаем тики (пауза..)
    else { //логика переходов
        if (digitalRead(BUZZER_PIN)==SOUND_OFF) {   // __/--
            digitalWrite(BUZZER_PIN, SOUND_ON);
            tone(TONE_PIN,1500);
            s_counter=2;
        } else {                                    // --\__
            digitalWrite(BUZZER_PIN, SOUND_OFF);
            noTone(TONE_PIN);
            s_counter=40;
       } 
    }
}

//два звуковых сигнала по 2 тика, пауза=37 тиков
void  handle_2_1_2_37(){
    if (s_counter>0) s_counter--;  //считаем тики (пауза..)
    else { //логика переходов
        if (digitalRead(BUZZER_PIN)==SOUND_OFF && blink_counter==1){        // __/-- 1
            digitalWrite(BUZZER_PIN, SOUND_ON);
            tone(TONE_PIN,1800);
            s_counter=2;
        } else if(digitalRead(BUZZER_PIN)==SOUND_ON && blink_counter==1){   // --\__ 1
            digitalWrite(BUZZER_PIN, SOUND_OFF);
            noTone(TONE_PIN);
            s_counter=1;blink_counter=2;
        } else if(digitalRead(BUZZER_PIN)==SOUND_OFF && blink_counter==2){  // __/-- 2
            digitalWrite(BUZZER_PIN, SOUND_ON);
            tone(TONE_PIN,1800);
            s_counter=2;
        } else {                                                            // --\__ 2
            digitalWrite(BUZZER_PIN, SOUND_OFF);
            noTone(TONE_PIN);
            s_counter=37;blink_counter=1;
        }
    }
}

//три звуковых сигнала по 2 тика, пауза=20 тиков
void  handle_2_1_2_1_2_20(){
    if (s_counter>0) s_counter--;  //считаем тики (пауза..)
    else { //логика переходов
        if (digitalRead(BUZZER_PIN)==SOUND_OFF && blink_counter==1){        // __/-- 1
            digitalWrite(BUZZER_PIN, SOUND_ON);
            tone(TONE_PIN,2000);
            s_counter=2;
        } else if(digitalRead(BUZZER_PIN)==SOUND_ON && blink_counter==1){   // --\__ 1
            digitalWrite(BUZZER_PIN, SOUND_OFF);
            noTone(TONE_PIN);
            s_counter=1;blink_counter=2;
        } else if (digitalRead(BUZZER_PIN)==SOUND_OFF && blink_counter==2){ // __/-- 2
            digitalWrite(BUZZER_PIN, SOUND_ON);
            tone(TONE_PIN,2000);
            s_counter=2;
        } else if(digitalRead(BUZZER_PIN)==SOUND_ON && blink_counter==2){   // --\__ 2
            digitalWrite(BUZZER_PIN, SOUND_OFF);
            noTone(TONE_PIN);
            s_counter=1;blink_counter=3;
        } else if(digitalRead(BUZZER_PIN)==SOUND_OFF && blink_counter==3){  // __/-- 3
            digitalWrite(BUZZER_PIN, SOUND_ON);
            tone(TONE_PIN,2000);
            s_counter=2;
        } else {                                                            // --\__ 3
            digitalWrite(BUZZER_PIN, SOUND_OFF);
            noTone(TONE_PIN);
            s_counter=20;blink_counter=1;
        }
    }
}

//звуковой сигнал=2 тика, пауза=8 тиков (частый)
void handle_2_8(){
    if (s_counter>0) s_counter--;  //считаем тики (пауза..)
    else { //логика переходов
        if (digitalRead(BUZZER_PIN)==SOUND_OFF) {   // __/--
            digitalWrite(BUZZER_PIN, SOUND_ON);
            tone(TONE_PIN,1500);
            s_counter=2;
        } else {                                    // --\__
            digitalWrite(BUZZER_PIN, SOUND_OFF);
            noTone(TONE_PIN);
            s_counter=8;
       } 
    }
}

//однократный звуковой сигнал=2 тика (внимание..)
void handle_attention(){
    if (s_counter>0) s_counter--;  //считаем тики (пауза..)
    else { //логика переходов
        if (digitalRead(BUZZER_PIN)==SOUND_OFF) {   // __/--
            digitalWrite(BUZZER_PIN, SOUND_ON);
            tone(TONE_PIN,1500);
            s_counter=2;
        } else {                                    // --\__
            digitalWrite(BUZZER_PIN, SOUND_OFF);
            noTone(TONE_PIN);
            sound_state=0;
       } 
    }
}

//мигаем светодиодом
void led_handle_8_8(){
    if (l_counter>0) l_counter--;  //считаем тики (пауза..)
    else { //логика переходов
        if (digitalRead(LED_PIN)==LED_OFF) {   // __/--
            digitalWrite(LED_PIN, LED_ON);
            l_counter=8;
        } else {                               // --\__
            digitalWrite(LED_PIN, LED_OFF);
            l_counter=8;
        } 
    }
}

//процедура каждые 40 милисекунд
void t_40ms_job(){
    
    temp_watch();  //следим за температурой двигателя и ошибками

    //распределитель..звук
    if (sound_state==1){ //2-40
        handle_2_40();
    }
    else if (sound_state==2){ //2-1-2-37
        handle_2_1_2_37();
    }
    else if (sound_state==3){ //перегрев !!!
        handle_2_1_2_1_2_20();
        //if(digitalRead(BUZZER_PIN)==SOUND_OFF){
        //    digitalWrite(BUZZER_PIN, SOUND_ON);
        //    tone(TONE_PIN,1500);
        //}
    }
    else if (sound_state==4){ //2-8 (ошибка)
        handle_2_8();
    }
    else if (sound_state==5){ //2-0 (внимание..)
        handle_attention();//Serial.println("!");
    }
    else {
        if(digitalRead(BUZZER_PIN)==SOUND_ON){
            digitalWrite(BUZZER_PIN, SOUND_OFF); //глушим звук..режим 0
            noTone(TONE_PIN);
        }
    }

    //распределитель..LED
    if (led_state==1){
        led_handle_8_8();  //светодиод мигает
    }
    else if (led_state==2){
        digitalWrite(LED_PIN, LED_ON); //светодиод включен
    }
    else {
        digitalWrite(LED_PIN, LED_OFF); //светодиод выключен
    }
}

//индикация и звук
void ticker_init() {

    pinMode(BUZZER_PIN, OUTPUT);  //активный зуммер
    digitalWrite(BUZZER_PIN, SOUND_OFF); //SOUND OFF

    pinMode(TONE_PIN,OUTPUT);  //пассивный зуммер
    //noTone(TONE_PIN);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_OFF); //LED OFF

    hTicker.attach_ms(40, t_40ms_job); //1 тик = 40мс

    sound_state = 5; //однократный звуковой сигнал
}

