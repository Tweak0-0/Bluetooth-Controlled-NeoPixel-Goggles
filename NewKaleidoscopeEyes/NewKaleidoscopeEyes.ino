// Bluetooth Goggles Sketch -- shows the Adafruit Bluefruit LE UART Friend
// can be used even with Trinket or Gemma!

// https://www.adafruit.com/products/2479

// Works in conjunction with Bluefruit LE Connect app on iOS or Android --
// pick colors or use '1' and '2' buttons to select pinwheel or sparkle modes.
// You can try adding more, but space is VERY tight...helps to use Arduino IDE
// 1.6.4 or later; produces slightly smaller code than the 1.0.X releases.

// BLUEFRUIT LE UART FRIEND MUST BE SWITCHED TO 'UART' MODE

#include <SoftwareSerial.h>
//#include <Adafruit_NeoPixel.h>
#include "Adafruit_NeoPixel.h"
#ifdef __AVR_ATtiny85__ // Trinket, Gemma, etc.
 #include <avr/power.h>
#endif

#define RX_PIN    0   // original value (2) Connect this Trinket pin to BLE 'TXO' pin
#define CTS_PIN   13  // original value (1) Connect this Trinket pin to BLE 'CTS' pin
#define LED_PIN   6   // original value (0) Connect NeoPixels to this Trinket pin
#define NUM_LEDS  46  // original value (32) Two 16-LED NeoPixel rings
#define FPS       30  // original value (30) Animation frames/second (ish)

//Lightning effect parameters:
#define SECTION_LEN (4)     // number of LEDs to be grouped as one
#define NUM_SECTIONS (4)   // number of LED groups

const int sec_count = 9;  // the maximum "width" of each lightning sequence
const int seq_count = 9;  // the maximum "duration" of each lightning sequence
bool BLE_DataReceived = false;
//**********************************************************************************

SoftwareSerial    ser(RX_PIN, -1);
Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN);

void setup() {
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  // MUST do this on 16 MHz Trinket for serial & NeoPixels!
  clock_prescale_set(clock_div_1);
#endif
  // Stop incoming data & init software serial
  pinMode(CTS_PIN, OUTPUT); digitalWrite(CTS_PIN, HIGH);
  ser.begin(9600);

  pixels.begin(); // NeoPixel init
  // Flash space is tight on Trinket/Gemma, so setBrightness() is avoided --
  // it adds ~200 bytes.  Instead the color picker input is 'manually' scaled.
}

uint8_t  buf[3],              // Enough for RGB parse; expand if using sensors
         animMode = 0,        // Current animation mode
         animPos  = 0,        // Current animation position
         animFase = 0,
         animSubFase = 0;
uint8_t colorRed = 0x3f;
uint8_t colorGreen = 0;
uint8_t colorBlue = 0;
uint32_t color    = 0x400000, // Current animation color (red by default)
         prevTime = 0L;       // For animation timing

