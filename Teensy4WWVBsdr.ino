
/***********************************************************************

   Thanks to the following which was the inspiration and original source
   for my efforts -- Chris Howard  email w0ep@w0ep.us  February 2020

   *********************************************************************

   Copyright (c) 2016, Frank Bösing, f.boesing@gmx.de & Frank DD4WH, dd4wh.swl@gmail.com

    Teensy DCF77 Receiver & Real Time Clock

    uses only minimal hardware to receive accurate time signals

    For information on how to setup the antenna, see here:

    https://github.com/DD4WH/Teensy-DCF77/wiki
*/
#define VERSION     " v0.42w"
/*
   Permission is hereby granted, free of charge, to any perso obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice, development funding notice, and this permission
   notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

*/

#include <Time.h>
#include <TimeLib.h>

#include <Audio.h>
#include <SPI.h>
#include <Metro.h>
#include <ILI9341_t3.h>
#include "font_Arial.h"
#include <utility/imxrt_hw.h>

time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}

#define BACKLIGHT_PIN 22
#define TFT_DC      5
#define TFT_CS      14
#define TFT_RST     255  // 255 = unused. connect to 3.3V
#define TFT_MOSI     11
#define TFT_SCLK    13
#define TFT_MISO    12

#define T4

ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);

#define SAMPLE_RATE_MIN               0
#define SAMPLE_RATE_8K                0
#define SAMPLE_RATE_11K               1
#define SAMPLE_RATE_16K               2
#define SAMPLE_RATE_22K               3
#define SAMPLE_RATE_32K               4
#define SAMPLE_RATE_44K               5
#define SAMPLE_RATE_48K               6
#define SAMPLE_RATE_88K               7
#define SAMPLE_RATE_96K               8
#define SAMPLE_RATE_176K              9
#define SAMPLE_RATE_192K              10
#define SAMPLE_RATE_MAX               10

int debug = 1;

AudioInputI2S            i2s_in;         //xy=202,411
AudioSynthWaveformSine   sine1;          //xy=354,249
AudioFilterBiquad        biquad1;        //xy=394,403
AudioEffectMultiply      mult1;          //xy=594,250
AudioFilterBiquad        biquad2;        //xy=761,248
//AudioAnalyzeFFT256       myFFT;          //xy=962,434
AudioAnalyzeFFT1024       myFFT;          //xy=962,434
AudioOutputI2S           i2s_out;        //xy=975,247
AudioConnection          patchCord1(i2s_in, 0, biquad1, 0);
AudioConnection          patchCord2(sine1, 0, mult1, 1);
AudioConnection          patchCord3(biquad1, 0, mult1, 0);
AudioConnection          patchCord4(biquad1, myFFT);
AudioConnection          patchCord5(mult1, biquad2);
AudioConnection          patchCord6(biquad2, 0, i2s_out, 1);
AudioConnection          patchCord7(biquad2, 0, i2s_out, 0);
AudioControlSGTL5000     sgtl5000_1;

// Metro 1 second
Metro second_timer = Metro(1000);

const uint16_t FFT_points = 1024;
//const uint16_t FFT_points = 256;

int8_t mic_gain = 38 ;//start detecting with this MIC_GAIN in dB
const float bandpass_q = 10; // be careful when increasing Q, distortion can occur with higher Q because of fixed point 16bit math in the biquads
// const float DCF77_FREQ = 77500.0; //DCF-77 77.65 kHz
const float WWVB_FREQ = 60000.0;
// start detecting at this frequency, so that
// you can hear a 600Hz tone [77.5 - 76.9 = 0.6kHz]
unsigned int freq_real = WWVB_FREQ - 600;

// const unsigned int sample_rate = SAMPLE_RATE_176K;
// unsigned int sample_rate_real = 176400;
const unsigned int sample_rate = SAMPLE_RATE_192K;
unsigned int sample_rate_real = 192000;


unsigned int freq_LO = 7000;
float wwvb_signal = 0;
float wwvb_threshold = 0;
float wwvb_med = 0;
unsigned int WWVB_bin;// this is the FFT bin where the 60kHz signal is

bool timeflag = 0;
const int8_t pos_x_date = 14;
const int8_t pos_y_date = 68;
const int8_t pos_x_time = 14;
const int8_t pos_y_time = 114;
uint8_t hour10_old;
uint8_t hour1_old;
uint8_t minute10_old;
uint8_t minute1_old;
uint8_t second10_old;
uint8_t second1_old;
uint8_t precision_flag = 0;

const float displayscale = 2.5;

typedef struct SR_Descriptor
{
  const int SR_n;
  const char* const f1;
  const char* const f2;
  const char* const f3;
  const char* const f4;
  const float32_t x_factor;
} SR_Desc;

// Text and position for the FFT spectrum display scale
const SR_Descriptor SR[SAMPLE_RATE_MAX + 1] =
{
  //   SR_n ,  f1, f2, f3, f4, x_factor = pixels per f1 kHz in spectrum display
  {  SAMPLE_RATE_8K,  " 1", " 2", " 3", " 4", 64.0}, // which means 64 pixels per 1 kHz
  {  SAMPLE_RATE_11K,  " 1", " 2", " 3", " 4", 43.1},
  {  SAMPLE_RATE_16K,  " 2", " 4", " 6", " 8", 64.0},
  {  SAMPLE_RATE_22K,  " 2", " 4", " 6", " 8", 43.1},
  {  SAMPLE_RATE_32K,  "5", "10", "15", "20", 80.0},
  {  SAMPLE_RATE_44K,  "5", "10", "15", "20", 58.05},
  {  SAMPLE_RATE_48K,  "5", "10", "15", "20", 53.33},
  {  SAMPLE_RATE_88K,  "10", "20", "30", "40", 58.05},
  {  SAMPLE_RATE_96K,  "10", "20", "30", "40", 53.33},
  {  SAMPLE_RATE_176K,  "20", "40", "60", "80", 58.05},
  {  SAMPLE_RATE_192K,  "20", "40", "60", "80", 53.33} // which means 53.33 pixels per 20kHz
};

IntervalTimer nubbinTimer;

//const int myInput = AUDIO_INPUT_LINEIN;
const int myInput = AUDIO_INPUT_MIC;

void setup();
void loop();

//=========================================================================

