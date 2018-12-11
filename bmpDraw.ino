// SAMD boards use DMA 
// Two single-line 128-pixel buffers (16bpp) are used for DMA.
// Though you'd think fewer larger transfers would improve speed,
// multi-line buffering made no appreciable difference.
uint16_t          dmaBuf[2][128];
uint8_t           dmaIdx = 0; // Active DMA buffer # (alternate fill/send)
Adafruit_ZeroDMA  dma;
DmacDescriptor   *descriptor;

// DMA transfer-in-progress indicator and callback
static volatile bool dma_busy = false;
static void dma_callback(Adafruit_ZeroDMA *dma) { dma_busy = false; }

bool setupFlash()
{
bool flashFormatted = false;
::File MyFlashFile;

#ifdef __SAMD51__
if (flash.begin()) 
#else
if (flash.begin(FLASH_TYPE))
#endif
     {
     Serial.print("Flash chip JEDEC ID: 0x"); 
     Serial.println(flash.GetJEDECID(), HEX);
     fatfs.activate();
     if (fatfs.begin()) 
          {
          flashFormatted = true;
          }
     else Serial.println("Error, SPI flash not formatted.");
     }
return flashFormatted;
}
void setupDMA()
{
#ifdef ARDUINO_ARCH_SAMD
  // Set up SPI DMA on SAMD boards:
  int                dmac_id;
  volatile uint32_t *data_reg;
  if(&PERIPH_SPI == &sercom0) {
    dmac_id  = SERCOM0_DMAC_ID_TX;
    data_reg = &SERCOM0->SPI.DATA.reg;
    Serial.println("SERCOM0");
#if defined SERCOM1
  } else if(&PERIPH_SPI == &sercom1) {
    dmac_id  = SERCOM1_DMAC_ID_TX;
    data_reg = &SERCOM1->SPI.DATA.reg;
    Serial.println("SERCOM1");
#endif
#if defined SERCOM2
  } else if(&PERIPH_SPI == &sercom2) {
    dmac_id  = SERCOM2_DMAC_ID_TX;
    data_reg = &SERCOM2->SPI.DATA.reg;
    Serial.println("SERCOM2");
#endif
#if defined SERCOM3
  } else if(&PERIPH_SPI == &sercom3) {
    dmac_id  = SERCOM3_DMAC_ID_TX;
    data_reg = &SERCOM3->SPI.DATA.reg;
    Serial.println("SERCOM3");
#endif
#if defined SERCOM4
  } else if(&PERIPH_SPI == &sercom4) {
    dmac_id  = SERCOM4_DMAC_ID_TX;
    data_reg = &SERCOM4->SPI.DATA.reg;
    Serial.println("SERCOM4");
#endif
#if defined SERCOM5
  } else if(&PERIPH_SPI == &sercom5) {
    dmac_id  = SERCOM5_DMAC_ID_TX;
    data_reg = &SERCOM5->SPI.DATA.reg;
    Serial.println("SERCOM5");
#endif
  }

dma.allocate();
dma.setTrigger(dmac_id);
dma.setAction(DMA_TRIGGER_ACTON_BEAT);
descriptor = dma.addDescriptor(
                    NULL,               // move data
                    (void *)data_reg,   // to here
                    sizeof dmaBuf[0],   // this many...
                    DMA_BEAT_SIZE_BYTE, // bytes/hword/words
                    true,               // increment source addr?
                    false);             // increment dest addr?
dma.setCallback(dma_callback);

#endif // End SAMD-specific SPI DMA init
}

void eraseScreen()
{
#ifdef ARDUINO_ARCH_SAMD
uint32_t startTime,endTime;
int xmax,ymax;

// now use DMA to clear entire screen
startTime = millis();
memset(dmaBuf[dmaIdx], 0, sizeof(dmaBuf[0]));
// in case the DMA descriptor has been changed
descriptor->SRCADDR.reg = (uint32_t)&dmaBuf[dmaIdx] + sizeof(dmaBuf[0]);
descriptor->BTCNT.reg = 120*2;

int h = tft.height();
int w = tft.width();
// Set TFT address window to clipped image bounds
tft.startWrite(); // Start TFT transaction
SPI.beginTransaction(settings);
tft.setAddrWindow(0, 0, w, h);
digitalWrite(TFT_DC, HIGH);                      // Data mode
for (int i=0;i<h*w;i=i+120)
     {
     while(dma_busy); // Wait for prior DMA xfer to finish
     dma_busy = true;
     dma.startJob();
     }
while(dma_busy); // Wait for prior DMA xfer to finish

endTime = millis();
Serial.print(F("Screen cleared in "));
Serial.print(endTime - startTime);
Serial.println(" ms (DMA)");
#endif
}

#define MADCTL_MY  0x80     ///< Bottom to top
#define MADCTL_MX  0x40     ///< Right to left
#define MADCTL_MV  0x20     ///< Reverse Mode
#define MADCTL_ML  0x10     ///< LCD refresh Bottom to top
#define MADCTL_RGB 0x00     ///< Red-Green-Blue pixel order
#define MADCTL_BGR 0x08     ///< Blue-Green-Red pixel order
#define MADCTL_MH  0x04     ///< LCD refresh right to left