void loop(void) {
  int      c;
  uint32_t t;

  // Animation happens at about 30 frames/sec.  Rendering frames takes less
  // than that, so the idle time is used to monitor incoming serial data.
  digitalWrite(CTS_PIN, LOW); // Signal to BLE, OK to send data!
  for(;;) {
    t = micros();                            // Current time
    if((t - prevTime) >= (1000000L / FPS)) { // 1/30 sec elapsed?
      prevTime = t;
      break;                                 // Yes, go update LEDs
    }                                        // otherwise...
    if((c = ser.read()) == '!') {            // Received UART app input?
      while((c = ser.read()) < 0);           // Yes, wait for command byte
      switch(c) {
       case 'B':       // Button (Control Pad)
        if(readAndCheckCRC(255-'!'-'B', buf, 2) & (buf[1] == '1')) {
          buttonPress(buf[0]); // Handle button-press message
        }
        break;
       case 'C':       // Color Picker
        if(readAndCheckCRC(255-'!'-'C', buf, 3)) {
          // As mentioned earlier, setBrightness() was avoided to save space.
          // Instead, results from the color picker (in buf[]) are divided
          // by 4; essentially equivalent to setBrightness(64).  This is to
          // improve battery run time (NeoPixels are still plenty bright).
          colorRed = buf[0]/4;
          colorGreen = buf[1]/4;
          colorBlue = buf[2]/4;
          color = pixels.Color(buf[0]/4, buf[1]/4, buf[2]/4);
        }
        break;
       case 'Q':       // Quaternion
        skipBytes(17); // 4 floats + CRC (see note below re: parsing)
        break;
       case 'A':       // Accelerometer
#if 0
        // The phone sensors are NOT used by this sketch, but this shows how
        // they might be read.  First, buf[] must be delared large enough for
        // the expected data packet (minus header & CRC) -- that's 16 bytes
        // for quaternions (above), or 12 bytes for most of the others.
        // Second, the first arg to readAndCheckCRC() must be modified to
        // match the data type (e.g. 'A' here for accelerometer).  Finally,
        // values can be directly type-converted to float by using a suitable
        // offset into buf[] (e.g. 0, 4, 8, 12) ... it's not used in this
        // example because floating-point math uses lots of RAM and code
        // space, not suitable for the space-constrained Trinket/Gemma, but
        // maybe you're using a Pro Trinket, Teensy, etc.
        if(readAndCheckCRC(255-'!'-'A', buf, 12)) {
          float x = *(float *)(&buf[0]),
                y = *(float *)(&buf[4]),
                z = *(float *)(&buf[8]);
        }
        // In all likelihood, updates from the buttons and color picker
        // alone are infrequent enough that you could do without any mention
        // of the CTS pin in this code.  It's the extra sensors that really
        // start the firehose of data.
        break;
#endif
       case 'G':       // Gyroscope
       case 'M':       // Magnetometer
       case 'L':       // Location
        skipBytes(13); // 3 floats + CRC
      }
    }
  }
  digitalWrite(CTS_PIN, HIGH); // BLE STOP!

  // Show pixels calculated on *prior* pass; this ensures more uniform timing
  pixels.show();

  // Then calculate pixels for *next* frame...
  switch(animMode) {
    case 0: // Pinwheel mode
      for(uint8_t i=0; i<NUM_LEDS/2; i++) {
        uint32_t c = 0;
        if(((animPos + i) & 7) < 2) c = color; // 4 pixels on...
        pixels.setPixelColor(   i, c);         // First eye
        pixels.setPixelColor(NUM_LEDS-1-i, c); // Second eye (flipped)
      }
      animPos++;
      break;
    case 1: // Sparkle mode
      pixels.setPixelColor(animPos, 0);     // Erase old dot
      animPos = random(NUM_LEDS);           // Pick a new one
      pixels.setPixelColor(animPos, color); // and light it
      break;
    case 2: // Cylon Bounce
      CylonBounce(colorRed, colorGreen, colorBlue, 2, 30, 50);   // edit here for line size and speed
      break;
    case 3: // Lightning
      Lightning();
      break;
    case 4: // Twinkle
      Twinkle(colorRed, colorGreen, colorBlue, 10, 100, false);
      break;
    case 5: //
      RunningLights(colorRed, colorGreen, colorBlue, 50);
      break;
  }
}

boolean readAndCheckCRC(uint8_t sum, uint8_t *buf, uint8_t n) {
  for(int c;;) {
    while((c = ser.read()) < 0); // Wait for next byte
    if(!n--) return (c == sum);  // If CRC byte, we're done
    *buf++ = c;                  // Else store in buffer
    sum   -= c;                  // and accumulate sum
  }
}

void skipBytes(uint8_t n) {
  while(n--) {
    while(ser.read() < 0);
  }
}

void buttonPress(char c) {
  pixels.clear(); // Clear pixel data when switching modes (else residue)
  switch(c) {
   case '1':
    animMode = 0; // Switch to pinwheel mode
    animFase = 0;
    break;
   case '2':
    animMode = 1; // Switch to sparkle mode
    animFase = 0;
    break;
   case '3':
    animMode = 2;
    animFase = 0;
    break;
   case '4':
    animMode = 3;
    animFase = 0;
    break;
   case '5': // Up
    animMode = 4;
    animFase = 0;
    break;
   case '6': // Down
    animMode = 5;
    animFase = 0;
    break;
   case '7': // Left
    break;
   case '8': // Right
    break;
  }
}