void setup() {

  Serial.begin(115200);
  //Serial.begin(9600);

  setSyncProvider(getTeensy3Time);

  // Audio connections require memory.
  AudioMemory(16);

  // Enable the audio shield. select input. and enable output
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(myInput);
  sgtl5000_1.volume(0.9);
  sgtl5000_1.micGain (mic_gain);
  sgtl5000_1.adcHighPassFilterDisable(); // does not help too much!

  // Init TFT display
  pinMode( BACKLIGHT_PIN, OUTPUT );
  analogWrite( BACKLIGHT_PIN, 1023 );
  tft.begin();
  tft.setRotation( 3 );
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(14, 7);
  tft.setTextColor(ILI9341_ORANGE);
  tft.setFont(Arial_12);
  tft.print("Teensy UTC SDR Clock "); tft.print(VERSION);
  tft.setTextColor(ILI9341_WHITE);

  displaySettings();

  set_sample_rate (sample_rate);
  set_freq_LO (freq_real);

  initializePrecisionHistory();
  displayDate();
  displayClock();
  displayPrecisionMessage();
  // displayEtc();

  initializePatternsAndMeans();

  // start our interrupt timer - wake up every 0.010 seconds
  nubbinTimer.begin(nubbinBorn, 10000);

  //Serial.println("end of setup");
} // END SETUP


bool doOtherStuff = true;
void loop() {

  // Serial.println("in loop");

  if (myFFT.available())
  {
    agc();
    detectSymbol();
    spectrum();

    // the doOtherStuff flag gets set in detectSymbol
    if ( doOtherStuff )
    {
      displayClock();
      doOtherStuff = false;
    }
  }
  //  check_processor();
}

void set_mic_gain(int8_t gain) {
  // AudioNoInterrupts();
  sgtl5000_1.micGain (mic_gain);
  // AudioInterrupts();
  // displaySettings();
} // end function set_mic_gain

void       set_freq_LO(int freq) {
  // audio lib thinks we are still in 44118sps sample rate
  // therefore we have to scale the frequency of the local oscillator
  // in accordance with the REAL sample rate
  freq_LO = freq * (AUDIO_SAMPLE_RATE_EXACT / sample_rate_real);
  // if we switch to LOWER samples rates, make sure the running LO
  // frequency is allowed ( < 22k) ! If not, adjust consequently, so that
  // LO freq never goes up 22k, also adjust the variable freq_real
  if (freq_LO > 22000) {
    freq_LO = 22000;
    freq_real = freq_LO * (sample_rate_real / AUDIO_SAMPLE_RATE_EXACT) + 9;
  }
  if ( debug )
  {
    Serial.print("set_freq_LO ");
    Serial.print(freq);
    Serial.print(" freq_LO ");
    Serial.println(freq_LO);
  }
  AudioNoInterrupts();
  sine1.frequency(freq_LO);
  AudioInterrupts();
  // displaySettings();
} // END of function set_freq_LO

void      displaySettings() {
  // This code uses the same screen real estate as the precision message
  tft.fillRect(14, 32, 200, 17, ILI9341_BLACK);
  tft.setCursor(14, 32);
  tft.setFont(Arial_12);
  tft.print("gain: "); tft.print (mic_gain);
  tft.print("     ");
  tft.print("freq: "); tft.print (freq_real);
  tft.print("    ");
  tft.fillRect(232, 32, 88, 17, ILI9341_BLACK);
  tft.setCursor(232, 32);
  tft.print("       ");
  tft.print(sample_rate_real / 1000); tft.print("k");
}

void      set_sample_rate (int sr) {
  switch (sr) {
    case SAMPLE_RATE_8K:
      sample_rate_real = 8000;
      break;
    case SAMPLE_RATE_11K:
      sample_rate_real = 11025;
      break;
    case SAMPLE_RATE_16K:
      sample_rate_real = 16000;
      break;
    case SAMPLE_RATE_22K:
      sample_rate_real = 22050;
      break;
    case SAMPLE_RATE_32K:
      sample_rate_real = 32000;
      break;
    case SAMPLE_RATE_44K:
      sample_rate_real = 44100;
      break;
    case SAMPLE_RATE_48K:
      sample_rate_real = 48000;
      break;
    case SAMPLE_RATE_88K:
      sample_rate_real = 88200;
      break;
    case SAMPLE_RATE_96K:
      sample_rate_real = 96000;
      break;
    case SAMPLE_RATE_176K:
      sample_rate_real = 176400;
      break;
    case SAMPLE_RATE_192K:
      sample_rate_real = 192000;
      break;
  }
  AudioNoInterrupts();
  setI2SFreq(sample_rate_real);

  delay(200); // this delay seems to be very essential !
  set_freq_LO (freq_real);

  // never set the lowpass freq below 1700 (empirically derived by ear ;-))
  // distortion will occur because of precision issues due to fixed point 16bit in the biquads
  biquad2.setLowpass(0, 1700, 0.54);
  biquad2.setLowpass(1, 1700, 1.3);
  biquad2.setLowpass(2, 1700, 0.54);
  biquad2.setLowpass(3, 1700, 1.3);

  biquad1.setBandpass(0, WWVB_FREQ * (AUDIO_SAMPLE_RATE_EXACT / sample_rate_real), bandpass_q);
  biquad1.setBandpass(1, WWVB_FREQ * (AUDIO_SAMPLE_RATE_EXACT / sample_rate_real), bandpass_q);
  biquad1.setBandpass(2, WWVB_FREQ * (AUDIO_SAMPLE_RATE_EXACT / sample_rate_real), bandpass_q);
  biquad1.setBandpass(3, WWVB_FREQ * (AUDIO_SAMPLE_RATE_EXACT / sample_rate_real), bandpass_q);

  AudioInterrupts();
  delay(20);
  WWVB_bin = round((WWVB_FREQ / (sample_rate_real / 2.0)) * (FFT_points / 2));
  if ( debug )
  {
    Serial.print("WWVB_bin number: "); Serial.println(WWVB_bin);
  }

  // displaySettings();
  prepare_spectrum_display();

} // END function set_sample_rate

