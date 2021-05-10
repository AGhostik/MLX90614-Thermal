#include <Wire.h>
#include <SPI.h>
#include <Adafruit_MLX90614.h>
#include <MPU6050_light.h>
#include <Arduino_ST7735_Fast.h>

// Wiring:
// TFT display -> Arduino Nano
// VCC -> 5V
// GND -> GND
// CS -> D10
// RESET -> D12
// SDA -> D11 MOSI
// SCK -> D13 SCLK
// LED -> D9
// For the breakout board, you can use any 2 or 3 pins.
// These pins will also work for the 1.8" TFT shield.
#define TFT_CS 10
#define TFT_RST -1 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC 8
//#define TFT_MOSI 2  // Data out
//#define TFT_SCLK 3  // Clock out

Arduino_ST7735 tft = Arduino_ST7735(TFT_DC, TFT_RST, TFT_CS);
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
//Factory calibrated in wide temperature range: 
//-40 to 125°C for sensor temperature 
//-70 to 380°C for object temperature

MPU6050 mpu(Wire);

#define SCAN_BUTTON 2
#define MODE_BUTTON 3

bool isMode2;

void setup() {
  Serial.begin(9600);
  
  pinMode(SCAN_BUTTON, INPUT_PULLUP);
  pinMode(MODE_BUTTON, INPUT_PULLUP);
  
  tft.init();
  tft.powerSave(ST7735_POWSAVE);
  tft.setRotation(0);
  tft.setTextColor(WHITE);
  tft.setTextWrap(true);
  tft.setTextSize(1);
  tft.fillScreen(BLACK);

  tftPrintLine(1, "Initialize...");
  
  tftPrintLine(2, "Wire - ...");
  Wire.begin();
  tftPrintLine(2, "Wire - ok");
  
  tftPrintLine(3, "MLX90614 - ...");
  if (!mlx.begin())
  {
    tftPrintLine(3, "MLX90614 - error");
    while (true) {}
  }
  tftPrintLine(3, "MLX90614 - ok");
  
  tftPrintLine(4, "MPU6050 - ...");
  if (mpu.begin() != 0)
  {
    tftPrintLine(4, "MPU6050 - error");
    while (true) {}
  }
  tftPrintLine(4, "MPU6050 - ok");
  
  mpu.calcOffsets(true, true);
  //setMinMaxTemp(-70, 380);
  setMinMaxTemp(25, 42);
  isMode2 = true;
  delay(300);  
  cleanTft();
}

unsigned long lastScanMillis;
unsigned long lastModeMillis;
unsigned long lastSelectedCellMillis;

float maxTemp;
float minTemp;

// массив температурных значений для линейки
float tempStepArray[16];

// todo: сделать регулируемым в зависимости от настроенного разрешения
// размер квадратика
#define MODE2_PIXEL_SIZE 8

// todo: вынести в редактируемые настройки
// угол поворота (diff), необходимого для смены selectedCell
#define angle 2

// todo: сделать возможность записывать данные на sd карту
float cache[16][16];

char selectedCellX = 8;
char selectedCellY = 8;

void loop() {
  mpu.update();
  
  unsigned long nowMillis = millis();

  // смена режима
  if (!digitalRead(MODE_BUTTON) && lastModeMillis + 500 <= nowMillis)
  {
    switchMode();
    lastModeMillis = nowMillis;
  }

  // обновляем выбранную точку
  if (isMode2 && lastSelectedCellMillis + 100 <= nowMillis)
  {
    updateSelectedCell();
    lastSelectedCellMillis = nowMillis;
  }

  if (!digitalRead(SCAN_BUTTON) && lastScanMillis + 100 <= nowMillis)
  {
    if (isMode2)
      scanMode2();
    else
      scanMode1();
    
    lastScanMillis = nowMillis;
  }
}

// режим псевдо-тепловизора
void scanMode2()
{
  float temp = scan();

  cache[selectedCellX][selectedCellY] = temp;

  tft.fillRect(52, 135, 40, 8, BLACK);
  char str[8];
  getTemperatureString(temp, 2, str);
  tft.setTextColor(GREEN);
  tft.setCursor(52, 135);
  tft.print(str);
  tft.setTextColor(WHITE);

  uint16_t color = getTemperatureColor(temp);
  tft.fillRect(selectedCellX * 8, selectedCellY * 8, MODE2_PIXEL_SIZE, MODE2_PIXEL_SIZE, color);
}

// режим точечного сканирования
void scanMode1()
{
  float temp = scan();
  char str[8];
  getTemperatureString(temp, 2, str);

  tft.fillRect(0, 0, 128, 128, BLACK);
  tft.setTextSize(2);
  tft.setCursor(46, 80);
  tft.print(str);
}

// сканирует температуру и рисует ползунок
float scan()
{
  float temp = mlx.readObjectTempC();
  tft.fillRect(0, 144, 128, 4, BLACK);

  if (temp <= maxTemp && temp >= minTemp)
  {
    float range = maxTemp - minTemp;
    float delta = range / 128;
    float positionX = (maxTemp - temp)/delta;

    // заполняем 4 пикселя в той черной области "внутри" линейки
    tft.drawPixel(positionX, 144, GREEN);
    tft.drawPixel(positionX, 145, GREEN);
    tft.drawPixel(positionX, 146, GREEN);
    tft.drawPixel(positionX, 147, GREEN);
  }

  return temp;
}