void delayAndCheckBLE(uint32_t MicrosDelay){
  int      c;
  uint32_t tmpT;
  uint32_t tmpPrevT = micros();

  // Animation happens at about 30 frames/sec.  Rendering frames takes less
  // than that, so the idle time is used to monitor incoming serial data.
  digitalWrite(CTS_PIN, LOW); // Signal to BLE, OK to send data!
  for(;;) {
    tmpT = micros();                            // Current time
    if((tmpT - tmpPrevT) >= MicrosDelay) {  
      tmpPrevT = tmpT;
      break;                                 // Yes, go update LEDs
    }                                        // otherwise...
    if((c = ser.read()) == '!') {            // Received UART app input?
      while((c = ser.read()) < 0);           // Yes, wait for command byte
      switch(c) {
       case 'B':       // Button (Control Pad)
        if(readAndCheckCRC(255-'!'-'B', buf, 2) & (buf[1] == '1')) {
          buttonPress(buf[0]); // Handle button-press message
          BLE_DataReceived = true;
        }
        break;
       case 'C':       // Color Picker
        if(readAndCheckCRC(255-'!'-'C', buf, 3)) {
          // As mentioned earlier, setBrightness() was avoided to save space.
          // Instead, results from the color picker (in buf[]) are divided
          // by 4; essentially equivalent to setBrightness(64).  This is to
          // improve battery run time (NeoPixels are still plenty bright).
          colorRed = buf[0]/4;
          colorGreen = buf[1]/4;
          colorBlue = buf[2]/4;
          color = pixels.Color(buf[0]/4, buf[1]/4, buf[2]/4);
          BLE_DataReceived = true;
        }
        break;
       case 'Q':       // Quaternion
        skipBytes(17); // 4 floats + CRC (see note below re: parsing)
        BLE_DataReceived = true;
        break;
       case 'A':       // Accelerometer
#if 0
        // The phone sensors are NOT used by this sketch, but this shows how
        // they might be read.  First, buf[] must be delared large enough for
        // the expected data packet (minus header & CRC) -- that's 16 bytes
        // for quaternions (above), or 12 bytes for most of the others.
        // Second, the first arg to readAndCheckCRC() must be modified to
        // match the data type (e.g. 'A' here for accelerometer).  Finally,
        // values can be directly type-converted to float by using a suitable
        // offset into buf[] (e.g. 0, 4, 8, 12) ... it's not used in this
        // example because floating-point math uses lots of RAM and code
        // space, not suitable for the space-constrained Trinket/Gemma, but
        // maybe you're using a Pro Trinket, Teensy, etc.
        if(readAndCheckCRC(255-'!'-'A', buf, 12)) {
          float x = *(float *)(&buf[0]),
                y = *(float *)(&buf[4]),
                z = *(float *)(&buf[8]);
          BLE_DataReceived = true;      
        }
        // In all likelihood, updates from the buttons and color picker
        // alone are infrequent enough that you could do without any mention
        // of the CTS pin in this code.  It's the extra sensors that really
        // start the firehose of data.
        break;
#endif
       case 'G':       // Gyroscope
       case 'M':       // Magnetometer
       case 'L':       // Location
        skipBytes(13); // 3 floats + CRC
        BLE_DataReceived = true;
      }
    }
  }
  digitalWrite(CTS_PIN, HIGH); // BLE STOP!
}




//****************************************************************************************************
//****************************************************************************************************