void prepare_spectrum_display() {
  int base_y = 211;
  int b_x = 10;
  int x_f = SR[sample_rate].x_factor;
  tft.fillRect(0, base_y, 320, 240 - base_y, ILI9341_BLACK);
  //    tft.drawFastHLine(b_x, base_y + 2, 256, ILI9341_PURPLE);
  //    tft.drawFastHLine(b_x, base_y + 3, 256, ILI9341_PURPLE);
  tft.drawFastHLine(b_x, base_y + 2, 256, ILI9341_MAROON);
  tft.drawFastHLine(b_x, base_y + 3, 256, ILI9341_MAROON);
  // vertical lines
  tft.drawFastVLine(b_x - 4, base_y + 1, 10, ILI9341_YELLOW);
  tft.drawFastVLine(b_x - 3, base_y + 1, 10, ILI9341_YELLOW);
  tft.drawFastVLine( x_f + b_x,  base_y + 1, 10, ILI9341_YELLOW);
  tft.drawFastVLine( x_f + 1 + b_x,  base_y + 1, 10, ILI9341_YELLOW);
  tft.drawFastVLine( x_f * 2 + b_x,  base_y + 1, 10, ILI9341_YELLOW);
  tft.drawFastVLine( x_f * 2 + 1 + b_x,  base_y + 1, 10, ILI9341_YELLOW);
  if (x_f * 3 + b_x < 256 + b_x) {
    tft.drawFastVLine( x_f * 3 + b_x,  base_y + 1, 10, ILI9341_YELLOW);
    tft.drawFastVLine( x_f * 3 + 1 + b_x,  base_y + 1, 10, ILI9341_YELLOW);
  }
  if (x_f * 4 + b_x < 256 + b_x) {
    tft.drawFastVLine( x_f * 4 + b_x,  base_y + 1, 10, ILI9341_YELLOW);
    tft.drawFastVLine( x_f * 4 + 1 + b_x,  base_y + 1, 10, ILI9341_YELLOW);
  }
  tft.drawFastVLine( x_f * 0.5 + b_x,  base_y + 1, 6, ILI9341_YELLOW);
  tft.drawFastVLine( x_f * 1.5 + b_x,  base_y + 1, 6, ILI9341_YELLOW);
  tft.drawFastVLine( x_f * 2.5 + b_x,  base_y + 1, 6, ILI9341_YELLOW);
  if (x_f * 3.5 + b_x < 256 + b_x) {
    tft.drawFastVLine( x_f * 3.5 + b_x,  base_y + 1, 6, ILI9341_YELLOW);
  }
  if (x_f * 4.5 + b_x < 256 + b_x) {
    tft.drawFastVLine( x_f * 4.5 + b_x,  base_y + 1, 6, ILI9341_YELLOW);
  }
  // text
  tft.setTextColor(ILI9341_WHITE);
  tft.setFont(Arial_9);
  int text_y_offset = 16;
  int text_x_offset = - 5;
  // zero
  tft.setCursor (b_x + text_x_offset, base_y + text_y_offset);
  tft.print(0);
  tft.setCursor (b_x + x_f + text_x_offset, base_y + text_y_offset);
  tft.print(SR[sample_rate].f1);
  tft.setCursor (b_x + x_f * 2 + text_x_offset, base_y + text_y_offset);
  tft.print(SR[sample_rate].f2);
  tft.setCursor (b_x + x_f * 3 + text_x_offset, base_y + text_y_offset);
  tft.print(SR[sample_rate].f3);
  tft.setCursor (b_x + x_f * 4 + text_x_offset, base_y + text_y_offset);
  tft.print(SR[sample_rate].f4);
  //    tft.setCursor (b_x + text_x_offset + 256, base_y + text_y_offset);
  tft.print(" kHz");

  tft.setFont(Arial_14);
} // END prepare_spectrum_display


void agc() {
  static unsigned long tspeed = millis(); //Timer for startup

  const float speed_agc_start =  0.995;   //initial speed AGC
  const float speed_agc_run   =  0.9995;
  static float speed_agc = speed_agc_start;
  static unsigned long tagc = millis(); //Timer for AGC

  const float speed_thr = 0.995;

  //  tft.drawFastHLine(14, 220 - wwvb_med, 256, ILI9341_BLACK);
  tft.drawFastHLine(250, 220 - wwvb_med, 46, ILI9341_BLACK);
  wwvb_signal = (abs(myFFT.output[WWVB_bin]) + abs(myFFT.output[WWVB_bin + 1])) * displayscale;
  if (wwvb_signal > 175) wwvb_signal  = 175;
  else if (wwvb_med == 0) wwvb_med = wwvb_signal;
  wwvb_med = (1 - speed_agc) * wwvb_signal + speed_agc * wwvb_med;
  tft.drawFastHLine(250, 220 - wwvb_med, 46, ILI9341_ORANGE);

  tft.drawFastHLine(250, 220 - wwvb_threshold, 46, ILI9341_BLACK);
  wwvb_threshold = (1 - speed_thr) * wwvb_signal + speed_thr * wwvb_threshold;
  tft.drawFastHLine(250, 220 - wwvb_threshold, 46, ILI9341_GREEN);

  unsigned long t = millis();
  //Slow down speed after a while
  if ((t - tspeed > 1500) && (t - tspeed < 3500) ) {
    if (speed_agc < speed_agc_run) {
      speed_agc = speed_agc_run;
      if ( debug )
      {
        Serial.printf("Set AGC-Speed %f\n", speed_agc);
      }
    }
  }

  if ((t - tagc > 2221) || (speed_agc == speed_agc_start)) {
    tagc = t;
    if ((wwvb_med > 160) && (mic_gain > 30)) {
      //if ((wwvb_med > 160) && (mic_gain > 30)) {
      mic_gain--;
      set_mic_gain(mic_gain);
      //Serial.printf("(Gain-: %d)", mic_gain);
    }
    if ((wwvb_med < 100) && (mic_gain < 58)) {
      mic_gain++;
      set_mic_gain(mic_gain);
      //Serial.printf("(Gain+: %d)", mic_gain);
    }
  }
}

// Precision History
// Until first successful decode, show the red text.
// After first decode, show a string of colored  characters
//  that indicate the most recent success/fail.
//
// array is circular buffer

#define PH_SIZE 60
#define PH_DISPLAY 45
#define PH_PERIOD __seconds_in_an_hour  // seconds in each ph period
#define PH_NONE 0
#define PH_DECODE 1
#define PH_SET 2

int precisionHistory[PH_SIZE]; // 0 = fail; 1 = success
unsigned int phIndex = PH_DISPLAY;


void initializePrecisionHistory()
{
  // initialize all to fail state
  for (int i = 0; i < PH_SIZE; i++ )
  {
    precisionHistory[i] = PH_NONE;
  }
}