// function flipVOrder cause writing to go from bottom to top
//   we can't use a new rotation since it would also change left-to-right
void flipVOrder()
{
uint8_t m;
#ifdef USEILI9341
switch (tft.getRotation()) 
     {
     case 0:
          m = (MADCTL_MY | MADCTL_MX | MADCTL_BGR);
          break;
     case 1:
          m = (MADCTL_MX | MADCTL_MV | MADCTL_BGR);
          break;
     case 2:
          m = (MADCTL_BGR);
          break;
     case 3:
          m = (MADCTL_MY | MADCTL_MV | MADCTL_BGR);
          break;
     }
#else
switch (tft.getRotation()) 
     {
     case 0:
          m = MADCTL_MX | MADCTL_RGB;
          break;
     case 1:
          m = MADCTL_MX | MADCTL_MY  | MADCTL_MV | MADCTL_RGB;
          break;
     case 2:
          m = MADCTL_MY | MADCTL_RGB;
          break;
     case 3:
          m =  MADCTL_MV | MADCTL_RGB;
          break;
     }
#endif

tft.startWrite();
digitalWrite(TFT_CS, LOW);
digitalWrite(TFT_DC, LOW);
#ifdef USEILI9341
SPI.transfer(ILI9341_MADCTL); // Current TFT lib
#else
SPI.transfer(HX8357_MADCTL); // Current TFT lib
#endif
digitalWrite(TFT_DC, HIGH);
SPI.transfer(m);
delay(1);
digitalWrite(TFT_CS , HIGH);
tft.endWrite();
}

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 20
void bmpDraw(char *filename, uint16_t x, uint16_t y, bool loud) 