void CylonBounce(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay){
  uint32_t CylonColor1 = pixels.Color(red/10, green/10, blue/10);
  uint32_t CylonColor2 = pixels.Color(red, green, blue);
  uint8_t numLedsForEye = NUM_LEDS/2;

  if(animFase == 0){
    animPos  = 0;
    animFase = 1;
  }

  if(animFase == 1){
    if(animPos < (numLedsForEye-EyeSize-2)){
      setAll(0,0,0);
      pixels.setPixelColor(animPos, CylonColor1);                     // First eye
      pixels.setPixelColor(NUM_LEDS-1-animPos, CylonColor1);          // Second eye (flipped)
    
      for(int j = 1; j <= EyeSize; j++) {
        pixels.setPixelColor(animPos+j, CylonColor2);                 // First eye
        pixels.setPixelColor(NUM_LEDS-1-animPos-j, CylonColor2);      // Second eye (flipped)
      }
      
      pixels.setPixelColor(animPos+EyeSize+1, CylonColor1);           // First eye
      pixels.setPixelColor(NUM_LEDS-animPos-EyeSize-2, CylonColor1);  // Second eye (flipped)
      animPos++;
      pixels.show();
    }else{
      animFase = 2;
    }
  }else if(animFase == 2){
    if(animPos > 0){
      setAll(0,0,0);
      pixels.setPixelColor(animPos, CylonColor1);               // First eye
      pixels.setPixelColor(NUM_LEDS-1-animPos, CylonColor1);    // Second eye (flipped)    
          
      for(int j = 1; j <= EyeSize; j++) {
        pixels.setPixelColor(animPos+j, CylonColor2);                 // First eye
        pixels.setPixelColor(NUM_LEDS-1-animPos-j, CylonColor2);      // Second eye (flipped)
      }
      
      pixels.setPixelColor(animPos+EyeSize+1, CylonColor1);           // First eye
      pixels.setPixelColor(NUM_LEDS-animPos-EyeSize-2, CylonColor1);  // Second eye (flipped)
      animPos--;
      pixels.show();
    }else{
      animPos = 0;
      animFase = 0;
    }
  }
}

//****************************************************************************************************
//****************************************************************************************************
  int r;
  int g;
  int b;
  int seq_order[seq_count];
  int seq_max;
  int sections;
  int seq_start;

void Lightning()
{
  if(animFase == 0){
    animPos  = 0;
    animFase = 1;
    // randomly select the color of the current lightning sequence
    r = random(200, 255);
    g = random(150, 255) - 100;
    b = random(200, 255);

    // randomly select the order in which the LED sections will flash
    // for the current lightning sequence
    seq_order[seq_count];
    seq_max = 0;
    for (int i = 0; i < seq_count; i++)
    {
      seq_order[i] = random(0, sec_count);
      seq_max = max(seq_max, seq_order[i]);
    }

    // randomly select the "duration" of the current lightning sequence
    sections = random(5, seq_count);
  
    // randomly select the starting location of the current lightning sequence
    seq_start = random(0, NUM_SECTIONS - seq_max);
  }
  
  
/*  
  
  
  // randomly select the color of the current lightning sequence
  int r = random(200, 255);
  int g = random(150, 255) - 100;
  int b = random(200, 255);

  // randomly select the order in which the LED sections will flash
  // for the current lightning sequence
  int seq_order[seq_count];
  int seq_max = 0;
  for (int i = 0; i < seq_count; i++)
  {
    seq_order[i] = random(0, sec_count);
    seq_max = max(seq_max, seq_order[i]);
  }
  
  // randomly select the "duration" of the current lightning sequence
  int sections = random(5, seq_count);
  
  // randomly select the starting location of the current lightning sequence
  int seq_start = random(0, NUM_SECTIONS - seq_max);
*/  
  if(animFase == 1){
    if(animPos < sections){
      // loop through each LED of the current section
      for (int k = 0; k < SECTION_LEN; k++)
      {
        // turn on the current LED
        int pix_cur = ((seq_start + seq_order[animPos]) * SECTION_LEN) + k;
        pixels.setPixelColor(pix_cur, pixels.Color(r, g, b));

        // turn off the LEDs of the previous section
        if (animPos > 0)
        {
          int pix_prv = ((seq_start + seq_order[animPos - 1]) * SECTION_LEN) + k;
          pixels.setPixelColor(pix_prv, pixels.Color(0, 0, 0));
        }
      }

      // very short (random) delay so we actually see the lightning
      //int delay_quick = random(15, 40);
      //delay(delay_quick);
      int delay_quick = random(0, 10);
      delay(delay_quick);
      pixels.show();
      animPos++;
    }else{
      setAll(0,0,0);
      animFase = 2;
      animPos = ((random(500, 1500))/30);
    }
  }else if(animFase == 2){
    if(animPos>0){
      animPos--;
    }else{
      animFase = 0;
      animPos = 0;
    }
  }
  
  
  
  
  
/*  
  
  // loop through each of the chosen sections
  for (int j = 0; j < sections; j++)
  {
    // loop through each LED of the current section
    for (int k = 0; k < SECTION_LEN; k++)
    {
      // turn on the current LED
      int pix_cur = ((seq_start + seq_order[j]) * SECTION_LEN) + k;
      pixels.setPixelColor(pix_cur, pixels.Color(r, g, b));

      // turn off the LEDs of the previous section
      if (j > 0)
      {
        int pix_prv = ((seq_start + seq_order[j - 1]) * SECTION_LEN) + k;
        pixels.setPixelColor(pix_prv, pixels.Color(0, 0, 0));
      }
    }

    // very short (random) delay so we actually see the lightning
    int delay_quick = random(15, 40);
    //delay(delay_quick);
    delayAndCheckBLE(delay_quick*1000);
/*    
    if(BLE_DataReceived){
      BLE_DataReceived = false;
      return;
    }
   
    pixels.show();
  }
 
  setAll(0,0,0);
  int delay_long = random(500, 1500);
  //delay(delay_long);
  delayAndCheckBLE(delay_long*1000);
*/
}