void setPrecisionForPeriod (int newState, boolean inc)
{
  // set the Precision level for this period.
  // if inc, increment the period

  if ( precisionHistory[phIndex % PH_SIZE] < newState )
  {
    precisionHistory[phIndex % PH_SIZE] = newState;
  }
  if ( inc )
  {
    phIndex++;
  }

}
void displayPrecisionMessage()
{
  unsigned int i, j;
  // This code uses the same display real estate as displaySettings()
  if (precision_flag)
  {
    tft.fillRect(14, 32, 300, 18, ILI9341_BLACK);
    tft.setCursor(14, 32);
    tft.setFont(Arial_11);

    for ( i = 0; i < PH_DISPLAY; i++ )
    {
      j = (i + phIndex - PH_DISPLAY + 1) % PH_SIZE;
      if ( precisionHistory[j] == PH_SET )
      {
        tft.setTextColor(ILI9341_GREEN);
        tft.print("*");
      }
      else if ( precisionHistory[j] == PH_DECODE )
      {
        tft.setTextColor(ILI9341_YELLOW);
        tft.print("*");
      }
      else
      {
        tft.setTextColor(ILI9341_RED);
        tft.print("-");
      }
    }
    // tft.setTextColor(ILI9341_GREEN);
    // tft.print("Full precision of time and date");
    tft.drawRect(290, 4, 20, 20, ILI9341_GREEN);
  }
  else
  {
    tft.fillRect(14, 32, 300, 18, ILI9341_BLACK);
    tft.setCursor(14, 32);
    tft.setFont(Arial_11);
    tft.setTextColor(ILI9341_RED);
    tft.print("Unprecise, trying to collect data");
    tft.drawRect(290, 4, 20, 20, ILI9341_RED);
  }
} // end function displayPrecisionMessage

int symbolArray[60];
int symbolCount = 0;

const int __seconds_in_a_minute = 60;
const int __seconds_in_an_hour = 3600;
const int __seconds_in_a_day = 86400;

// for every year from 2020 to 2049
//   this is the time_t value at the first of that year
// Get two digit year, subtract 20 and index into this array
//   to determine how many seconds to add to our time_t
const unsigned int __seconds_years[30] =
{
  1577836800, 1609459200, 1640995200, 1672531200, 1704067200,
  1735689600, 1735689600, 1798761600, 1830297600, 1861920000,
  1893456000, 1924992000, 1956528000, 1988150400, 1988150400,
  2051222400, 2082758400, 2114380800, 2145916800, 2177452800,
  2208988800, 2208988800, 2272147200, 2303683200, 2335219200,
  2366841600, 2398377600, 2429913600, 2461449600, 2493072000
};


// One long state machine based on the symbolCount.
// Given the symbol we have received
//  and the current symbolCount, are we still on track or
//  are we busted.  Machine starts over when busted.
//  The start condition is looking for 'M' symbol.
//
//  If we completely decode two successive minutes, set the clock.
//

int leapYear = 0;
int leapSecondWarning = 0;
int statusDST = 0;
int offsetDUT1 = 0;
char signDUT1 = ' ';

// Save the most recent decode;
static time_t recent_decode = now();