{
  SDFile   bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t bmpHeadersize;         // size of header
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint32_t compression;           // 0=24 bit, 3= 565
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint16_t colorTable[256];
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint8_t  b0,b1;
  uint16_t color;
  uint32_t pos = 0, startTime = millis();

if((x >= tft.width()) || (y >= tft.height())) 
     return;

if (loud)
     {
     Serial.print(F("+++bmpDraw Loading image '"));
     Serial.print(filename);
     Serial.println('\'');
     }

// Open requested file on SD card
if ((bmpFile = SD.open(filename)) == NULL) 
     {
     Serial.println(F("File not found"));
     return;
     }

// Parse BMP header
if(read16(bmpFile) == 0x4D42) 
     { // BMP signature
     if (loud)
          {
          Serial.print(F("File size: ")); 
          Serial.println(read32(bmpFile));
          }
     else read32(bmpFile);  // Read & ignore File Size bytes
     (void)read32(bmpFile); // Read & ignore creator bytes
     bmpImageoffset = read32(bmpFile); // Start of image data
     bmpHeadersize = read32(bmpFile); // 
     if (loud)
          {
          Serial.print(F("Image Offset: 0x")); 
          Serial.println(bmpImageoffset, HEX);
          Serial.print(F("Header size: ")); 
          Serial.println(bmpHeadersize);
          }
     bmpWidth  = read32(bmpFile);
     bmpHeight = read32(bmpFile);
     if(read16(bmpFile) == 1) 
          { // # planes -- must be '1'
          bmpDepth = read16(bmpFile); // bits per pixel
          compression = read32(bmpFile);
          if (loud)
               {
               Serial.print(F("Bit Depth: "));
               Serial.println(bmpDepth);
               Serial.print(F("Image size: "));
               Serial.print(bmpWidth);
               Serial.print('x');
               Serial.println(bmpHeight);
               Serial.print(F("Compression: "));
               Serial.println(compression);
               }
          if ((bmpDepth == 8)  || (bmpDepth == 4))
               {

               bmpFile.seek(0x2E);
               int numColors = read16(bmpFile);
               if (numColors == 0)
                    numColors = 1 << bmpDepth;
               if (loud)
                    {
                    Serial.print("Number of colors ");
                    Serial.println(numColors);
                    }
               bmpFile.seek(bmpHeadersize+0x0E);
               for (col = 0;col < numColors;col=col+8)
                    {
                    bmpFile.read(sdbuffer, 32);
                    buffidx = 0;
                    for (int i = 0; i < 8; i++)
                         {
                         b = sdbuffer[buffidx++];
                         g = sdbuffer[buffidx++];
                         r = sdbuffer[buffidx++];
                         colorTable[col+i] = tft.color565(r,g,b);
                         buffidx++;
                         }
                    }
               }
          if ((bmpDepth == 24) && (compression == 0) 
               || (bmpDepth == 16) && ((compression == 0) || (compression == 3))
               || (bmpDepth == 8) && ((compression == 0) || (compression == 3))) 
               { // All acceptable formats except RLE

               goodBmp = true; // Supported BMP format -- proceed!

               // BMP rows are padded (if needed) to 4-byte boundary
               if (bmpDepth == 24)
                    rowSize = (bmpWidth*3 + 3) & ~3;
               else if (bmpDepth == 16)
                         rowSize = (bmpWidth*2 + 3) & ~3;
                    else rowSize = (bmpWidth + 3) & ~3;
               // If bmpHeight is negative, image is in top-down order.
               // This is not canon but has been observed in the wild.
               if(bmpHeight < 0) 
                    {
                    bmpHeight = -bmpHeight;
                    flip      = false;
                    }

               // Crop area to be loaded
               w = bmpWidth;
               h = bmpHeight;
               if((x+w-1) >= tft.width())  
                    w = tft.width()  - x;
               if((y+h-1) >= tft.height()) 
                    h = tft.height() - y;

               if (flip)
                    flipVOrder();
               y = tft.height()-y-h;
//             // Set TFT address window to clipped image bounds
               tft.startWrite(); // Start TFT transaction
               tft.setAddrWindow(x, y, w, h);

               buffidx = sizeof(sdbuffer); // Force buffer reload
               bmpFile.seek(bmpImageoffset);
               for (row=bmpHeight; row>0; row--) 
                    { // For each scanline...
                    // make sure every row starts on 4-byte boundary
                    while (buffidx & 3)
                         buffidx++;
                    for (col=0; col<bmpWidth; col++) 
                         { // For each pixel...
                         // Time to read more pixel data?   
                         if (buffidx >= sizeof(sdbuffer)) 
                              { // Indeed
                              tft.endWrite(); // End TFT transaction
                              bmpFile.read(sdbuffer, sizeof(sdbuffer));
                              buffidx = buffidx - sizeof(sdbuffer); // Set index to beginning
                              tft.startWrite(); // Start new TFT transaction
                              }

                         // Convert pixel from BMP to TFT format, push to display
                         if (bmpDepth == 24)
                              {
                              b = sdbuffer[buffidx++];
                              g = sdbuffer[buffidx++];
                              r = sdbuffer[buffidx++];
                              color = tft.color565(r,g,b);
                              }
                         else if (bmpDepth == 16)
                                   {
                                   b0 = sdbuffer[buffidx++];
                                   b1 = sdbuffer[buffidx++];
                                   ((uint8_t *)&color)[0] = b0; // LSB
                                   ((uint8_t *)&color)[1] = b1; // MSB
                                   }
                              else {
                                   b0 = sdbuffer[buffidx++];
                                   color = colorTable[b0];
                                   }
                         if ((col < w) && (row <= h))
                              tft.pushColor(color);
                         
                         } // end pixel
                    } // end scanline
               tft.endWrite(); // End last TFT transaction
               if (flip)
                    tft.setRotation(tft.getRotation());
               if (loud)
                    {
                    Serial.print(F("====Loaded in "));
                    Serial.print(millis() - startTime);
                    Serial.println(" ms");
                    }
               } // end uncompressed
          if ((bmpDepth == 8) && (compression == 1)
               || (bmpDepth == 4) && (compression == 2)) 
               { // RLE encoding
               goodBmp = true; // Supported BMP format -- proceed!
               // Crop area to be loaded
               w = bmpWidth;
               h = bmpHeight;
               if((x+w-1) >= tft.width())  
                    w = tft.width()  - x;
               if((y+h-1) >= tft.height()) 
                    h = tft.height() - y;

               // since BMP file is top to bottom we are going to flip the
               // controllers drawing order.  Because that is flipped we also need
               // to flip our starting "y".  May be possible to find a RLE encoded
               // file without being in flipped order, but I haven't seen one.
               flipVOrder();
               y = tft.height()-y-h;
               tft.startWrite(); // Start TFT transaction
               // Set TFT address window to clipped image bounds
               tft.setAddrWindow(x, y, w, h);

               buffidx = sizeof(sdbuffer); // Force buffer reload
               bmpFile.seek(bmpImageoffset);
               for (row=bmpHeight; row>0; row--) 
                    { // For each scanline...
                    // decode per http://www.martinreddy.net/gfx/2d/BMP.txt
                    bool cont = true;
                    int uncompressedDataLen = 0;
                    int consecutivePixelsDataLen = 0;
                    bool HOnib = true;  // High Order nibble
                                        // only used for 16 colors or less
                    col = 0;
                    
                    while (cont) 
                         { // For each pixel...
                         // Time to read more pixel data?
                         if (buffidx >= sizeof(sdbuffer)) 
                              { // Indeed
                              tft.endWrite(); // End TFT transaction
                              bmpFile.read(sdbuffer, sizeof(sdbuffer));
                              buffidx = 0; // Set index to beginning
                              tft.startWrite(); // Start new TFT transaction
                              }
                              
//                              new stuff
 
                         if (consecutivePixelsDataLen)
                              {
                              if (compression == 2)
                                   {
                                   if (HOnib)
                                        color = colorTable[b1 >> 4];
                                   else color = colorTable[b1 & 0xF];
                                   HOnib = !HOnib;
                                   }
                              else color = colorTable[b1];
                              if ((col < w) && (row <= h))
                                   {
                                   tft.pushColor(color);
                                   }
                              col++;
                              --consecutivePixelsDataLen;
                              }
                         else {
                              if (uncompressedDataLen)
                                   {
                                   if (compression == 2)
                                        {
                                        if (HOnib)
                                             {
                                             b0 = sdbuffer[buffidx++];
                                             color = colorTable[b0 >> 4];
                                             }
                                        else color = colorTable[b0 & 0xF];
                                        HOnib = !HOnib;
                                        }
                                   else {
                                        b0 = sdbuffer[buffidx++];
                                        color = colorTable[b0];
                                        }
                                   if ((col < w) && (row <= h))
                                        {
                                        tft.pushColor(color);
                                        }
                                   col++;
                                   --uncompressedDataLen;
                                   // make sure it ends on an even boundary
                                   if ((uncompressedDataLen == 0) && ((buffidx & 1) == 1))
                                        buffidx++;
                                   }
                              // b0 (when > 0) num of consecutive pixels
                              else {
                                   b0 = sdbuffer[buffidx++];
                                   b1 = sdbuffer[buffidx++];
                                   if (b0)
                                        {// encoded mode
                                        // b1 is color index
                                        consecutivePixelsDataLen = b0;
                                        HOnib = true;
                                        }
                                   else {// absolute Mode
                                        if (b1==0) // EOL
                                             cont = false;
                                        if (b1==1) // EOF
                                             cont = false;
                                        if (b1==2) // ??
                                             {
                                             cont = false;
                                             Serial.print("unknown decode at ");
                                             Serial.println(pos);
                                             }
                                        if (b1 >= 3) //Uncompressed data mode
                                             {
                                             uncompressedDataLen = b1;
                                             HOnib = true;
                                             }
                                        }
                                   }
                              }
                         } // end pixel
                    } // end scanline
               tft.endWrite(); // End last TFT transaction
               // restore the Rotation for rest of the Program
               tft.setRotation(tft.getRotation());
               if (loud)
                    {
                    Serial.print(F("====Loaded in "));
                    Serial.print(millis() - startTime);
                    Serial.println(" ms");
                    }
               } // end uncompressed
          }
     }

bmpFile.close();
if(!goodBmp) 
     Serial.println(F("BMP format not recognized."));
}