//****************************************************************************************************
//****************************************************************************************************

void Twinkle(byte red, byte green, byte blue, int Count, int SpeedDelay, boolean OnlyOne) {
  uint32_t TwinkleColor = pixels.Color(red, green, blue);
  
  if(animFase == 0){
    setAll(0,0,0);
    animPos  = 0;
    animSubFase = 0;
    animFase = 1;
  }

  if(animFase == 1){
    if( animPos < Count){
      if(animSubFase == 0){
        pixels.setPixelColor(random(NUM_LEDS), TwinkleColor);
        pixels.show();
        SpeedDelay /= 30;
        animSubFase = SpeedDelay + 2;
      }

      if(animSubFase == 1){
        if(OnlyOne) { 
          setAll(0,0,0); 
        }
        animPos++;
        animSubFase = 0;
      }else{
        animSubFase--;
      }
    }else{
      animFase = 2;
      animSubFase = 0;
      animPos = SpeedDelay;
    }
  }else if(animFase == 2){
    if( animPos>1 ){
      animPos--;
    }else{
      animFase = 0;
      animSubFase = 0;
      animPos = 0;
    }
  }
}

/*
void Twinkle(byte red, byte green, byte blue, int Count, int SpeedDelay, boolean OnlyOne) {
  uint32_t TwinkleColor = pixels.Color(red, green, blue);
  
  setAll(0,0,0);
  for (int i=0; i<Count; i++) {
    pixels.setPixelColor(random(NUM_LEDS), TwinkleColor);
    pixels.show();
    //delay(SpeedDelay);
    delayAndCheckBLE(SpeedDelay*1000);

    if(OnlyOne) { 
      setAll(0,0,0); 
    }
  }
  //delay(SpeedDelay);
  delayAndCheckBLE(SpeedDelay*1000);

}
*/
//****************************************************************************************************
//****************************************************************************************************
int runningPos=0;

