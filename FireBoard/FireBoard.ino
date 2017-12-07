/**
   FireBoard - fireplace simulation with strips of WS2812 (neopixel) on cardboard.
   Use 14 strips across, with 8 LEDs per strip, in serpentine layout.
   Using FastLED library ver 3.1.3

   Added kHot and kCool values, set with linear-sweep 50KOhm pot. (user can adjust "gas" level)
*/

#include <FastLED.h>

//#define DEBUG 1

/* NeoPixel Shield data pin is always 6. Change for other boards */
#define CONTROL_PIN 10     // LED data signal output.
#define GAS_INPUT_POT  A0  //  linear-sweep potentiometer voltage divider input

/* Board shape and size configuration. Sheild is 8x5, 40 pixels */
/* FireBoard is 14 strips across, with 8 LEDs per strip, in serpentine layout */
#define HEIGHT 9   // LEDs tall per strip
#define WIDTH 14    // num strips across board
#define NUM_LEDS   HEIGHT*WIDTH

// voltage divider across 5V. Input to A0.
// HOT/COOL for LOW/HIGH 'gas'
#define LOW_COOL  180
#define LOW_HOT    10

#define HIGH_COOL  40
#define HIGH_HOT  160  // above 160 gives white sparks (hotter than fire)

/* Refresh rate. Higher makes for flickerier
   Recommend small values for small displays */
//#define FPS 17
#define FPS 12
#define FPS_DELAY 1000/FPS


#define MATRIX_SERPENTINE_LAYOUT  1     // strips are flipped zig-zag.
//#define MATRIX_SERPENTINE_LAYOUT  0     // strips are linked in sequential order

/* Rate of cooling. Play with to change fire from
   roaring (smaller values) to weak (larger values) */
uint8_t kCooling = HIGH_COOL; // 145;

/* How hot is "hot"? Increase for brighter fire */
uint8_t kHot = HIGH_HOT; // 140;
//#define MAXHOT HOT*HEIGHT

CRGB leds[NUM_LEDS];
CRGBPalette16 gPal;
unsigned long start_tm;

void setup() {
  Serial.begin(115200);
  Serial.println("FireBoard");

  FastLED.addLeds<NEOPIXEL, CONTROL_PIN>(leds, NUM_LEDS);
  start_tm  = millis();

  /* Set a black-body radiation palette
     This comes from FastLED */
  gPal = HeatColors_p;

  /* Clear display before starting */
  FastLED.clear();
  FastLED.show();
  FastLED.delay(500); // Sanity before start
}


void loop() {

  random16_add_entropy( random() ); // We chew a lot of entropy

  readGasControl();

#ifdef DEBUG
  if ( millis() < start_tm + 60e3) // DEBUG FIXME turn off after 30 secs, for testing.
    Fireplace();
  else
    ClearBoard(); // clear LEDs to black.
#else
  Fireplace();
#endif


  FastLED.show();
  FastLED.delay(FPS_DELAY);
}


// clockwise = more gas (larger flames, more heat)
// anti-clockwise = less gas (smaller flames, less heat)
// use 50KOhm Potentiometer voltage divider across 5V. Center wiper wired to A0 input.
void readGasControl() {
  static unsigned long print_tm = 0;

  // read pot to get value for 'gas' to adjust kHot (and cooling too?)
  uint16_t pot = analogRead(GAS_INPUT_POT); // 50k divider across 5V
  kHot  = map(pot, 1023, 0, LOW_HOT, HIGH_HOT); // map 0-1023 -->
  kCooling = map(pot, 1023, 0, LOW_COOL, HIGH_COOL); // map 0-1023 --> cool
  //kHot = 80;
  //kCooling = 80;

#ifdef DEBUG
  // output to Serial only 1 per second.
  if (millis() - print_tm < 1e3)
    return;
  print_tm = millis();
  Serial.print("heat/cool/pot:\t");
  Serial.print(kHot, DEC);
  Serial.print("\t");
  Serial.print(kCooling, DEC);
  Serial.print("\t");
  Serial.println(pot, DEC);
#endif
}


void ClearBoard() {
  for (int i = 0; i < WIDTH * HEIGHT; i++)
    leds[i] = 0;
}

