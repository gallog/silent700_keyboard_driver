/*
 * Driver per tastiera Silent 700 - v1 - 30/08/2016
 * Arduino mini 16mhz
 *
 * Connettore tastiera (pull up da inserire sulla scheda):
 *                 +-------+
 *             Gnd | 20 10 | C0
 *              C4 | 19  9 | C1
 *              C5 | 18  8 | C2
 *              C6 | 17  7 | C3
 *              NC | 16  6 | NC
 *    Vcc -10k- R3 | 15  5 | R4 -10k- Vcc
 *    Vcc -10k- R2 | 14  4 | Ctrl -10k- Vcc
 *    Vcc -10k- R1 | 13  3 | R5 -10k- Vcc
 *    Vcc -10k- R0 | 12  2 | R6 -10k- Vcc
 * Vcc -10k- Shift | 11  1 | R7 -10k- Vcc
 *                 +-------+
 *
 *
 * Conettore uscita (compatibile con connettore scheda driver tastiera Apple II):
 *         +---| |----+ 
 *    +5v  | 1     12 | Data 0
 * Strobe  | 2     11 | Data 1
 *     NC  | 3     10 | Data 2
 *     NC  | 4      9 | Data 3 
 * Data 6  | 5      8 | Data 4 
 *    Gnd  | 6      7 | Data 5 
 *         +----------+ 
 * 
 * Strobe alto per OUTPUT_STROBE_DURATION_US=12 quando il dato e' da campionare
 * 
 */
#define KEY_SHIFT 1 
#define KEY_CTRL 8
#define R0 0 
#define R1 2 
#define R2 3 
#define R3 4 
#define R4 9 
#define R5 7 
#define R6 6 
#define R7 5
#define D0 13
#define D1 A0
#define D2 A1
#define D3 A2
#define D4 A3
#define D5 12
#define D6 11
#define STROBE     10
#define SERIAL_D   A4
#define SERIAL_CLK A5

#define SHIFT_CLOCK_DELAY_US 1
#define OUTPUT_STROBE_DURATION_US 12
#define doClock()                            \
   digitalWrite(SERIAL_CLK, HIGH);           \
   delayMicroseconds(SHIFT_CLOCK_DELAY_US);  \
   digitalWrite(SERIAL_CLK, LOW);            \
   delayMicroseconds(SHIFT_CLOCK_DELAY_US);

void sendDataToShiftRegister(byte columData);
void ouputKeyboardData(byte data);
byte decodeRow();

#define COL 7
#define ROW 8
byte keyMap[ROW][COL]      = {{'g',0,'2',13,6,'w','o'},       //13=rtn, 6=cmd
                              {'h',0,'3',27,9,'x','p'},       //27=esc, 9=paper adv
                              {'c','6','>','=',0,'s','k'},
                              {'e','8','0',' ',0,'u','m'},
                              {'f','9','1',10,0,'v','n'},
                              {'b','5','-',':','z','r','j'},
                              {'a','4','<','"','y','q','i'},
                              {'d','7','?',0,0,'t','l'}};     //10=line feed
byte keyMapShift[ROW][COL] = {{'G',0,'@',13,6,'W','O'},       //13=rtn, 6=cmd
                              {'H',0,'#',27,9,'X','P'},       //27=esc, 9=paper adv
                              {'C','^','.','+',0,'S','K'},
                              {'E','*',')',' ',0,'U','M'},
                              {'F','(','!',10,0,'V','N'},
                              {'B','%','_',';','Z','R','J'},
                              {'A','$',',','\'','Y','Q','I'},
                              {'D','&','/',0,0,'T','L'}};     //10=line feed
byte keyMapCtrl[ROW][COL] =  {{7,0,6,13,6,'w','o'},           //7=bell, 6=break, 13=rtn, 6=cmd
                              {8,0,127,27,9,'x','p'},         //8=backspace, 127=delete, 27=esc, 9=paper adv
                              {'c',']','>','}',0,'s','k'},
                              {'e','|',126,' ',0,'u','m'},    //('|' dovrebbe forse essere 221?), 126=tilde
                              {'f','\'',5,10,0,'v','n'},      //('\'' dovrebbe essere 239,backquote), 5=here is, 10=line feed
                              {'b','[','{',':','z','r','j'},
                              {'a','4','<','"','y','q','i'},
                              {'d','\\','?',0,0,'t','l'}};                 

unsigned long lastSentSameCharTimeMs = 0;
unsigned long currentPressTimeMs;
byte rowIndex[ROW] = {0,0,0,0,0,0,0,0};
byte colIndex = 0;
byte colData  = 1;
byte oldKeyboardStatus[ROW*COL];
byte currentKeyboardStatus[ROW*COL];
int timeToWaitBeforeMultiplePress;
byte charPressedIndex = 255;
byte totalPressed = 0;
byte totalReleased = 0;
byte totalNewPressed = 0;
byte oldCharPressedIndex = 255;
#define minTimeBetweenSameKeyPressMsFirstTime  550  //solo per tasto tenuto premuto la prima volta
#define minTimeBetweenSameKeyPressMsOtherTimes 40   //le battute successive vanno piu' veloci