// todo: разобраться в XYZ, сделать возможным сканирование в любом положении
void updateSelectedCell()
{
  auto testX = mpu.getAngleX();
  auto testY = mpu.getAngleY();
  auto testZ = mpu.getAngleZ();

  //Serial.print("x: ");
  //Serial.print(testX);
  //Serial.print(" y: ");
  //Serial.print(testY);
  //Serial.print(" z: ");
  //Serial.println(testZ);
  
  char x = 8 + (char)(testX / angle);
  char y = 8 + (char)(testY / angle);
  if (x > 15)
    x = 15;
  else if (x < 0)
    x = 0;

  if (y > 15)
    y = 15;
  else if (y < 0)
    y = 0;

  selectedCellX = x;
  selectedCellY = y;
}

void switchMode()
{
  isMode2 = !isMode2;
      
  cleanTft();

  // сброс настроек
  if (isMode2)
  {
    selectedCellX = 8;
    selectedCellY = 8;
    mpu.calcOffsets(true, true); // todo: возможно нужно убрать это
    tft.setTextSize(1);
  }
  else
  {
    tft.setTextSize(2);
  }
}

void cleanTft()
{
  if (isMode2)
  {
    tft.fillScreen(BLACK);
    drawTemperatureLiner();
    drawGrid();
  }
  else
  {
    tft.fillScreen(BLACK);
    drawTemperatureLiner();
  }
}

void drawTemperatureLiner()
{
  // пишем max и min температуру в конце линейки
  tft.setTextSize(1);
  
  char str[8];
  getTemperatureString(maxTemp, 0, str);
  tft.setCursor(0, 135);
  tft.print(str);

  int width = getTempWidth(minTemp);
  getTemperatureString(minTemp, 0, str);
  tft.setCursor((124 - width * 7), 135); // применяется костыль что бы двигать число справа; ширина символа 7px
  tft.print(str);

  for (int i=0; i<16; i++)
  {
    // ширина одного квадрата - 8px, т.к. 128px - ширина экрана
    // высота квадрата 17px - так сделано ради ползунка текущего значения
    // позиция 143 - это 160px - 17px
    tft.fillRect(i * 8, 143, 8, 17, getColor(i));
  }
  // заполняем часть линейки черным для ползунка текущего значения
  tft.fillRect(0, 144, 128, 4, BLACK);
  // итого получается градиентная линейка 12px снизу, 4px черная область для ползунка и 1px над ползунком
}

// todo: добавить меню настроек и возможность менять разрешение сканирования
// рисует сетку
void drawGrid()
{
  for (int i=0; i<16; i++)
  {
    auto value = i*8;
    tft.drawFastHLine(0, value, 128, WHITE);
    tft.drawFastVLine(value, 0, 128, WHITE);
  }
  tft.drawFastHLine(0, 128, 128, WHITE); // нижняя линия
}

// перерасчитывает температурную линейку
void setMinMaxTemp(float minValue, float maxValue)
{
  maxTemp = maxValue;
  minTemp = minValue;

  float range = maxValue - minValue;
  float tempStep = range / 16;
  range = maxValue;
  for (int i=0; i<16; i++)
  {
    range -= tempStep;
    tempStepArray[i] = range;
  }
}

// todo: расширить до 32 цветов
// возвращает подходящий цвет температуры
uint16_t getTemperatureColor(float temp)
{
  for (int i=0; i<16; i++)
  {
    if (temp > tempStepArray[i])
      return getColor(i);
  }
}

// todo: расширить до 32 цветов
// возвращает цвет по индексу температуры
// 0 - самый горячий цвет, 15 - самый холодный
uint16_t getColor(unsigned int index)
{
  switch(index)
  {
    case 0:
      return 0xF7B9;
    case 1:
      return 0xF74D;
    case 2:
      return 0xF6A4;
    case 3:
      return 0xF5E2;
    case 4:
      return 0xF501;
    case 5:
      return 0xEC21;
    case 6:
      return 0xE341;
    case 7:
      return 0xDA82;  
    case 8:
      return 0xD1A7;
    case 9:
      return 0xC0EE;
    case 10:
      return 0xA850;
    case 11:
      return 0x9011;  
    case 12:
      return 0x6811;
    case 13:
      return 0x4011;
    case 14:
      return 0x180E;
    case 15:
      return 0x0007;
  }
}

// возвращает строку вида "-123.45C"
// dec - количество знаков после запятой
void getTemperatureString(float value, int dec, char* output)
{
  int width = getTempWidth(value) + dec;
  
  dtostrf(value, width, dec, output);
  output[width] = 'C';
  output[width + 1] = 0;
}

// возвращает количество знаков float числа, учитывая знак минус; максимум для трехзначных чисел
char getTempWidth(float value)
{
  char width;
  if (value >= 100)
    width = 3;
  else if (value >= 10)
    width = 2;
  else if (value >= 0)
    width = 1;
  else if (value <= -100)
    width = 4;
  else if (value <= -10)
    width = 3;
  else if (value < 0)
    width = 2;
  return width;
}

void tftPrintLine(char lineNumber, char* str)
{
  char posY = 9*(lineNumber - 1);
  tft.fillRect(0, posY, 128, 9, BLACK);
  tft.setCursor(0, posY);
  tft.print(str);
}