void Fireplace () {
  static unsigned int spark[WIDTH]; // base heat
  //static unsigned int last[WIDTH]; // last value, for averaging to make smoother/slower flames.
  CRGB stack[WIDTH][HEIGHT];        // stacks that are cooler

  unsigned int kMaxHot = kHot * HEIGHT;

  // 1. Generate sparks to re-heat
  for ( int i = 0; i < WIDTH; i++) {
    if (spark[i] < kHot ) {
      //      int base = kHot * 2;
      int base = kHot;
      //spark[i] = random16( base, kMaxHot ) >> 3;
      spark[i] = random16( base, kMaxHot ) >> 2;
    }
  }

  // 2. Cool all the sparks
  for ( int i = 0; i < WIDTH; i++) {
    spark[i] = qsub8( spark[i], random8(0, kCooling) >> 3 ); // div 8, will subtract 0-7 to cool spark
    //    spark[i] = qsub8( spark[i], random8(0, kCooling) >> 2 ); // div 4, will subtract 0-16 to cool spark
  }

  // 3. Build the stack
  /*    This works on the idea that pixels are "cooler"
        as they get further from the spark at the bottom */
  for ( int i = 0; i < WIDTH; i++) {
    unsigned int heat = constrain(spark[i], kHot / 2, kMaxHot);
    //unsigned int heat = constrain(spark[i], kHot / 3, kMaxHot);
    for ( int j = HEIGHT - 1; j >= 0; j--) {
      /* Calculate the color on the palette from how hot this
         pixel is */
      byte index = constrain(heat, 0, kHot);
      stack[i][j] = ColorFromPalette( gPal, index );

      /* The next higher pixel will be "cooler", so calculate
         the drop */
      unsigned int drop, newdrop = random8(0, kHot);
      //last[i] = (last[i] + 2 * newdrop) / 3; // smoothing filter
      //drop = last[i];
      drop = newdrop;
      if (drop > heat) heat = 0; // avoid wrap-arounds from going "negative"
      else heat -= drop;

      heat = constrain(heat, 0, kMaxHot);
    }
  }

  // 4. map stacks to led array
  for ( int i = 0; i < WIDTH; i++) {
    for ( int j = 0; j < HEIGHT; j++) {
      //orig     leds[(j*WIDTH) + i] = stack[i][j];
      //leds[XY(i, j)] = stack[i][j];
      //leds[ sajXY(i, j) ] = stack[i][j];
      leds[ sajXYinv(i, j) ] = stack[i][j];

    }
  }

}



/**
   Visual format: stacks[x][y] to LED[i]
   XY function with vertical strips (original below is horizontal strips)
   9 Rows: 0..8 ;  14 Cols: 0..13
       (top)
     ____  __.input
     |  |  |  (row #8)
     v  ^  v  (data direction)
     |  |__|  (row #0)
     0  1  2  (col#  0..13)
*/
uint16_t sajXY( uint8_t x, uint8_t y)
{
  uint16_t i;

#if  MATRIX_SERPENTINE_LAYOUT
  //#warning "Strip is serpentine layout"
  if ( x & 0x01) {
    // Odd cols run backwards
    uint8_t reverseY = (HEIGHT - 1) - y;
    i = (x * HEIGHT) + reverseY;
  } else {
    // Even cols run forwards
    i = (x * HEIGHT) + y;
  }
#else
  //#warning "Strip is sequential layout"
  i = (y * WIDTH) + x;
#endif

  return i;
}




/**
   Visual format: stacks[x][y] to LED[i]
   XY function with vertical strips (original below is horizontal strips)
   FEED LOW LEFT (upside down inverse of sajXY)
   9 Rows: 0..8 ;  14 Cols: 0..13
       (top)
     ____  __... output...  (in = input lower left)
     |  |  |  (row #8)
     v  ^  v  (data direction)
 in__|  |__|  (row #0)
     0  1  2  (col#  0..13)
*/
uint16_t sajXYinv( uint8_t x, uint8_t y)
{
  uint16_t i;

#if  MATRIX_SERPENTINE_LAYOUT
  //#warning "Strip is serpentine layout, inverted"
  if ( x & 0x01) {
    // Odd cols run forwards
    i = (x * HEIGHT) + y;
  } else {
    // Even cols run backwards
    uint8_t reverseY = (HEIGHT - 1) - y;
    i = (x * HEIGHT) + reverseY;
  }
#else
  //#warning "Strip is sequential layout"
  i = (y * WIDTH) + x;
#endif

  return i;
}




//
// Mark's xy coordinate mapping code.  See the XYMatrix for more information on it.
// Scott: updated to use preprocessor #if to avoid needless extra code and branching.
//
uint16_t XY( uint8_t x, uint8_t y)
{
  uint16_t i;

#if  MATRIX_SERPENTINE_LAYOUT
  //#warning "Strip is serpentine layout"
  if ( y & 0x01) {
    // Odd rows run backwards
    uint8_t reverseX = (WIDTH - 1) - x;
    i = (y * WIDTH) + reverseX;
  } else {
    // Even rows run forwards
    i = (y * WIDTH) + x;
  }
#else
  //#warning "Strip is sequential layout"
  i = (y * WIDTH) + x;
#endif

  return i;
}