void setup() {
  pinMode(KEY_SHIFT, INPUT);
  pinMode(KEY_CTRL, INPUT);
  pinMode(R0, INPUT);
  pinMode(R1, INPUT);
  pinMode(R2, INPUT);
  pinMode(R3, INPUT);
  pinMode(R4, INPUT);
  pinMode(R5, INPUT);
  pinMode(R6, INPUT);
  pinMode(R7, INPUT);
  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(STROBE,     OUTPUT);
  pinMode(SERIAL_D,   OUTPUT);
  pinMode(SERIAL_CLK, OUTPUT);

  timeToWaitBeforeMultiplePress = minTimeBetweenSameKeyPressMsFirstTime;
  memset(oldKeyboardStatus,0,sizeof(oldKeyboardStatus));
  memset(currentKeyboardStatus,0,sizeof(currentKeyboardStatus));

  //Serial.begin(9600);
}
void loop() {
  //decodifica
  sendDataToShiftRegister(colData);
  if (decodeRow()) {
    for (byte i = 0; i < ROW; i++) {
      if (rowIndex[i]) {
        currentKeyboardStatus[i*COL+colIndex] = 1;
      }
    }
  }
  //identificazione tasti
  if (colIndex == COL-1) {  //fine scansione colonna  
    currentPressTimeMs = millis();
    for (byte i = 0; i < ROW*COL; ++i) {
      if (currentKeyboardStatus[i] == 1) {
          ++totalPressed;
          if (charPressedIndex == 255) {
            charPressedIndex = i;
          }
          if (oldKeyboardStatus[i] == 0) { //solo un tasto premuto alla volta
            ++totalNewPressed;
            charPressedIndex = i;
         }
      } else if (oldKeyboardStatus[i] == 1) {
        ++totalReleased;
      }
    }
    //invio
    if (charPressedIndex != oldCharPressedIndex) { //nuovo
      ouputKeyboardData(charPressedIndex);
      lastSentSameCharTimeMs = currentPressTimeMs;
      timeToWaitBeforeMultiplePress = minTimeBetweenSameKeyPressMsFirstTime; 
    } else if (currentPressTimeMs - lastSentSameCharTimeMs > timeToWaitBeforeMultiplePress) { 
      ouputKeyboardData(charPressedIndex);
      lastSentSameCharTimeMs = currentPressTimeMs;    
      timeToWaitBeforeMultiplePress = minTimeBetweenSameKeyPressMsOtherTimes; //prossima volta più veloce    
    }
    
    //reset
    oldCharPressedIndex = charPressedIndex;
    if (totalReleased) {      
      charPressedIndex = 255;
      timeToWaitBeforeMultiplePress = minTimeBetweenSameKeyPressMsFirstTime; 
    }
    if (totalPressed == 0 || totalNewPressed == 0) { //con totalNewPressed si fa tornare totalReleased a 0 il prossimo giro in caso di pressione multiple
      totalReleased = 0;
    }
    totalPressed = 0;
    totalNewPressed = 0;
    memcpy(oldKeyboardStatus,currentKeyboardStatus,ROW*COL);
    memset(currentKeyboardStatus,0,ROW*COL);
  }
  colData  = colData < 64 ? colData << 1 : 1; //incrementa colonna per prossima iterazione (64=2^(COL-1))
  colIndex = colIndex+1 >= COL ? 0 : colIndex+1;
}

/*
 * Colonne attive basse, in uscita dallo shift register 
 * solo uno 0 ruotante
 * 11111110
 * 11111101
 * 11111011
 * 11110111
 * 11101111
 * 11011111
 * 10111111
 */
void sendDataToShiftRegister(byte data) {  
  byte mask = 0b10000000;
  for (byte i = 0; i < 8; i++) {
     if (data & mask) { digitalWrite(SERIAL_D, LOW);  }
     else             { digitalWrite(SERIAL_D, HIGH); }
     doClock();
     mask >>= 1;
  }
}

byte decodeRow() {
  byte rowActived = 0; 
  memset(rowIndex,0,ROW); 
  if (digitalRead(R0) == LOW) { rowIndex[0] = 1; ++rowActived; }
  if (digitalRead(R1) == LOW) { rowIndex[1] = 1; ++rowActived; }
  if (digitalRead(R2) == LOW) { rowIndex[2] = 1; ++rowActived; }
  if (digitalRead(R3) == LOW) { rowIndex[3] = 1; ++rowActived; }
  if (digitalRead(R4) == LOW) { rowIndex[4] = 1; ++rowActived; }
  if (digitalRead(R5) == LOW) { rowIndex[5] = 1; ++rowActived; }
  if (digitalRead(R6) == LOW) { rowIndex[6] = 1; ++rowActived; }
  if (digitalRead(R7) == LOW) { rowIndex[7] = 1; ++rowActived; } 
  return rowActived;
}

void ouputKeyboardData(byte index) {
  byte data;
  if (index == 255) { return; }
  if (digitalRead(KEY_CTRL) == LOW) {
    data = *(keyMapCtrl[0]+index);
  } else if (digitalRead(KEY_SHIFT) == LOW) {
    data = *(keyMapShift[0]+index);
  } else {
    data = *(keyMap[0]+index);
  }  
  // Serial.print((char)data);
  if (data & 0b01000000) { digitalWrite(D6, HIGH); } else { digitalWrite(D6, LOW); }
  if (data & 0b00100000) { digitalWrite(D5, HIGH); } else { digitalWrite(D5, LOW); }
  if (data & 0b00010000) { digitalWrite(D4, HIGH); } else { digitalWrite(D4, LOW); }
  if (data & 0b00001000) { digitalWrite(D3, HIGH); } else { digitalWrite(D3, LOW); }
  if (data & 0b00000100) { digitalWrite(D2, HIGH); } else { digitalWrite(D2, LOW); }
  if (data & 0b00000010) { digitalWrite(D1, HIGH); } else { digitalWrite(D1, LOW); }
  if (data & 0b00000001) { digitalWrite(D0, HIGH); } else { digitalWrite(D0, LOW); }
  digitalWrite(STROBE, HIGH);
  delayMicroseconds(OUTPUT_STROBE_DURATION_US);
  digitalWrite(STROBE, LOW);
}

