#include "RTClib.h"

#define MAX_FREQ 11999
#define MIN_FREQ 1600
#define MAX_FREQ20 14350
#define MIN_FREQ20 14000
#define NUM_BANDS 4

// Feinabstimmung
#define MIN_VALUE_POT 646

// Definitionen der LCD Pins
#define LCD_RS			12
#define LCD_ENABLE	11
#define LCD_D4			5
#define LCD_D5			4
#define LCD_D6			3
#define LCD_D7			2

// Definitionen der LED Anzeigen
#define MHZ_D3 46
#define MHZ_C3 44
#define MHZ_B3 42
#define MHZ_A3 40

#define KHZ_D2 41
#define KHZ_C2 43
#define KHZ_B2 45
#define KHZ_A2 47

#define KHZ_D1 30
#define KHZ_C1 28
#define KHZ_B1 26
#define KHZ_A1 24

#define KHZ_D0 25
#define KHZ_C0 27
#define KHZ_B0 29
#define KHZ_A0 31

#define POT_INPUT A0

#define ZA_2M			  46   //  14  - d3     1 MHz
#define ZB_2M			  44   //  12  - c3
#define ZC_2M			  42   //  10  - b3 
#define ZD_2M			  40   //   9  - a3
#define EA_2M			  41   //   8  - d2   100 KHz
#define EB_2M			  43   //   6  - c2
#define EC_2M			  45   //   4  - b2
#define ED_2M			  47   //   2  - a2

#define ZA_70CM			30   //  14  - d1    10 KHz
#define ZB_70CM			28   //  12  - c1
#define ZC_70CM			26   //  10  - b1
#define ZD_70CM			24   //   9  - a1 
#define EA_70CM			25   //   8  - d0     1 KHz
#define EB_70CM			27   //   6  - c0
#define EC_70CM			29   //   4  - b0
#define ED_70CM			31   //   2  - a0

#define ENC_PUSH	 17
#define ENC_A			 19
#define ENC_B		 	 18
#define ENC_PULSES	4

#define ANT_20 10

#include <LiquidCrystal.h>
#include <Encoder.h>

LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
Encoder encoder(ENC_A, ENC_B);
RTC_DS3231 rtc;

struct Band {
  int lower;
  int upper;
  char band_name[4];
};

Band band_table[NUM_BANDS] = {
  {1810, 2000, "160"},
  {3500, 3800, " 80"},
  {7000, 7200, " 40"},
  {14000, 14350, " 20"}
};

// LUT fuer die BCD Codierung
unsigned char bcd_channels[4][4] = {
  {KHZ_A0,KHZ_B0,KHZ_C0,KHZ_D0},  // 1KHz
  {KHZ_A1,KHZ_B1,KHZ_C1,KHZ_D1},  // 10 KHz
  {KHZ_A2,KHZ_B2,KHZ_C2,KHZ_D2},  // 100 KHZ
  {MHZ_A3,MHZ_B3,MHZ_C3,MHZ_D3},  // 1MHz
};