void decode (int symbol)
{
  static int badData = 0;
  static time_t newTime = 0;
  static int phCount = 0;
 
  if ( debug > 2)
  {
    Serial.print(" ");
    Serial.print(symbolCount);
    Serial.print(" ");
  }

  if ( symbol == 2) // M
  {
    if ( symbolCount == 0 || symbolCount == 1)
    {
      symbolArray[symbolCount] = symbol;
      symbolCount = 1;
      badData = 0;
      newTime = 0;
    }
    else if ( symbolCount == 9 )
    {

      int myMinute = symbolArray[1] * 40 + symbolArray[2] * 20 + symbolArray[3] * 10
                     + symbolArray[5] * 8 + symbolArray[6] * 4 + symbolArray[7] * 2 + symbolArray[8] * 1;
      symbolArray[symbolCount++] = symbol;
      if ( myMinute > 59 )
      {
        if ( debug )
        {
          Serial.println();
          Serial.print("Minute out of range (0-59): ");
          Serial.println(myMinute);
        }
        badData++;
      }
      else
      {
        if ( debug )
        {
          Serial.println();
          Serial.print("Minute: ");
          Serial.println(myMinute);
        }
        // Embrace the time_t way:  just
        //   count up all of the seconds and let TimeLib do the
        //   heavy lifting turning that into minutes, hours, days, etc.
        newTime += myMinute * __seconds_in_a_minute;
      }
    }
    else if ( symbolCount == 19 )
    {
      int myHour = symbolArray[12] * 20 + symbolArray[13] * 10
                   + symbolArray[15] * 8 + symbolArray[16] * 4 + symbolArray[17] * 2 + symbolArray[18] * 1;
      symbolArray[symbolCount++] = symbol;
      if ( myHour > 23 )
      {
        if ( debug )
        {
          Serial.println();
          Serial.print("Hour out of range (0-23): ");
          Serial.println(myHour);
        }
        badData++;
      }
      else
      {
        if ( debug )
        {
          Serial.println();
          Serial.print("Hour: ");
          Serial.println(myHour);
        }
        newTime += myHour * __seconds_in_an_hour;
      }

    }
    else if ( symbolCount == 29 ||  symbolCount == 39 || symbolCount == 49 )
    {
      symbolArray[symbolCount] = 2;
      symbolCount++;
    }
    else if ( symbolCount == 59 )
    {
      symbolArray[symbolCount] = 2;
      symbolCount = 0;
      if ( debug )
      {
        Serial.print("****finished!!**");
        Serial.print(badData);
        Serial.println("**");
      }
      Serial.println();

      leapYear = symbolArray[55];
      // skipping the baddata cross-check on leap year

      leapSecondWarning = symbolArray[56];
      statusDST = symbolArray[57] * 2 + symbolArray[58];

      if ( badData == 0 )
      {
        // Success!  All data seems to be ok.

        // wait for two decodes in a row before we
        // actually call it good.
        if ( newTime == recent_decode )
        {
          Serial.println("two in a row, setting time");
          // This adjustment is because they send over the air the "current" time
          //   when the transmitted minute started.
          // By the time we receive and decode we are exactly 1 minute behind.
          setTime(newTime + __seconds_in_a_minute);

          // set the hardware real-time-clock
          Teensy3Clock.set(now());

          if ( precision_flag < 1 )
          {
            precision_flag = 1;
          }

          //  Precision history to indicate which minutes had good decodes
          setPrecisionForPeriod(PH_SET, false);
          displayDate();
          // displayEtc();
        }
        else
        {
          setPrecisionForPeriod(PH_DECODE, false);
        }
        recent_decode = newTime + __seconds_in_a_minute;
        Serial.print("recent decode is ");
        Serial.println(recent_decode);
      }
      else
      {
        badData = 0;
        setPrecisionForPeriod(PH_NONE, false);
      }
      // phCount = 0;
      displayPrecisionMessage();

    }
    else
    {
      //Busted!
      // Serial.println();
      if ( debug )
      {
        Serial.println("*M*Busted*M*");
      }
      // precision_flag = 0;
      symbolCount = 0;
    }
  }  // end of symbol == 2  "M"
  else if ( symbol == 1 )
  {
    if ( symbolCount == 0 )
    {
      symbolCount = 0;
    }
    else if ( symbolCount == 4  || symbolCount == 9  ||
              symbolCount == 10 || symbolCount == 11 || symbolCount == 14 ||
              symbolCount == 19 || symbolCount == 20 || symbolCount == 21 ||  symbolCount == 29 ||
              symbolCount == 34 || symbolCount == 35 || symbolCount == 39 ||
              symbolCount == 44 ||  symbolCount == 49 || symbolCount == 54 || symbolCount == 59 )
    {
      //Busted!
      //Serial.println();
      if ( debug )
      {
        Serial.println("*1*Busted*1*");
      }
      //precision_flag = 0;
      symbolCount = 0;
    }
    else
    {
      symbolArray[symbolCount++] = 1;
    }
  } // end of symbol == 1
  else if ( symbol == 0 )
  {
    if ( symbolCount == 0 )
    {
      symbolCount = 0;
    }
    else if (symbolCount == 34 )
    {
      int myDay = symbolArray[22] * 200 + symbolArray[23] * 100
                  + symbolArray[25] * 80 + symbolArray[26] * 40 + symbolArray[27] * 20 + symbolArray[28] * 10
                  + symbolArray[30] * 8 + symbolArray[31] * 4 + symbolArray[32] * 2 + symbolArray[33] * 1;
      if ( myDay > 366 )
      {
        if ( debug )
        {
          Serial.println();
          Serial.print("Day out of range (1-366): ");
          Serial.println(myDay);
        }
        badData++;
      }
      else
      {
        // day range sent is 1-365  (366 on leap year)
        newTime += (myDay - 1) * __seconds_in_a_day;
        if ( debug )
        {
          Serial.println();
          Serial.print("day of the year: ");
          Serial.println(myDay);
        }
      }
      symbolArray[symbolCount++] = 0;
    }
    else if (symbolCount == 44 )  // DUT1 offset
    {
      // it is really a floating point number.
      // but we will handle that when we display it.
      offsetDUT1 = symbolArray[40] * 8 + symbolArray[41] * 4 + symbolArray[42] * 2
                   + symbolArray[43];
      if ( symbolArray[37] == 1 && symbolArray[36] == 0 && symbolArray[38] == 0)
      {
        signDUT1 = '-';
      }
      else if (symbolArray[37] == 0 && symbolArray[36] == 1 && symbolArray[38] == 1)
      {
        signDUT1 = '+';
      }
      else
      {
        if ( debug )
        {
          Serial.println();
          Serial.print("DUT1 sign conflict: ");
        }
        badData++;
      }
      symbolArray[symbolCount++] = 0;
    }
    else if (symbolCount == 54 )
    {
      int myYear = symbolArray[45] * 80 + symbolArray[46] * 40 + symbolArray[47] * 20 + symbolArray[48] * 10
                   + symbolArray[50] * 8 + symbolArray[51] * 4 + symbolArray[52] * 2 + symbolArray[53] * 1;

      if ( myYear > 99 )
      {
        if ( debug )
        {
          Serial.println();
          Serial.print("year out of range (0-99): ");
          Serial.println(myYear);
        }
        badData++;
      }
      else
      {
        newTime += __seconds_years[myYear - 20];
        if ( debug )
        {
          Serial.println();
          Serial.print("year: ");
          Serial.println(myYear);
        }

      }
      symbolArray[symbolCount++] = 0;
    }
    else if ( symbolCount == 9 || symbolCount == 19 || symbolCount == 29 ||
              symbolCount == 39 || symbolCount == 49 )
    {
      //Busted!
      //Serial.println();
      if ( debug )
      {
        Serial.println("*0*Busted*0*");
      }
      symbolCount = 0;
      //precision_flag = 0;
    }
    else
    {
      symbolArray[symbolCount++] = 0;
    }
  }  // end of symbol == 0

  if ( phCount >= PH_PERIOD ) // we've gone one period without success
  {
    setPrecisionForPeriod(PH_NONE, true);
    phCount = 0;
    displayPrecisionMessage();
    Serial.println();
  }
  else
  {
    phCount++;
  }
}

// Cross Correlation
// Sample the signal strength 100 times per second.
// We call each sample a nubbin.
// Interrupt every 10 ms and make note of the current signal strength.
// Initialize patterns for the Zero, One and Mark symbols.
// Collect nubbins up to nubbinMax  (slightly less than 100),
//  then do the cross correlation checks and be ready in time
//  for the next minute to start.
// Cross correlation uses a floating center with a range on
//  either side called maxDelay.  As the system runs, we
//  try to decrease maxDelay so we can do less math when the
//  signal is strong.

float patternZero[100];
float meanZero = 0;
float sxZero = 0;
float patternOne[100];
float meanOne = 0;
float sxOne = 0;
float patternMark[100];
float meanMark = 0;
float sxMark = 0;
float meanNubbins = 0;
float syNubbins = 0;
volatile int newNubbin;
float nubbins[300];
int nubbinCount = 0;
int nubbinMax = 95;
int center = nubbinMax;
float scoreBucket[200];

int maxDelay = nubbinMax / 2; // half of the nubbin count

int offsetZero; // Keeping track of what offset was our best correlation
int offsetOne;  //   for each symbol.  When signal is strong they should
int offsetMark; //   all be close to the same number.