void bmpFlashDraw(char *filename, uint16_t x, uint16_t y, bool loud) 

{
  ::File   bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t bmpHeadersize;         // size of header
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint32_t compression;           // 0=24 bit, 3= 565
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint16_t colorTable[256];
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint8_t  b0,b1;
  uint16_t color;
  uint32_t pos = 0, startTime = millis();

if((x >= tft.width()) || (y >= tft.height())) 
     return;

if (loud)
     {
     Serial.print(F("+++bmpFlashDraw Loading image '"));
     Serial.print(filename);
     Serial.println('\'');
     }

// Open requested file on QSPI Flash
if ((bmpFile = fatfs.open(filename)) == NULL) 
     {
     Serial.println(F("File not found"));
     return;
     }

// Parse BMP header
if(read16(bmpFile) == 0x4D42) 
     { // BMP signature
     if (loud)
          {
          Serial.print(F("File size: ")); 
          Serial.println(read32(bmpFile));
          }
     else read32(bmpFile);  // Read & ignore File Size bytes
     (void)read32(bmpFile); // Read & ignore creator bytes
     bmpImageoffset = read32(bmpFile); // Start of image data
     bmpHeadersize = read32(bmpFile); // 
     if (loud)
          {
          Serial.print(F("Image Offset: 0x")); 
          Serial.println(bmpImageoffset, HEX);
          Serial.print(F("Header size: ")); 
          Serial.println(bmpHeadersize);
          }
     bmpWidth  = read32(bmpFile);
     bmpHeight = read32(bmpFile);
     if(read16(bmpFile) == 1) 
          { // # planes -- must be '1'
          bmpDepth = read16(bmpFile); // bits per pixel
          compression = read32(bmpFile);
          if (loud)
               {
               Serial.print(F("Bit Depth: "));
               Serial.println(bmpDepth);
               Serial.print(F("Image size: "));
               Serial.print(bmpWidth);
               Serial.print('x');
               Serial.println(bmpHeight);
               Serial.print(F("Compression: "));
               Serial.println(compression);
               }
          if ((bmpDepth == 8)  || (bmpDepth == 4))
               {

               bmpFile.seek(0x2E);
               int numColors = read16(bmpFile);
               if (numColors == 0)
                    numColors = 1 << bmpDepth;
               if (loud)
                    {
                    Serial.print("Number of colors ");
                    Serial.println(numColors);
                    }
               bmpFile.seek(bmpHeadersize+0x0E);
               for (col = 0;col < numColors;col=col+8)
                    {
                    bmpFile.read(sdbuffer, 32);
                    buffidx = 0;
                    for (int i = 0; i < 8; i++)
                         {
                         b = sdbuffer[buffidx++];
                         g = sdbuffer[buffidx++];
                         r = sdbuffer[buffidx++];
                         colorTable[col+i] = tft.color565(r,g,b);
                         buffidx++;
                         }
                    }
               }
          if ((bmpDepth == 24) && (compression == 0) 
               || (bmpDepth == 16) && ((compression == 0) || (compression == 3))
               || (bmpDepth == 8) && ((compression == 0) || (compression == 3))) 
               { // All acceptable formats except RLE

               goodBmp = true; // Supported BMP format -- proceed!

               // BMP rows are padded (if needed) to 4-byte boundary
               if (bmpDepth == 24)
                    rowSize = (bmpWidth*3 + 3) & ~3;
               else if (bmpDepth == 16)
                         rowSize = (bmpWidth*2 + 3) & ~3;
                    else rowSize = (bmpWidth + 3) & ~3;
               // If bmpHeight is negative, image is in top-down order.
               // This is not canon but has been observed in the wild.
               if(bmpHeight < 0) 
                    {
                    bmpHeight = -bmpHeight;
                    flip      = false;
                    }

               // Crop area to be loaded
               w = bmpWidth;
               h = bmpHeight;
               if((x+w-1) >= tft.width())  
                    w = tft.width()  - x;
               if((y+h-1) >= tft.height()) 
                    h = tft.height() - y;

               if (flip)
                    flipVOrder();
               y = tft.height()-y-h;
//             // Set TFT address window to clipped image bounds
               tft.startWrite(); // Start TFT transaction
               tft.setAddrWindow(x, y, w, h);

               buffidx = sizeof(sdbuffer); // Force buffer reload
               bmpFile.seek(bmpImageoffset);
               for (row=bmpHeight; row>0; row--) 
                    { // For each scanline...
                    // make sure every row starts on 4-byte boundary
                    while (buffidx & 3)
                         buffidx++;
                    for (col=0; col<bmpWidth; col++) 
                         { // For each pixel...
                         // Time to read more pixel data?   
                         if (buffidx >= sizeof(sdbuffer)) 
                              { // Indeed
                              tft.endWrite(); // End TFT transaction
                              bmpFile.read(sdbuffer, sizeof(sdbuffer));
                              buffidx = buffidx - sizeof(sdbuffer); // Set index to beginning
                              tft.startWrite(); // Start new TFT transaction
                              }

                         // Convert pixel from BMP to TFT format, push to display
                         if (bmpDepth == 24)
                              {
                              b = sdbuffer[buffidx++];
                              g = sdbuffer[buffidx++];
                              r = sdbuffer[buffidx++];
                              color = tft.color565(r,g,b);
                              }
                         else if (bmpDepth == 16)
                                   {
                                   b0 = sdbuffer[buffidx++];
                                   b1 = sdbuffer[buffidx++];
                                   ((uint8_t *)&color)[0] = b0; // LSB
                                   ((uint8_t *)&color)[1] = b1; // MSB
                                   }
                              else {
                                   b0 = sdbuffer[buffidx++];
                                   color = colorTable[b0];
                                   }
                         if ((col < w) && (row <= h))
                              tft.pushColor(color);
                         
                         } // end pixel
                    } // end scanline
               tft.endWrite(); // End last TFT transaction
               if (flip)
                    tft.setRotation(tft.getRotation());
               if (loud)
                    {
                    Serial.print(F("====Loaded in "));
                    Serial.print(millis() - startTime);
                    Serial.println(" ms");
                    }
               } // end uncompressed
          if ((bmpDepth == 8) && (compression == 1)
               || (bmpDepth == 4) && (compression == 2)) 
               { // RLE encoding
               goodBmp = true; // Supported BMP format -- proceed!
               // Crop area to be loaded
               w = bmpWidth;
               h = bmpHeight;
               if((x+w-1) >= tft.width())  
                    w = tft.width()  - x;
               if((y+h-1) >= tft.height()) 
                    h = tft.height() - y;

               // since BMP file is top to bottom we are going to flip the
               // controllers drawing order.  Because that is flipped we also need
               // to flip our starting "y".  May be possible to find a RLE encoded
               // file without being in flipped order, but I haven't seen one.
               flipVOrder();
               y = tft.height()-y-h;
               tft.startWrite(); // Start TFT transaction
               // Set TFT address window to clipped image bounds
               tft.setAddrWindow(x, y, w, h);

               buffidx = sizeof(sdbuffer); // Force buffer reload
               bmpFile.seek(bmpImageoffset);
               for (row=bmpHeight; row>0; row--) 
                    { // For each scanline...
                    // decode per http://www.martinreddy.net/gfx/2d/BMP.txt
                    bool cont = true;
                    int uncompressedDataLen = 0;
                    int consecutivePixelsDataLen = 0;
                    bool HOnib = true;  // High Order nibble
                                        // only used for 16 colors or less
                    col = 0;
                    
                    while (cont) 
                         { // For each pixel...
                         // Time to read more pixel data?
                         if (buffidx >= sizeof(sdbuffer)) 
                              { // Indeed
                              tft.endWrite(); // End TFT transaction
                              bmpFile.read(sdbuffer, sizeof(sdbuffer));
                              buffidx = 0; // Set index to beginning
                              tft.startWrite(); // Start new TFT transaction
                              }
                         if (consecutivePixelsDataLen)
                              {
                              if (compression == 2)
                                   {
                                   if (HOnib)
                                        color = colorTable[b1 >> 4];
                                   else color = colorTable[b1 & 0xF];
                                   HOnib = !HOnib;
                                   }
                              else color = colorTable[b1];
                              if ((col < w) && (row <= h))
                                   {
                                   tft.pushColor(color);
                                   }
                              col++;
                              --consecutivePixelsDataLen;
                              }
                         else {
                              if (uncompressedDataLen)
                                   {
                                   if (compression == 2)
                                        {
                                        if (HOnib)
                                             {
                                             b0 = sdbuffer[buffidx++];
                                             color = colorTable[b0 >> 4];
                                             }
                                        else color = colorTable[b0 & 0xF];
                                        HOnib = !HOnib;
                                        }
                                   else {
                                        b0 = sdbuffer[buffidx++];
                                        color = colorTable[b0];
                                        }
                                   if ((col < w) && (row <= h))
                                        {
                                        tft.pushColor(color);
                                        }
                                   col++;
                                   --uncompressedDataLen;
                                   // make sure it ends on an even boundary
                                   if ((uncompressedDataLen == 0) && ((buffidx & 1) == 1))
                                        buffidx++;
                                   }
                              // b0 (when > 0) num of consecutive pixels
                              else {
                                   b0 = sdbuffer[buffidx++];
                                   b1 = sdbuffer[buffidx++];
                                   if (b0)
                                        {// encoded mode
                                        // b1 is color index
                                        consecutivePixelsDataLen = b0;
                                        HOnib = true;
                                        }
                                   else {// absolute Mode
                                        if (b1==0) // EOL
                                             cont = false;
                                        if (b1==1) // EOF
                                             cont = false;
                                        if (b1==2) // ??
                                             {
                                             cont = false;
                                             Serial.print("unknown decode at ");
                                             Serial.println(pos);
                                             }
                                        if (b1 >= 3) //Uncompressed data mode
                                             {
                                             uncompressedDataLen = b1;
                                             HOnib = true;
                                             }
                                        }
                                   }
                              }
                         } // end pixel
                    } // end scanline
               tft.endWrite(); // End last TFT transaction
               // restore the Rotation for rest of the Program
               tft.setRotation(tft.getRotation());
               if (loud)
                    {
                    Serial.print(F("====Loaded in "));
                    Serial.print(millis() - startTime);
                    Serial.println(" ms");
                    }
               } // end uncompressed
          }
     }

bmpFile.close();
if(!goodBmp) 
     Serial.println(F("BMP format not recognized."));
}

