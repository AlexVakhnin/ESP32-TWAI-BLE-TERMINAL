#include <Arduino.h>

extern String ds2;
extern void disp_show();
extern String formatted_time;

//Local Variables
unsigned long sUpTime = 0; //счетчик посекундно
unsigned long ihour = 0;
unsigned long imin = 0;
unsigned long isec = 0;
unsigned long iday = 0;
//boolean led_state = false;

//вычислить uptime д/ч/м/с (вызывается с периодом 1 сек)
void get_uptime(){
    sUpTime+=1;  //прибавляем 1 сек
    auto n=sUpTime; //количество всех секунд
    isec = n % 60;  //остаток от деления на 60 (секунд в минуте)
    n /= 60; //количество всех минут (целая часть)  
    imin = n % 60;  //остаток от деления на 60 (минут в часе)
    n /= 60; //количество всех часов (целая часть)
    ihour = n % 24; //остаток от деления на 24 (часов в дне)
    iday = n/24; //количество всех дней (целая часть)
}

// Функцию вызывает Ticker с периодом 1 сек.
void t_1s_job(){
get_uptime();

char locTime[9];
sprintf(locTime, "%02d:%02d:%02d", ihour, imin, isec);
formatted_time = locTime; //'hh:mm:ss'

ds2=String(iday)+" "+formatted_time;//String(ihour)+":"+String(imin)+":"+String(isec);
disp_show();

//if (led_state){ digitalWrite(8, HIGH);led_state=false;}
//else {digitalWrite(8, LOW);led_state=true;}
}