void initializePatternsAndMeans()
{
  int i;
  // Official patterns:
  //     Zero is 200ms low, 800ms high
  //     One  is 500ms low, 500ms high
  //     Mark is 800ms low, 200ms high
  // I'm offsetting my pattern by 100 ms because
  //   I am not employing all 100 nubbins. Didn't want to rob
  //   the last few ms of high signal from the Mark.
  // Also I wanted to try to get the calcuations done as close
  // to the actual time mark as possible.
  //
  for (i = 0; i < 10; i++) // 0.0 to 0.10s  All low
  {
    patternZero[i] = 10;
    patternOne[i] = 10;
    patternMark[i] = 10;
    meanZero += 10;
    meanOne += 10;
    meanMark += 10;
  }
  for (; i < 40; i++)   // 0.10 to 0.40s  Zero high
  {
    patternZero[i] = 90;
    patternOne[i] = 10;
    patternMark[i] = 10;
    meanZero += 90;
    meanOne += 10;
    meanMark += 10;
  }
  for (; i < 70; i++)   // 0.40 to 0.70s Zero & One high
  {
    patternZero[i] = 90;
    patternOne[i] = 90;
    patternMark[i] = 10;
    meanZero += 90;
    meanOne += 90;
    meanMark += 10;
  }
  for (; i < 90; i++)  // 0.70 to 0.90s  All high
  {
    patternZero[i] = 90;
    patternOne[i] = 90;
    patternMark[i] = 90;
    meanZero += 90;
    meanOne += 90;
    meanMark += 90;
  }
  for (; i < 100; i++) // 0.90 to 1.0s  all low
  {
    patternZero[i] = 10;
    patternOne[i] = 10;
    patternMark[i] = 10;
    meanZero += 10;
    meanOne += 10;
    meanMark += 10;
  }

  meanZero = meanZero / 100;
  meanOne = meanOne / 100;
  meanMark = meanMark / 100;

  for (i = 0; i < nubbinMax; i++)
  {
    sxZero  += (patternZero[i] - meanZero) * (patternZero[i] - meanZero);
    sxOne   += (patternOne[i]  - meanOne)  * (patternOne[i]  - meanOne);
    sxMark  += (patternMark[i] - meanMark) * (patternMark[i] - meanMark);
  }

  for (i = 0; i < 200; i++)
  {
    scoreBucket[i] = 0;
  }
  //Serial.print("initialize patterns ");
  //Serial.print("sxZero ");
  //Serial.print(sxZero);
  //Serial.print(" meanZero ");
  //Serial.println(meanZero);

}

// This is our interrupt routine.
// All it does is count new nubbins
void nubbinBorn()
{
  // The birth of a new nubbin
  newNubbin++;
}

// After we have gathered sufficient nubbins,
// use this to calculate mean and deviation
void calculateNubbinStats()
{
  int i;

  meanNubbins = 0;
  for (i = 0; i < nubbinMax; i++)
  {
    meanNubbins += nubbins[i];
  }
  meanNubbins = meanNubbins / nubbinMax;

  syNubbins = 0;
  for (i = 0; i < nubbinMax; i++)
  {
    syNubbins  += (nubbins[i] - meanNubbins) * (nubbins[i] - meanNubbins);
  }
}

// from  http://paulbourke.net/miscellaneous/correlate/
float crossCorrelationZero()
{
  int i, j, d;
  float mx, my, sx, sy, sxy, denom, r, maxr;

  //Serial.println(meanZero);
  mx = meanZero;
  my = meanNubbins;

  sx = sxZero;
  sy = syNubbins;
  denom = sqrt(sx * sy);

  maxr = 0;
  for ( d = (center - maxDelay); d < (center + maxDelay); d++)
  {
    sxy = 0;
    for (i = 0; i < nubbinMax; i++)
    {
      j = i + d;
      sxy += (patternZero[i] - mx) * (nubbins[j] - my);
    }
    r = sxy / denom;
    if ( maxr < r )
    {
      maxr = r;
      offsetZero = d;
    }
  }
  return maxr;
}

float crossCorrelationOne()
{
  int i, j, d;
  float mx, my, sx, sy, sxy, denom, r, maxr;

  //Serial.println(meanOne);
  mx = meanOne;
  my = meanNubbins;

  sx = sxOne;
  sy = syNubbins;
  denom = sqrt(sx * sy);

  maxr = 0;
  for ( d = (center - maxDelay); d < (center + maxDelay); d++)
  {
    sxy = 0;
    for (i = 0; i < nubbinMax; i++)
    {
      j = i + d;
      // if( j < 0 || j >= 90 )
      //   continue;
      // else
      sxy += (patternOne[i] - mx) * (nubbins[j] - my);
    }
    r = sxy / denom;
    if ( maxr < r )
    {
      maxr = r;
      offsetOne = d;
    }
  }

  return maxr;
}

float crossCorrelationMark()
{
  int i, j, d;
  float mx, my, sx, sy, sxy, denom, r, maxr;

  //Serial.println(meanMark);
  mx = meanMark;
  my = meanNubbins;

  sx = sxMark;
  sy = syNubbins;
  denom = sqrt(sx * sy);

  maxr = 0;
  for ( d = (center - maxDelay); d < (center + maxDelay); d++)
  {
    sxy = 0;
    for (i = 0; i < nubbinMax; i++)
    {
      j = i + d;
      sxy += (patternMark[i] - mx) * (nubbins[j] - my);
    }
    r = sxy / denom;
    if ( maxr < r )
    {
      maxr = r;
      offsetMark = d;
    }
  }
  return maxr;
}

// todo:  The floating center idea seems to work very well when
//          the center is near nubbinMax  (90-100 range) and not
//          so well when our center is down below 80 or above 120.
//          I haven't figured out why.

// Compare our collection of nubbins with the patterns for Zero, One and Mark

