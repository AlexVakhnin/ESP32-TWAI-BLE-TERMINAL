
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define I2C_CLOCK 7
#define I2C_DATA 6
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

//Function Declaration
void disp_show();
extern String ds1;
extern String ds2;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


//Init OLED SSD1306 128X32
void disp_setup(){
  Wire.begin(I2C_DATA, I2C_CLOCK); //Redefine I2C Pins
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display(); //show logo..
  delay(2000);  //2 sec
  display.setTextColor(WHITE);
  display.setTextSize(2);
  //display.display();//show
}

void disp_show(){
      display.clearDisplay();
      display.setCursor(0,0); //координаты 1-й строки
      display.print(ds1);
      display.setCursor(0,18); //координаты 2-й строки
      display.print(ds2);
      display.display();//show
}