unsigned char month_days[12] = {31,28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Frequenz und Verschiebung fuer Anzeige am LCD
unsigned int freq = 1600; // Frequenz in kHz
char freq_index = 4;
int potValue;
bool adjustTimeState = false;
bool is20m = false;
int currPotVal = 0;
int time_index = 6;
DateTime lastTimestamp;
DateTime adjustDateTime;

void setup() {
  lcd.begin(20,2);
  rtc.begin();

  // Setze RTC Zeit auf die aktuelle Programmierzeit
  if(rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  lastTimestamp = rtc.now();
  refreshScreen();

  for(unsigned char i = 0; i < 4; i++){
    for(unsigned char j = 0; j < 4; j++){
      pinMode(bcd_channels[i][j], OUTPUT);
    }
  }

  pinMode(POT_INPUT, INPUT);
  potValue = analogRead(POT_INPUT);
  
  pinMode(ANT_20, OUTPUT);

	pinMode(ENC_PUSH, INPUT_PULLUP);
	encoder.write(0);
  writeFreq();

}

// Zeichnet alle wichtigen Infos auf das LCD
void refreshScreen(){
  lcd.clear();
  lcd.noCursor();
  lcd.setCursor(0,0);
  // Schreibt die Frequenz in einen formatierten String
  char* freq_string = malloc(sizeof(char) * 8);
  freq_string[7] = 0;
  // Format fuer **.*** mit 0-Padding, da diese sonst nicht angezeigt werden
  snprintf(freq_string, 8, "%02d.%03d", freq/1000, freq%1000);
  lcd.print(freq_string);
  free(freq_string);
  refreshDecimal(false);
  lcd.print(" MHz");
  
  refreshTime(false);

  setIndicatorCursor();
  
}

void refreshTime(bool setIndicator) {
  lcd.setCursor(0, 1);

  DateTime now = adjustTimeState ? adjustDateTime : rtc.now();
  lcd.print(now.timestamp(DateTime::TIMESTAMP_TIME));
  lcd.print(" ");
  if(now.day() < 10) {
    lcd.print("0");
  }
  lcd.print(now.day(), 10);
  lcd.print(".");
  if(now.month() < 10) {
    lcd.print("0");
  }
  lcd.print(now.month(), 10);
  lcd.print(".");
  int short_year = now.year() % 100;
  if(short_year < 10) {
    lcd.print("0");
  }
  lcd.print(short_year, 10);
  for(unsigned char i = 0; i < NUM_BANDS; i++){
    if(band_table[i].lower <= freq && band_table[i].upper >= freq) {
      lcd.setCursor(16, 0);
      lcd.print(band_table[i].band_name);
      lcd.print("m");
      break;
    }
  }

  if(setIndicator) {
    setIndicatorCursor();
  }
}

void refreshDecimal(bool setCursor){
  char* freq_string = malloc(sizeof(char) * 4);
  lcd.setCursor(6, 0);
  snprintf(freq_string, 4, ".%02d", potValue);
  lcd.print(freq_string);
  free(freq_string);
  if(setCursor){
    setIndicatorCursor();
  }
}

// Zeige aktuelle Cursor Position
// Mit Unterscheidung fuer den Dezimalpunkt  
void setIndicatorCursor(){
  if(!adjustTimeState) {
    lcd.setCursor((freq_index >= 3 ? 4 : 5) - freq_index, 0);
    lcd.cursor();
  }else {
    int x_index;
    switch(time_index){
      case 0: {x_index = 13; break;}
      case 1: {x_index = 10; break;}
      case 2: {x_index = 7; break;}
      case 3: {x_index = 6; break;}
      case 4: {x_index = 4; break;}
      case 5: {x_index = 3; break;}
      case 6: {x_index = 1; break;}
    }
    lcd.setCursor(x_index, 1);
    lcd.cursor();
  }
}

// Schreibt ein byte in den vorgegebenen BCD Kanal
void write_bcd(unsigned char index, uint8_t value){
  digitalWrite(bcd_channels[index][0], value & 0b1);
  digitalWrite(bcd_channels[index][1], (value & 0b10) >> 1);
  digitalWrite(bcd_channels[index][2], (value & 0b100) >> 2);
  digitalWrite(bcd_channels[index][3], (value & 0b1000)>> 3);
}

// Schreibt eine Frequenz Zahl in den BCD Kanal
void write_digit(unsigned char index, uint8_t value){
  if(index < 3){
    // Fuer alle Stellen, ausser die MHz Stelle
    // Muessen die Zahlen zwischen 0 und 9 invertiert werden
    // um das gewuenschte Mapping von BCD und Frequenz im SEG15 zu erzeugen
    uint8_t invert_byte = 9 - value;
    write_bcd(index, invert_byte);
  }else {
    // Fuer die MHz Stelle muss das selbe mit 11 geschehen
    uint8_t invert_byte = 11 - value;
    write_bcd(index, invert_byte);
  }
}

// Schreibt die aktuelle Frequenz zum SEG15
void writeFreq(){
  unsigned int temp_freq = freq;
  // Schreibt die einer Stelle
  write_digit(0, temp_freq % 10);
  temp_freq /= 10;
  // Schreibt die 10er Stelle
  write_digit(1, temp_freq % 10);
  temp_freq /= 10;
  // Schreibt die 100er Stelle
  write_digit(2, temp_freq % 10);
  temp_freq /= 10;
  // Schreibt die MHz-Stelle
  if(is20m) {
    write_digit(3, 11);
  }else {
    write_digit(3, temp_freq);
  }
  digitalWrite(ANT_20, is20m ? HIGH : LOW);
}

uint32_t get_exp10(uint8_t y) {
  uint32_t out = 1;
  for(uint8_t i = 0; i < y; i++){
    out *= 10;
  }
  return out;
}

unsigned long lastUpdate = 0;
void loop() {
  unsigned long currTime = millis();
  bool shouldRefresh = false;
  if(!digitalRead(ENC_PUSH)){
    unsigned long startTime = currTime;
    while(!digitalRead(ENC_PUSH)) {
      delay(10);
    }
    if((millis() - startTime) >= 2000){
      adjustTimeState = !adjustTimeState;
      if(!adjustTimeState) {
        lastTimestamp = adjustDateTime;
        rtc.adjust(adjustDateTime);
      }else {
        adjustDateTime = rtc.now();
      }
      shouldRefresh = true;
    }else if(!adjustTimeState) {
      // Schiebe die Cursor Position um eins nach links
      freq_index--;
      if(freq_index < 0) {
        freq_index = 4;
      }
      shouldRefresh = true;
    }else {
      time_index--;
      if(time_index < 0) {
        time_index = 6;
      }
      shouldRefresh = true;
    }
  }

  if(!adjustTimeState) {
    int currPotVal = (int)map(analogRead(POT_INPUT), MIN_VALUE_POT, 1023, 0, 99);
    if(abs(currPotVal - potValue) >= 2 || (currPotVal != potValue && currPotVal == 0) || (currPotVal != potValue && currPotVal == 99)){
      potValue = currPotVal;
      refreshDecimal(true);
      delay(100);
    }

    if (encoder.read() / ENC_PULSES) {
        // Der Faktor um den die Frequenz erhoeht/gesenkt wird
        int new_freq = (int)freq - (int)(encoder.read() / ENC_PULSES) * get_exp10(freq_index);
        if((new_freq >= MIN_FREQ && new_freq <= MAX_FREQ) || (new_freq >= MIN_FREQ20 && new_freq <= MAX_FREQ20)){
          freq = new_freq;
        }else if(freq <= MAX_FREQ && new_freq > MAX_FREQ) {
          is20m = true;
          
          //if(freq + 3000 <= MAX_FREQ || (freq + 3000 > MAX_FREQ && freq + 3000 < MIN_FREQ20) || freq + 3000 > MAX_FREQ20) {
          //  freq = MIN_FREQ20;
          //}else {
            freq = min(new_freq + 3000, MAX_FREQ20);
          //}
        }else if(freq >= MIN_FREQ20 && new_freq < MIN_FREQ20) {
          is20m = false;
          freq = MAX_FREQ + 1 - get_exp10(freq_index);
        }
        encoder.write(0);
        writeFreq();
        shouldRefresh = true;
    }
    
    if(lastUpdate >= 100) {
      DateTime now = rtc.now();
      if((now - lastTimestamp).seconds() >= 1) {
        refreshTime(true);
        lastTimestamp = now;
      }
      lastUpdate = 0;
    }

    lastUpdate += millis() - currTime;

  }else {
    if(encoder.read() / ENC_PULSES) {
      int factor = -(int)(encoder.read() / ENC_PULSES);
      int sign = (factor < 0) ? -1 : 1;
      switch(time_index) {
        case 0: {adjustDateTime = adjustDateTime + TimeSpan(sign * month_days[adjustDateTime.month()-1],0,0,0);break;} // month
        case 1: {adjustDateTime = adjustDateTime + TimeSpan(factor,0,0,0); break;} // day
        case 2: {adjustDateTime = adjustDateTime + TimeSpan(0,0,0,factor);break;} // 0s
        case 3: {adjustDateTime = adjustDateTime + TimeSpan(0,0,0,factor*10);break;} // s0
        case 4: {adjustDateTime = adjustDateTime + TimeSpan(0,0,factor,0);break;} // 0m
        case 5: {adjustDateTime = adjustDateTime + TimeSpan(0,0,factor*10,0);break;} // m0
        case 6: {adjustDateTime = adjustDateTime + TimeSpan(0,factor,0,0);break;} // 0h
        default: break;
      }
      shouldRefresh = true;
      encoder.write(0);
    }
  }  

  if(shouldRefresh) {
    refreshScreen();
  }
}