void detectSymbol() {
  //  static unsigned long secStart = 0;
  int localNewNubbin = 0;
  float guessZero, guessOne, guessMark;
  static int secondCount = 0;
  char bit;
  int bitVal;

  // check if we just ticked over a new nubbin
  // This works because our loop() visits this function more often
  //   than once every 10ms.
  // This is our only interface point with the interrupt routine.
  noInterrupts();
  if ( newNubbin > 0 )
  {
    localNewNubbin = newNubbin;
    newNubbin = 0;
  }
  interrupts();

  //   ...put current signal value in nubbin array ..
  //Serial.print(localNewNubbin);
  if ( localNewNubbin > 0 )
  {
    if ( localNewNubbin > 1 ) // watching for a skipped beat
    {
      if ( debug )
      {
        Serial.println("@@@@@");
      }
    }
    localNewNubbin = 0;

    if ( nubbinCount < nubbinMax )
    {
      // Three copies of the nubbins in one long array.
      // This facilitates the floating center for our cross correlation
      //    calculation.
      nubbins[nubbinCount] = wwvb_signal;
      nubbins[nubbinCount + nubbinMax] = wwvb_signal;
      nubbins[nubbinCount + nubbinMax + nubbinMax] = wwvb_signal;
      // nubbinCount++;

      //   and prepare for the next happy arrival
    }
    else if ( nubbinCount >= 100 ) // 1-100 = 100 nubbins per second
    {
      // For the last few nubbins we don't even mark them down.
      // We don't want to overwrite the duplicates which start at nubbinMax + 1
      // Hopefully we have our calculations done and are ready for the
      //   top of the next second coming up fast.
      nubbinCount = 0;

    }
    else if (nubbinCount == nubbinMax)
    {
      //Serial.println();
      // Serial.println("check Correlation");
      calculateNubbinStats();
      guessZero = crossCorrelationZero();
      guessOne = crossCorrelationOne();
      guessMark = crossCorrelationMark();


      if ( guessZero >= guessOne && guessZero >= guessMark)
      {
        bit = '0';
        bitVal = 0;
      }
      else if (  guessOne >= guessMark)
      {
        bit = '1';
        bitVal = 1;
      }
      else
      {
        bit = 'M';
        bitVal = 2;
      }

      Serial.print(bit);

      // write the current bit symbol in the
      // upper right corner of the display
      tft.fillRect(291, 5, 18, 18, ILI9341_BLACK);
      tft.setFont(Arial_12);
      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(295, 8);
      tft.print(bit);

      // save some indication of how strong our guesses are
      scoreBucket[offsetZero] += guessZero;
      scoreBucket[offsetOne] += guessOne;
      scoreBucket[offsetMark] += guessMark;

      secondCount++;

      if ( secondCount == 60 )
      {
        secondCount = 0;

        // Correlation offset adjust
        // Find the offset that gave us the highest scores.
        float m = 0;
        int maxi = 0;
        for (int i = 0; i < nubbinMax * 2; i++)
        {
          if ( scoreBucket[i] > m )
          {
            m = scoreBucket[i];
            maxi = i;
          }
          scoreBucket[i] = 0;
        }
        if ( debug )
        {
          Serial.println();
          Serial.print(" ==Current offset guess==: ");
          Serial.print(m);
          Serial.print(" : ");
          Serial.print(maxi);
        }

        // If we are out of the middle range, do a short delay
        //  and try again
        if ( maxi < 75 || maxi > 125 )
        {
          //noInterrupts();
          center = nubbinMax;
          maxDelay = 50;
          delay(320);
          //interrupts()
        }
        // Center is good, now see if we can narrow the search
        //
        // Strong signals mean that one bucket is getting a substantial
        //  portion of the score.  If the highest scoring bucket is
        //  strong, we can narrow the search and save some work and
        //  lower potential for false matches.
        // Number of buckets we are using is 2*maxDelay.
        // (100/maxDelay) is 2 when maxDelay is 50.
        //
        // Minimum window should be large enough to account for
        //  oscillator drift
        else if ( m > (100 / maxDelay ) && maxi > 50 && maxi < 150)
        {
          center = maxi;
          if ( maxDelay >= 10 )
          {
            maxDelay = maxDelay - 5;
          }
        }
        // Or maybe we should widen the search
        else if ( m < (100 / maxDelay) )
        {
          if ( maxDelay < 40 )
          {
            maxDelay++;
          }
        }
        if ( debug )
        {
          Serial.print(" maxDelay ");
          Serial.print(maxDelay);
          Serial.println();
        }
      }
      if ( debug > 2 )
      {
        Serial.print("  Zero  ");
        Serial.print(guessZero);
        Serial.print("  ");
        Serial.print(offsetZero);

        Serial.print(" One ");
        Serial.print(guessOne);
        Serial.print("  ");
        Serial.print(offsetOne);

        Serial.print("  Mark  ");
        Serial.print(guessMark);
        Serial.print("  ");
        Serial.print(offsetMark);

        Serial.println();
      }

      decode(bitVal);
    }
    nubbinCount++;

    if ( nubbinCount % 10 == 0)
    {
      doOtherStuff = true;
    }
  }

}


void spectrum() { // spectrum analyser code by rheslip - modified

  static int barm [512];

  // Serial.println("spectrum draw");

  for (unsigned int x = 2; x < FFT_points / 2; x++) {
    int bar = abs(myFFT.output[x]) * (int)(displayscale * 2.0);
    if (bar > 175) bar = 175;

    // this is a very simple first order IIR filter to smooth the reaction of the bars
    bar = 0.05 * bar + 0.95 * barm[x];
    tft.drawPixel(x / 2 + 10, 210 - barm[x], ILI9341_BLACK);
    if ( x == WWVB_bin)
    {
      tft.drawPixel(x / 2 + 10, 210 - bar, ILI9341_RED);
    }
    else
    {
      tft.drawPixel(x / 2 + 10, 210 - bar, ILI9341_WHITE);
    }
    barm[x] = bar;
  }
  tft.drawPixel(WWVB_bin / 2 + 10, 210 - barm[WWVB_bin], ILI9341_RED);
} // end void spectrum


void setI2SFreq(int freq) {

  // PLL between 27*24 = 648MHz und 54*24=1296MHz
  int n1 = 4; //SAI prescaler 4 => (n1*n2) = multiple of 4
  int n2 = 1 + (24000000 * 27) / (freq * 256 * n1);
  double C = ((double)freq * 256 * n1 * n2) / 24000000;
  int c0 = C;
  int c2 = 10000;
  int c1 = C * c2 - (c0 * c2);
  set_audioClock(c0, c1, c2, true);
  CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
               | CCM_CS1CDR_SAI1_CLK_PRED(n1 - 1) // &0x07
               | CCM_CS1CDR_SAI1_CLK_PODF(n2 - 1); // &0x3f

  //START//Added afterwards to make the SAI2 function at the desired frequency as well.

  CCM_CS2CDR = (CCM_CS2CDR & ~(CCM_CS2CDR_SAI2_CLK_PRED_MASK | CCM_CS2CDR_SAI2_CLK_PODF_MASK))
               | CCM_CS2CDR_SAI2_CLK_PRED(n1 - 1) // &0x07
               | CCM_CS2CDR_SAI2_CLK_PODF(n2 - 1); // &0x3f)

  //END//Added afterwards to make the SAI2 function at the desired frequency as well.
}

/*
  void check_processor() {
  if (second_timer.check() == 1) {
    Serial.print("Proc = ");
    Serial.print(AudioProcessorUsage());
    Serial.print(" (");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("),  Mem = ");
    Serial.print(AudioMemoryUsage());
    Serial.print(" (");
    Serial.print(AudioMemoryUsageMax());
    Serial.println(")");
    tft.fillRect(100, 120, 200, 80, ILI9341_BLACK);
    tft.setCursor(10, 120);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setFont(Arial_14);
    tft.print ("Proc = ");
    tft.setCursor(100, 120);
    tft.print (AudioProcessorUsage());
    tft.setCursor(180, 120);
    tft.print (AudioProcessorUsageMax());
    tft.setCursor(10, 150);
    tft.print ("Mem  = ");
    tft.setCursor(100, 150);
    tft.print (AudioMemoryUsage());
    tft.setCursor(180, 150);
    tft.print (AudioMemoryUsageMax());

    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
  }
  } // END function check_processor
*/

