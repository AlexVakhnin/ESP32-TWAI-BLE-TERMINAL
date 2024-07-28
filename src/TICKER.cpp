
#include <Arduino.h>

//Global Variables
extern unsigned long sFreeMem;
extern unsigned long sUpTime;
//extern unsigned long g3Time;
//extern unsigned long g2Time;
//extern unsigned long g1Time;
extern unsigned long ihour;
extern unsigned long imin;
extern unsigned long isec;
extern unsigned long iday;
extern String formatted_time;
//extern float vsens;
extern boolean led_state;
extern String ds2;
extern void disp_show();


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

ds2=String(iday)+" "+String(ihour)+":"+String(imin)+":"+String(isec);
disp_show();

if (led_state){ digitalWrite(8, HIGH);led_state=false;}
else {digitalWrite(8, LOW);led_state=true;}
}