void RunningLights(byte red, byte green, byte blue, int WaveDelay) {
  uint8_t numLedsForEye = NUM_LEDS/2;
  uint32_t RunningLightsColor;
  
  if(animFase == 0){
    runningPos=0;
    animPos  = 0;
    animFase = 1;
    animSubFase = 0;
  }
  
  if(animFase == 1){
    if(animSubFase == 0){
      if(animPos < NUM_LEDS){
        runningPos++;
        for(int i=0; i<numLedsForEye; i++){
          RunningLightsColor = pixels.Color( ((sin(i+runningPos) * 127 + 128)/255)*red,
                                             ((sin(i+runningPos) * 127 + 128)/255)*green,
                                             ((sin(i+runningPos) * 127 + 128)/255)*blue);
                      
          pixels.setPixelColor(i, RunningLightsColor);                     // First eye
          pixels.setPixelColor(NUM_LEDS-1-i, RunningLightsColor);          // Second eye (flipped)
        }
        pixels.show();
        animSubFase = 1 + WaveDelay/30;
      }else{
        animFase = 2;
        animSubFase = 1 + WaveDelay/30;
      }
    }
    if(animSubFase == 1){
      animPos++;
      animSubFase = 0;
    }else{
      animSubFase--;
    }
  }else if(animFase == 2){
    if(animSubFase == 1){
      animFase = 0;
      animSubFase = 0;
      animPos = 0;
    }else{
      animSubFase--;
    }
  }
}

//****************************************************************************************************
//****************************************************************************************************

/*
void CylonBounce(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay){
  uint32_t CylonColor1 = pixels.Color(red/10, green/10, blue/10);
  uint32_t CylonColor2 = pixels.Color(red, green, blue);

  for(int i = 0; i < NUM_LEDS-EyeSize-2; i++) {
    setAll(0,0,0);
    //setPixel(i, red/10, green/10, blue/10);
    pixels.setPixelColor(i, CylonColor1);
    
    for(int j = 1; j <= EyeSize; j++) {
      //setPixel(i+j, red, green, blue);
      pixels.setPixelColor(i+j, CylonColor2);
    }
    //setPixel(i+EyeSize+1, red/10, green/10, blue/10);
    pixels.setPixelColor(i+EyeSize+1, CylonColor1);
    pixels.show();
    delay(SpeedDelay);
  }

  delay(ReturnDelay);

  for(int i = NUM_LEDS-EyeSize-2; i > 0; i--) {
    setAll(0,0,0);
    //setPixel(i, red/10, green/10, blue/10);
    pixels.setPixelColor(i, CylonColor1);
        
    for(int j = 1; j <= EyeSize; j++) {
      //setPixel(i+j, red, green, blue);
      pixels.setPixelColor(i+j, CylonColor2);
    }
    //setPixel(i+EyeSize+1, red/10, green/10, blue/10);
    pixels.setPixelColor(i+EyeSize+1, CylonColor1);
        
    pixels.show();
    delay(SpeedDelay);
  }
  
  delay(ReturnDelay);
}
*/
void setAll(byte red, byte green, byte blue) {
  uint32_t AllColor = pixels.Color(red, green, blue);  
  
  for(int i = 0; i < NUM_LEDS; i++ ) {
    //setPixel(i, red, green, blue); 
    pixels.setPixelColor(i, AllColor);
  }
  pixels.show();
}