void bmpFlashDrawDMA(char *filename, uint16_t x, uint16_t y, bool loud) 

{
  ::File   bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t bmpHeadersize;         // size of header
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint32_t compression;           // 0=24 bit, 3= 565
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint16_t colorTable[256];
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint8_t  b0,b1;
  uint16_t color;
  uint32_t pos = 0, startTime = millis();

if((x >= tft.width()) || (y >= tft.height())) 
     return;

if (loud)
     {
     Serial.print(F("+++bmpFlashDrawDMA Loading image '"));
     Serial.print(filename);
     Serial.println('\'');
     }

// Open requested file on QSPI Flash
if ((bmpFile = fatfs.open(filename)) == NULL) 
     {
     Serial.println(F("File not found"));
     return;
     }

// Parse BMP header
if(read16(bmpFile) == 0x4D42) 
     { // BMP signature
     if (loud)
          {
          Serial.print(F("File size: ")); 
          Serial.println(read32(bmpFile));
          }
     else read32(bmpFile);  // Read & ignore File Size bytes
     (void)read32(bmpFile); // Read & ignore creator bytes
     bmpImageoffset = read32(bmpFile); // Start of image data
     bmpHeadersize = read32(bmpFile); // 
     if (loud)
          {
          Serial.print(F("Image Offset: 0x")); 
          Serial.println(bmpImageoffset, HEX);
          Serial.print(F("Header size: ")); 
          Serial.println(bmpHeadersize);
          }
     bmpWidth  = read32(bmpFile);
     bmpHeight = read32(bmpFile);
     if(read16(bmpFile) == 1) 
          { // # planes -- must be '1'
          bmpDepth = read16(bmpFile); // bits per pixel
          compression = read32(bmpFile);
          if (loud)
               {
               Serial.print(F("Bit Depth: "));
               Serial.println(bmpDepth);
               Serial.print(F("Image size: "));
               Serial.print(bmpWidth);
               Serial.print('x');
               Serial.println(bmpHeight);
               Serial.print(F("Compression: "));
               Serial.println(compression);
               }
          if ((bmpDepth == 8)  || (bmpDepth == 4))
               {

               bmpFile.seek(0x2E);
               int numColors = read16(bmpFile);
               if (numColors == 0)
                    numColors = 1 << bmpDepth;
               if (loud)
                    {
                    Serial.print("Number of colors ");
                    Serial.println(numColors);
                    }
               bmpFile.seek(bmpHeadersize+0x0E);
               for (col = 0;col < numColors;col=col+8)
                    {
                    bmpFile.read(sdbuffer, 32);
                    buffidx = 0;
                    for (int i = 0; i < 8; i++)
                         {
                         b = sdbuffer[buffidx++];
                         g = sdbuffer[buffidx++];
                         r = sdbuffer[buffidx++];
                         colorTable[col+i] = tft.color565(r,g,b);
                         buffidx++;
                         }
                    }
               }
          if ((bmpDepth == 24) && (compression == 0) 
               || (bmpDepth == 16) && ((compression == 0) || (compression == 3))
               || (bmpDepth == 8) && ((compression == 0) || (compression == 3))) 
               { // All acceptable formats except RLE

               goodBmp = true; // Supported BMP format -- proceed!

               // BMP rows are padded (if needed) to 4-byte boundary
               if (bmpDepth == 24)
                    rowSize = (bmpWidth*3 + 3) & ~3;
               else if (bmpDepth == 16)
                         rowSize = (bmpWidth*2 + 3) & ~3;
                    else rowSize = (bmpWidth + 3) & ~3;
               // If bmpHeight is negative, image is in top-down order.
               // This is not canon but has been observed in the wild.
               if(bmpHeight < 0) 
                    {
                    bmpHeight = -bmpHeight;
                    flip      = false;
                    }

               // Crop area to be loaded
               w = bmpWidth;
               h = bmpHeight;
               if((x+w-1) >= tft.width())  
                    w = tft.width()  - x;
               if((y+h-1) >= tft.height()) 
                    h = tft.height() - y;

               if (flip)
                    flipVOrder();
               y = tft.height()-y-h;
               // make sure we know where we are starting
               descriptor->BTCNT.reg = sizeof(dmaBuf[0]);
               descriptor->SRCADDR.reg = (uint32_t)&dmaBuf[dmaIdx] + sizeof(dmaBuf[0]);
               // Set TFT address window to clipped image bounds
               tft.startWrite(); // Start TFT transaction
               SPI.beginTransaction(settings);
               tft.setAddrWindow(x, y, w, h);
               digitalWrite(TFT_DC, HIGH);                      // Data mode

               uint16_t *ptr = &dmaBuf[dmaIdx][0];
               uint16_t iptr = 0;
               buffidx = sizeof(sdbuffer); // Force buffer reload
               bmpFile.seek(bmpImageoffset);
               for (row=bmpHeight; row>0; row--) 
                    { // For each scanline...
                    // make sure every row starts on 4-byte boundary
                    while (buffidx & 3)
                         buffidx++;
                    for (col=0; col<bmpWidth; col++) 
                         { // For each pixel...
                         // Time to read more pixel data?   
                         if (buffidx >= sizeof(sdbuffer)) 
                              { // Indeed
//                            tft.endWrite(); // End TFT transaction
                              bmpFile.read(sdbuffer, sizeof(sdbuffer));
                              buffidx = buffidx - sizeof(sdbuffer); // Set index to beginning
//                            tft.startWrite(); // Start new TFT transaction
                              }

                         // Convert pixel from BMP to TFT format, write to DMA buff
                         if (bmpDepth == 24)
                              {
                              b = sdbuffer[buffidx++];
                              g = sdbuffer[buffidx++];
                              r = sdbuffer[buffidx++];
                              color = tft.color565(r,g,b);
                              }
                         else if (bmpDepth == 16)
                                   {
                                   b0 = sdbuffer[buffidx++];
                                   b1 = sdbuffer[buffidx++];
                                   ((uint8_t *)&color)[0] = b0; // LSB
                                   ((uint8_t *)&color)[1] = b1; // MSB
                                   }
                              else {
                                   b0 = sdbuffer[buffidx++];
                                   color = colorTable[b0];
                                   }
                         if ((col < w) && (row <= h))
                              {
                              *ptr++ = __builtin_bswap16(color);
                              iptr++;
                              }
                         if (iptr*2 >= sizeof(dmaBuf[0]))
                              {
                              // Serial.print(row);
                              // Serial.print(", ");
                              // Serial.println(col);
                              while(dma_busy); // Wait for prior DMA xfer to finish
                              descriptor->SRCADDR.reg = (uint32_t)&dmaBuf[dmaIdx] + sizeof(dmaBuf[0]);
                              dma_busy = true;
                              dma.startJob();
                              dmaIdx   = 1 - dmaIdx;
                              ptr = &dmaBuf[dmaIdx][0];
                              iptr = 0;
                              }
                         } // end pixel
                    } // end scanline
               if (iptr >0)
                    {
                    while(dma_busy); // Wait for prior DMA xfer to finish
                    descriptor->BTCNT.reg = iptr*2;
                    descriptor->SRCADDR.reg = (uint32_t)&dmaBuf[dmaIdx] + iptr*2;
                    dma_busy = true;
                    dma.startJob();
                    }
               while(dma_busy);  // Wait for last scanline to transmit
               tft.endWrite(); // End last TFT transaction

//             digitalWrite(eyeInfo[0].select, HIGH);          // Deselect
               SPI.endTransaction();
               if (flip)
                    tft.setRotation(tft.getRotation());
               if (loud)
                    {
                    Serial.print(F("====Loaded in "));
                    Serial.print(millis() - startTime);
                    Serial.println(" ms");
                    }
               } 
          if ((bmpDepth == 8) && (compression == 1)
               || (bmpDepth == 4) && (compression == 2)) 
               { // RLE encoding
               goodBmp = true; // Supported BMP format -- proceed!
               // Crop area to be loaded
               w = bmpWidth;
               h = bmpHeight;
               if((x+w-1) >= tft.width())  
                    w = tft.width()  - x;
               if((y+h-1) >= tft.height()) 
                    h = tft.height() - y;

               // make sure we know where we are starting
               descriptor->BTCNT.reg = sizeof(dmaBuf[0]);
               descriptor->SRCADDR.reg = (uint32_t)&dmaBuf[dmaIdx] + sizeof(dmaBuf[0]);
               flipVOrder();
               // Set TFT address window to clipped image bounds
               tft.startWrite(); // Start TFT transaction
               y = tft.height()-y-h;
//             Serial.println(y);
               SPI.beginTransaction(settings);
               tft.setAddrWindow(x, y, w, h);
               digitalWrite(TFT_DC, HIGH);                      // Data mode

               uint16_t *ptr = &dmaBuf[dmaIdx][0];
               uint16_t iptr = 0;
               buffidx = sizeof(sdbuffer); // Force buffer reload
               bmpFile.seek(bmpImageoffset);
               for (row=bmpHeight; row>0; row--) 
                    { // For each scanline...
                    // decode per http://www.martinreddy.net/gfx/2d/BMP.txt
                    bool cont = true;
                    int uncompressedDataLen = 0;
                    int consecutivePixelsDataLen = 0;
                    bool HOnib = true;  // High Order nibble
                                        // only used for 16 colors or less
                    col = 0;
                    while (cont) 
                         { // For each pixel...
                         // Time to read more pixel data?
                         if (buffidx >= sizeof(sdbuffer)) 
                              { // Indeed
                              bmpFile.read(sdbuffer, sizeof(sdbuffer));
                              buffidx = 0; // Set index to beginning
                              }
                         if (consecutivePixelsDataLen)
                              {
                              if (compression == 2)
                                   {
                                   if (HOnib)
                                        color = colorTable[b1 >> 4];
                                   else color = colorTable[b1 & 0xF];
                                   HOnib = !HOnib;
                                   }
                              else color = colorTable[b1];
                              if ((col < w) && (row <= h))
                                   {
                                   *ptr++ = __builtin_bswap16(color);
                                   iptr++;
                                   }
                              col++;
                              --consecutivePixelsDataLen;
                              }
                         else {
                              if (uncompressedDataLen)
                                   {
                                   if (compression == 2)
                                        {
                                        if (HOnib)
                                             {
                                             b0 = sdbuffer[buffidx++];
                                             color = colorTable[b0 >> 4];
                                             }
                                        else color = colorTable[b0 & 0xF];
                                        HOnib = !HOnib;
                                        }
                                   else {
                                        b0 = sdbuffer[buffidx++];
                                        color = colorTable[b0];
                                        }
                                   if ((col < w) && (row <= h))
                                        {
                                        *ptr++ = __builtin_bswap16(color);
                                        iptr++;
                                        }
                                   col++;
                                   --uncompressedDataLen;
                                   // make sure it ends on an even boundary
                                   if ((uncompressedDataLen == 0) && ((buffidx & 1) == 1))
                                        buffidx++;
                                   }
                              // b0 (when > 0) num of consecutive pixels
                              else {
                                   b0 = sdbuffer[buffidx++];
                                   b1 = sdbuffer[buffidx++];
                                   if (b0)
                                        {// encoded mode
                                        // b1 is color index
                                        //color = colorTable[b1];
                                        consecutivePixelsDataLen = b0;
                                        HOnib = true;
                                        }
                                   else {// absolute Mode
                                        if (b1==0) // EOL
                                             cont = false;
                                        if (b1==1) // EOF
                                             cont = false;
                                        if (b1==2) // ??
                                             {
                                             cont = false;
                                             Serial.print("unknown decode at ");
                                             Serial.println(pos);
                                             }
                                        if (b1 >= 3) //Uncompressed data mode
                                             {
                                             uncompressedDataLen = b1;
                                             HOnib = true;
                                             }
                                        }
                                   }
                              }
                         if (iptr*2 >= sizeof(dmaBuf[0]))
                              {
                              // Serial.print(row);
                              // Serial.print(", ");
                              // Serial.println(col);
                              while(dma_busy); // Wait for prior DMA xfer to finish
                              descriptor->SRCADDR.reg = (uint32_t)&dmaBuf[dmaIdx] + sizeof(dmaBuf[0]);
                              dma_busy = true;
                              dma.startJob();
                              dmaIdx   = 1 - dmaIdx;
                              ptr = &dmaBuf[dmaIdx][0];
                              iptr = 0;
                              }
                         } // end pixel
                    } // end scanline
               if (iptr >0)
                    {
                    while(dma_busy); // Wait for prior DMA xfer to finish
                    descriptor->BTCNT.reg = iptr*2;
                    descriptor->SRCADDR.reg = (uint32_t)&dmaBuf[dmaIdx] + iptr*2;
                    dma_busy = true;
                    dma.startJob();
                    }
               while(dma_busy);  // Wait for last scanline to transmit
               tft.endWrite(); // End last TFT transaction

               SPI.endTransaction();
               tft.setRotation(tft.getRotation());
               if (loud)
                    {
                    Serial.print(F("====Loaded in "));
                    Serial.print(millis() - startTime);
                    Serial.println(" ms");
                    }
               } // end uncompressed
          }
     }

bmpFile.close();

if(!goodBmp) 
     Serial.println(F("BMP format not recognized."));
}


// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(SDFile &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(SDFile &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

uint16_t read16(::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

