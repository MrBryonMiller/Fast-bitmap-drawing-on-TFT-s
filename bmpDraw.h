#include <Adafruit_ZeroDMA.h>
#include <SD.h>
#include "Adafruit_SPIFlash_FatFs.h"
// Include the FatFs library header to use its low level functions
// directly.  Specifically the f_fdisk and f_mkfs functions are used
// to partition and create the filesystem.
#include "utility/ff.h"

#ifdef __SAMD51__
   #include "Adafruit_QSPI_GD25Q.h"
   Adafruit_QSPI_GD25Q flash;

#else
   #define FLASH_TYPE     SPIFLASHTYPE_W25Q16BV  // Flash chip type.
                                              // If you change this be
                                              // sure to change the fatfs
                                              // object type below to match.

   #define FLASH_SS       SS1                    // Flash chip SS pin.
   #define FLASH_SPI_PORT SPI1                   // What SPI port is Flash on?
   #define NEOPIN         40

   Adafruit_SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT);     // Use hardware SPI
#endif

Adafruit_W25Q16BV_FatFs fatfs(flash);


// Only device I know this works for is Feather M0 or M4 Express's
// otherwise there'll be a compile error.
#if defined(ARDUINO_SAMD_FEATHER_M0) || defined(__SAMD51__)
   #define STMPE_CS 6
   #define TFT_CS   9
   #define TFT_DC   10
   #define SD_CS    5
#endif


#define SPI_FREQ 24000000    // TFT: use max SPI (clips to 12 MHz on M0)
SPISettings settings(SPI_FREQ, MSBFIRST, SPI_MODE0);

#ifdef USEILI9341
   Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
#else
   Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC);
#endif

Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);

void bmpDraw(char *filename, uint16_t x, uint16_t y, bool loud);

void bmpDraw(char *filename, uint16_t x, uint16_t y) 
{
bmpDraw(filename,x,y,true);
}

void bmpFlashDraw(char *filename, uint16_t x, uint16_t y, bool loud);

void bmpFlashDraw(char *filename, uint16_t x, uint16_t y) 
{
bmpFlashDraw(filename,x,y,true);
}

void bmpFlashDrawDMA(char *filename, uint16_t x, uint16_t y, bool loud);

void bmpFlashDrawDMA(char *filename, uint16_t x, uint16_t y) 
{
bmpFlashDrawDMA(filename,x,y,1);
}