void displayClock() {

  //Serial.print(".");

  uint8_t hour10 = hour() / 10 % 10;
  uint8_t hour1 = hour() % 10;
  uint8_t minute10 = minute() / 10 % 10;
  uint8_t minute1 = minute() % 10;
  uint8_t second10 = second() / 10 % 10;
  uint8_t second1 = second() % 10;
  uint8_t time_pos_shift = 26;
  tft.setFont(Arial_28);
  tft.setTextColor(ILI9341_WHITE);
  uint8_t dp = 14;

  tft.setFont(Arial_28);
  tft.setTextColor(ILI9341_WHITE);

  // set up ":" for time display
  if (!timeflag) {
    tft.setCursor(pos_x_time + 2 * time_pos_shift, pos_y_time);
    tft.print(":");
    tft.setCursor(pos_x_time + 4 * time_pos_shift + dp, pos_y_time);
    tft.print(":");
    tft.setCursor(pos_x_time + 7 * time_pos_shift + 2 * dp, pos_y_time);
    // tft.print("UTC");
  }

  if (hour10 != hour10_old || !timeflag) {
    tft.setCursor(pos_x_time, pos_y_time);
    tft.fillRect(pos_x_time, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    if (hour10) tft.print(hour10);  // do not display, if zero

  }
  if (hour1 != hour1_old || !timeflag) {
    tft.setCursor(pos_x_time + time_pos_shift, pos_y_time);
    tft.fillRect(pos_x_time  + time_pos_shift, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    tft.print(hour1);  // always display
  }
  if (minute1 != minute1_old || !timeflag) {
    tft.setCursor(pos_x_time + 3 * time_pos_shift + dp, pos_y_time);
    tft.fillRect(pos_x_time  + 3 * time_pos_shift + dp, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    tft.print(minute1);  // always display
  }
  if (minute10 != minute10_old || !timeflag) {
    tft.setCursor(pos_x_time + 2 * time_pos_shift + dp, pos_y_time);
    tft.fillRect(pos_x_time  + 2 * time_pos_shift + dp, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    tft.print(minute10);  // always display
  }
  if (second10 != second10_old || !timeflag) {
    tft.setCursor(pos_x_time + 4 * time_pos_shift + 2 * dp, pos_y_time);
    tft.fillRect(pos_x_time  + 4 * time_pos_shift + 2 * dp, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    tft.print(second10);  // always display
  }
  if (second1 != second1_old || !timeflag) {
    tft.setCursor(pos_x_time + 5 * time_pos_shift + 2 * dp, pos_y_time);
    tft.fillRect(pos_x_time  + 5 * time_pos_shift + 2 * dp, pos_y_time, time_pos_shift, time_pos_shift + 2, ILI9341_BLACK);
    tft.print(second1);  // always display
  }

  if (hour1 != hour1_old || !timeflag) {
    tft.setCursor(pos_x_time + 6 * time_pos_shift + 2 * dp, pos_y_time);
    tft.fillRect(pos_x_time  + 6 * time_pos_shift + 2 * dp, pos_y_time, time_pos_shift + 3, 11, ILI9341_BLACK);

    tft.setFont(Arial_9);
    tft.setTextColor(ILI9341_RED);
    tft.print(" ");
    tft.print(signDUT1);
    tft.print("0.");
    tft.print(offsetDUT1);
  }

  hour1_old = hour1;
  hour10_old = hour10;
  minute1_old = minute1;
  minute10_old = minute10;
  second1_old = second1;
  second10_old = second10;

  timeflag = 1;

} // end function displayTime

const char* Months[13] = {
  "Zero", "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December"
};

void displayDate() {
  char string99 [20];

  static int oldDay = 0, oldMonth = 0, oldYear = 0;
  int newDay, newMonth, newYear;

  newDay = day();
  newMonth = month();
  newYear = year();

  if ( newDay != oldDay || newMonth != oldMonth || newYear != oldYear )
  {

    tft.fillRect(pos_x_date, pos_y_date, 320 - pos_x_date, 20, ILI9341_BLACK); // erase old string
    tft.setTextColor(ILI9341_ORANGE);
    tft.setFont(Arial_16);
    tft.setCursor(pos_x_date, pos_y_date);

    if ( debug )
    {
      Serial.println("displayDate");
    }
    // Date: %s, %d.%d.20%d P:%d %d", Days[weekday-1], day, month, year
    // sprintf(string99, "%s, %02d.%02d.%04d", Days[weekday()], newDay, newMonth, newYear);
    // todo:  the weekday() function caused me trouble.  Not infrequently the whole
    //   system would lock up at this point.  So I took it out.  May revisit it later.
    // sprintf(string99, "%02d.%02d.%04d", newDay, newMonth, newYear);
    sprintf(string99, "%s %02d, %04d", Months[newMonth], newDay, newYear);
    tft.print(string99);

    oldDay = newDay;
    oldMonth = newMonth;
    oldYear = newYear;

    if ( leapSecondWarning )
    {
      tft.fillRect(10, 160, 118, 13, ILI9341_BLACK); // erase old string
      tft.setTextColor(ILI9341_ORANGE);
      tft.setFont(Arial_9);
      tft.setCursor(10, 160);
      tft.print("leap second warning");
    }
    else
    {
      tft.fillRect(10, 160, 118, 13, ILI9341_BLACK);
    }

    Serial.print("LY");
    Serial.println(leapYear);
    if ( leapYear )
    {
      tft.fillRect(10, 175, 55, 13, ILI9341_BLACK); // erase old string
      tft.setTextColor(ILI9341_ORANGE);
      tft.setFont(Arial_9);
      tft.setCursor(10, 175);
      tft.print("leap year");
    }
    else
    {
      tft.fillRect(10, 175, 55, 13, ILI9341_BLACK);
    }

    if ( statusDST > 0 )
    {
      tft.fillRect(10, 190, 103, 13, ILI9341_BLACK); // erase old string
      tft.setTextColor(ILI9341_ORANGE);
      tft.setFont(Arial_9);
      tft.setCursor(10, 190);
      if ( statusDST == 2 )
      {
        tft.print("DST begins today");
      }
      else if ( statusDST == 1 )
      {
        tft.print("DST ends today");
      }
      else
      {
        tft.print("DST");
      }
    }
    else
    {
      tft.fillRect(10, 190, 103, 13, ILI9341_BLACK);
    }
  }
} // end function displayDate
