/****
bitmapdraw_featherwing
highly modified by BryonMiller
this has been tested on AdaFruit 3.5 & 2.4 TFT featherwings
using Feather M4 Express and Feather M0 Express
below is the original comment
****/
/***************************************************
  This is our library for the Adafruit HX8357D FeatherWing
  ----> http://www.adafruit.com/products/3651

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

#define USEILI9341  // comment this line out for HX8357

#include <SPI.h>
#include <Adafruit_GFX.h>    // Core graphics library
#ifdef USEILI9341
  #include <Adafruit_ILI9341.h>
  char imgDirectory[] = "/ili9341";
#else
  #include <Adafruit_HX8357.h>
  char imgDirectory[] = "/hx8357";
#endif
#include <Adafruit_STMPE610.h>
#include "bmpDraw.h"

#define PENRADIUS 3
// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 3800
#define TS_MAXX 100
#define TS_MINY 100
#define TS_MAXY 3750

char imgPath[25];

void setup() {
pinMode(11,OUTPUT); // screen backlight

Serial.begin(115200);
int waitcnt = 0;
while(!Serial && (waitcnt++ < 100))  // wait 10 secs then go on even w/o Monitor
     delay(100);
  
Serial.println("TFTLCD Featherwing full control test!"); 

if (!ts.begin()) 
     {
     Serial.println("Couldn't start touchscreen controller");
     while (1);
     }
Serial.println("Touchscreen started");

tft.begin();
tft.setRotation(1);
tft.fillScreen(0);
digitalWrite(11,HIGH);

Serial.print("Initializing SD card...");
if (!SD.begin(SD_CS))
     Serial.println("failed!");
else Serial.println("OK!");
  
::File MyFlashFile;

bool flashFormatted = setupFlash();

setupDMA();

// start test
Serial.println("***reading adafruit 24 bit bmp from SD");
tft.fillScreen(0);
tft.setCursor(5,5);
tft.setTextSize(2);
tft.print(" 24 bit bmp from SD");
strcpy(imgPath,imgDirectory);
strcat(imgPath,"/adafruit.bmp");
bmpDraw(imgPath, 0, 0,true);

Serial.println("***reading adafruiu 8 bit bmp from SD");
tft.fillScreen(0);
tft.setCursor(5,5);
tft.print(" 8 bit, SD");
strcpy(imgPath,imgDirectory);
strcat(imgPath,"/adafruiu.bmp");
bmpDraw(imgPath, 0, 0,true);

Serial.println("***reading adafruiv 8 bit RLE bmp from SD");
tft.fillScreen(0);
tft.setCursor(5,5);
tft.print(" 8 bit, RLE, SD");
strcpy(imgPath,imgDirectory);
strcat(imgPath,"/adafruiv.bmp");
bmpDraw(imgPath, 0, 0,true);

Serial.println("***reading 24 bit bmp from SD");
tft.fillScreen(0);
tft.setCursor(5,5);
tft.print(" 24 bit, SD");
strcpy(imgPath,imgDirectory);
strcat(imgPath,"/adabot0.bmp");
bmpDraw(imgPath, 0, 0,true);

Serial.println("***reading 16 bit bmp from SD");
tft.fillScreen(0);
tft.setCursor(5,5);
tft.print(" 16 bit, SD");
strcpy(imgPath,imgDirectory);
strcat(imgPath,"/adabot1.bmp");
bmpDraw(imgPath, 0, 0,true);

Serial.println("***reading 8 bit bmp from SD");
tft.fillScreen(0);
tft.setCursor(5,5);
tft.print(" 8 bit, SD");
strcpy(imgPath,imgDirectory);
strcat(imgPath,"/adabot2.bmp");
bmpDraw(imgPath, 0, 0,true);

Serial.println("***reading 8 bit RLE bmp from SD");
tft.fillScreen(0);
tft.setCursor(5,5);
tft.print(" 8 bit, RLE, SD");
strcpy(imgPath,imgDirectory);
strcat(imgPath,"/adabot3.bmp");
bmpDraw(imgPath, 0, 0,true);

if (flashFormatted)
     {

     Serial.println("***reading 24 bit bmp from Flash");
     eraseScreen();
     tft.setCursor(5,5);
     tft.print(" 24 bit, Flash");
     strcpy(imgPath,imgDirectory);
     strcat(imgPath,"/adabot0.bmp");
     bmpFlashDraw(imgPath, 0, 0,true);

     Serial.println("***reading 16 bit bmp from Flash");
     eraseScreen();
     tft.setCursor(5,5);
     tft.print(" 16 bit, Flash");
     strcpy(imgPath,imgDirectory);
     strcat(imgPath,"/adabot1.bmp");
     bmpFlashDraw(imgPath, 0, 0,true);

     Serial.println("***reading 8 bit bmp from Flash");
     eraseScreen();
     tft.setCursor(5,5);
     tft.print(" 8 bit, Flash");
     strcpy(imgPath,imgDirectory);
     strcat(imgPath,"/adabot2.bmp");
     bmpFlashDraw(imgPath, 0, 0,true);

     Serial.println("***reading 8 bit RLE bmp from Flash");
     eraseScreen();
     tft.setCursor(5,5);
     tft.print(" 8 bit, RLE, Flash");
     strcpy(imgPath,imgDirectory);
     strcat(imgPath,"/adabot3.bmp");
     bmpFlashDraw(imgPath, 0, 0,true);

     Serial.println("***reading 24 bit bmp from Flash, using DMA");
     eraseScreen();
     tft.setCursor(5,5);
     tft.print("24 bit, Flash, w/DMA");
     strcpy(imgPath,imgDirectory);
     strcat(imgPath,"/adabot0.bmp");
     bmpFlashDrawDMA(imgPath, 0, 0,true);

     Serial.println("***reading 16 bit bmp from Flash, using DMA");
     eraseScreen();
     tft.setCursor(5,5);
     tft.print("16 bit, Flash, w/DMA");
     strcpy(imgPath,imgDirectory);
     strcat(imgPath,"/adabot1.bmp");
     bmpFlashDrawDMA(imgPath, 0, 0,true);

     Serial.println("***reading 8 bit bmp from Flash, using DMA");
     eraseScreen();
     tft.setCursor(5,5);
     tft.print("8 bit, Flash, w/DMA");
     strcpy(imgPath,imgDirectory);
     strcat(imgPath,"/adabot2.bmp");
     bmpFlashDrawDMA(imgPath, 0, 0,true);

     Serial.println("***reading 8 RLE bit bmp from Flash, using DMA");
     eraseScreen();
     tft.setCursor(5,5);
     tft.print("8 bit, RLE, Flash, w/DMA");
     strcpy(imgPath,imgDirectory);
     strcat(imgPath,"/adabot3.bmp");
     bmpFlashDrawDMA(imgPath, 0, 0,true);

     Serial.println("***reading 8 RLE bit bmp from Flash, using DMA");
     eraseScreen();
     tft.setCursor(5,5);
     tft.print("8 bit, RLE, Flash, w/DMA");
     strcpy(imgPath,imgDirectory);
     strcat(imgPath,"/adafruiv.bmp");
     bmpFlashDrawDMA(imgPath, 0, 0,true);
     }
}


void loop(void) {
  // Retrieve a point  
  TS_Point p = ts.getPoint();
  //Serial.print("X = "); Serial.print(p.x);  Serial.print("\tY = "); Serial.print(p.y);  Serial.print("\tPressure = "); Serial.println(p.z);  
 
  // Scale from ~0->4000 to tft.width using the calibration #'s
  p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());

  if (((p.y-PENRADIUS) > 0) && ((p.y+PENRADIUS) < tft.height())) {
    tft.fillCircle(p.x, p.y, PENRADIUS, 0xF800);
  }
}