/*
void Fire(int Cooling, int Sparking, int SpeedDelay) {
  static byte heat[NUM_LEDS];
  int cooldown;
 
  // Step 1.  Cool down every cell a little
  for( int i = 0; i < NUM_LEDS; i++) {
    cooldown = random(0, ((Cooling * 10) / NUM_LEDS) + 2);
   
    if(cooldown>heat[i]) {
      heat[i]=0;
    } else {
      heat[i]=heat[i]-cooldown;
    }
  }
 
  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for( int k= NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }
   
  // Step 3.  Randomly ignite new 'sparks' near the bottom
  if( random(255) < Sparking ) {
    int y = random(7);
    heat[y] = heat[y] + random(160,255);
    //heat[y] = random(160,255);
  }

  // Step 4.  Convert heat to LED colors
  for( int j = 0; j < NUM_LEDS; j++) {
    setPixelHeatColor(j, heat[j] );
  }

  pixels.show();
  delay(SpeedDelay);
}

void setPixelHeatColor (int Pixel, byte temperature) {
  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = round((temperature/255.0)*191);
 
  // calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2; // scale up to 0..252
 
  // figure out which third of the spectrum we're in:
  if( t192 > 0x80) {                     // hottest
    pixels.setPixelColor(Pixel, pixels.Color(255, 255, heatramp));
  } else if( t192 > 0x40 ) {             // middle
    pixels.setPixelColor(Pixel, pixels.Color(255, heatramp, 0));
  } else {                               // coolest
    pixels.setPixelColor(Pixel, pixels.Color(heatramp, 0, 0));
  }
}

********************************************************************

void CylonBounce(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay){

  for(int i = 0; i < NUM_LEDS-EyeSize-2; i++) {
    setAll(0,0,0);
    setPixel(i, red/10, green/10, blue/10);
    for(int j = 1; j <= EyeSize; j++) {
      setPixel(i+j, red, green, blue);
    }
    setPixel(i+EyeSize+1, red/10, green/10, blue/10);
    showStrip();
    delay(SpeedDelay);
  }

  delay(ReturnDelay);

  for(int i = NUM_LEDS-EyeSize-2; i > 0; i--) {
    setAll(0,0,0);
    setPixel(i, red/10, green/10, blue/10);
    for(int j = 1; j <= EyeSize; j++) {
      setPixel(i+j, red, green, blue);
    }
    setPixel(i+EyeSize+1, red/10, green/10, blue/10);
    showStrip();
    delay(SpeedDelay);
  }
 
  delay(ReturnDelay);
}

********************************************************************************

void RunningLights(byte red, byte green, byte blue, int WaveDelay) {
  int Position=0;
 
  for(int j=0; j<NUM_LEDS*2; j++)
  {
      Position++; // = 0; //Position + Rate;
      for(int i=0; i<NUM_LEDS; i++) {
        // sine wave, 3 offset waves make a rainbow!
        //float level = sin(i+Position) * 127 + 128;
        //setPixel(i,level,0,0);
        //float level = sin(i+Position) * 127 + 128;
        setPixel(i,((sin(i+Position) * 127 + 128)/255)*red,
                   ((sin(i+Position) * 127 + 128)/255)*green,
                   ((sin(i+Position) * 127 + 128)/255)*blue);
      }
     
      showStrip();
      delay(WaveDelay);
  }
}

********************************************************************


void BouncingBalls(byte red, byte green, byte blue, int BallCount) {
  float Gravity = -9.81;
  int StartHeight = 1;
 
  float Height[BallCount];
  float ImpactVelocityStart = sqrt( -2 * Gravity * StartHeight );
  float ImpactVelocity[BallCount];
  float TimeSinceLastBounce[BallCount];
  int   Position[BallCount];
  long  ClockTimeSinceLastBounce[BallCount];
  float Dampening[BallCount];
 
  for (int i = 0 ; i < BallCount ; i++) {  
    ClockTimeSinceLastBounce[i] = millis();
    Height[i] = StartHeight;
    Position[i] = 0;
    ImpactVelocity[i] = ImpactVelocityStart;
    TimeSinceLastBounce[i] = 0;
    Dampening[i] = 0.90 - float(i)/pow(BallCount,2);
  }

  while (true) {
    for (int i = 0 ; i < BallCount ; i++) {
      TimeSinceLastBounce[i] =  millis() - ClockTimeSinceLastBounce[i];
      Height[i] = 0.5 * Gravity * pow( TimeSinceLastBounce[i]/1000 , 2.0 ) + ImpactVelocity[i] * TimeSinceLastBounce[i]/1000;
 
      if ( Height[i] < 0 ) {                      
        Height[i] = 0;
        ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
        ClockTimeSinceLastBounce[i] = millis();
 
        if ( ImpactVelocity[i] < 0.01 ) {
          ImpactVelocity[i] = ImpactVelocityStart;
        }
      }
      Position[i] = round( Height[i] * (NUM_LEDS - 1) / StartHeight);
    }
 
    for (int i = 0 ; i < BallCount ; i++) {
      setPixel(Position[i],red,green,blue);
    }
   
    showStrip();
    setAll(0,0,0);
  }
}





*/
