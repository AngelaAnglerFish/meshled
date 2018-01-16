// Disable interrupts allowing longer led strips without flicker.  Mesh still settles nearly as fast
#define FASTLED_ALLOW_INTERRUPTS 0

#include "FastLED.h"

FASTLED_USING_NAMESPACE

// #if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
// #warning "Requires FastLED 3.1 or later; check github for latest code."
// #endif

#define NUM_STRIPS 1
#define NUM_LEDS_PER_STRIP 60
#define NUM_LEDS NUM_LEDS_PER_STRIP * NUM_STRIPS
CRGB leds[NUM_STRIPS * NUM_LEDS_PER_STRIP];

#define BRIGHTNESS          30
#define FRAMES_PER_SECOND  30

// Mesh Setup
#include "painlessMesh.h"
bool amController = false;  // flag to designate that this CPU is the current controller

// set these to whatever you like - don't use your home info - this is a separate private network
#define   MESH_PREFIX     "LEDMesh01"
#define   MESH_PASSWORD   "foofoofoo"
#define   MESH_PORT       5555

painlessMesh  mesh;


//Forward Declare Led Functions
void nextPattern();
void rainbow();
void rainbowWithGlitter();
void addGlitter( fract8 chanceOfGlitter);
void confetti();
void sinelon();
void bpm();
void juggle();

// LED patterns
// List of patterns to cycle through.  Each is defined as a separate function below.

typedef void (*SimplePattern)();
typedef SimplePattern SimplePatternList[];
typedef struct { SimplePattern mPattern;  uint16_t mTime; } PatternAndTime;
typedef PatternAndTime PatternAndTimeList[];

// These times are in seconds, but could be changed to milliseconds if desired;
// there's some discussion further below.

const PatternAndTimeList gPlaylist = {
  { rainbow,                 5 },
  { confetti,                10 },
  { juggle,                  10 },
  { bpm,                     5 }
};




//SimplePatternList gPatterns = { rainbow, confetti, juggle, bpm };

uint8_t gCurrentTrackNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
bool gRestartTimerFlag = false;

//Forward Declare mesh functions

// this gets called when the designated controller sends a command to start a new animation
// init any animation specific vars for the new mode, and reset the timer vars
void receivedCallback( uint32_t from, String &msg );

void newConnectionCallback(uint32_t nodeId);

// this gets called when a node is added or removed from the mesh, so set the controller to the node with the lowest chip id
void changedConnectionCallback();

void nodeTimeAdjustedCallback(int32_t offset);

// sort the given list of nodes
void sortNodeList(SimpleList<uint32_t> &nodes);

// check the timer and do one animation step if needed
void stepAnimation();

// send a broadcast message to all the nodes specifying the new animation mode for all of them
void sendMessage(int display_step);




// #Custom rainbow
void my_fill_rainbow( struct CRGB * pFirstLED, int numToFill,
                  uint8_t initialhue,
                  uint8_t deltahue,
                bool dir=0 )
{
    CHSV hsv;
    hsv.hue = initialhue;
    hsv.val = 255;
    hsv.sat = 240;
    if(dir==1){
      for( int i = 0; i < numToFill; i++) {
          pFirstLED[i] = hsv;
          hsv.hue += deltahue;
      }
    }else{
      for( int i = numToFill-1; i >= 0; i--) {
          pFirstLED[i] = hsv;
          hsv.hue += deltahue;
      }
    }
}

void setup() {
  delay(3000); // 3 second delay for recovery

  // tell FastLED there's 60 NEOPIXEL leds on pin 3, starting at index 0 in the led array
  FastLED.addLeds<NEOPIXEL, D3>(leds, 0, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);

  // // tell FastLED there's 60 NEOPIXEL leds on pin 11, starting at index 60 in the led array
  // FastLED.addLeds<NEOPIXEL, 4>(leds, NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //
  // // tell FastLED there's 60 NEOPIXEL leds on pin 5, starting at index 120 in the led array
  // FastLED.addLeds<NEOPIXEL, 5>(leds, 2 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //
  // // tell FastLED there's 60 NEOPIXEL leds on pin 6, starting at index 180 in the led array
  // FastLED.addLeds<NEOPIXEL, 6>(leds, 3 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //
  // // tell FastLED there's 60 NEOPIXEL leds on pin 6, starting at index 180 in the led array
  // FastLED.addLeds<NEOPIXEL, 23>(leds, 4 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //
  // // tell FastLED there's 60 NEOPIXEL leds on pin 6, starting at index 180 in the led array
  // FastLED.addLeds<NEOPIXEL, 22>(leds, 5 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //
  // // tell FastLED there's 60 NEOPIXEL leds on pin 6, starting at index 180 in the led array
  // FastLED.addLeds<NEOPIXEL, 21>(leds, 6 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //
  // // tell FastLED there's 60 NEOPIXEL leds on pin 6, starting at index 180 in the led array
  // FastLED.addLeds<NEOPIXEL, 20>(leds, 7 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);

  // FastLED.addLeds<LED_TYPE,6,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  //mesh settings
  Serial.begin(115200);
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages

  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  sendMessage(gCurrentTrackNumber);

  // make this one the controller if there are no others on the mesh
  if (mesh.getNodeList().size() == 0) amController = true;

}


void loop()
{
  mesh.update();
  // Call the current pattern function once, updating the 'leds' array
  gPlaylist[gCurrentTrackNumber].mPattern();

  // send the 'leds' array out to the actual LED strip
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND);

  // do some periodic updates
  EVERY_N_MILLISECONDS( 1 ) { gHue = gHue+3; } // slowly cycle the "base color" through the rainbow
  // EVERY_N_SECONDS( 20 ) { nextPattern(); gHue=0;} // change patterns periodically
  {
    EVERY_N_SECONDS_I(patternTimer,gPlaylist[gCurrentTrackNumber].mTime) {
      nextPattern();
      patternTimer.setPeriod( gPlaylist[gCurrentTrackNumber].mTime);
    }

    // Here we reset the timers to make sure the nodes stay in step
    // This is done after reciving the command to switch
    if( gRestartTimerFlag ) {
      // Set the playback duration for this patter to it's correct time
      patternTimer.setPeriod( gPlaylist[gCurrentTrackNumber].mTime);
      // Reset the pattern timer so that we start marking time from right now
      patternTimer.reset();
      // Finally, clear the gRestartPlaylistFlag flag
      gRestartTimerFlag = false;
    }
  }
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern()
{
  //Only the master node can change modes.
  if (amController) {

    // add one to the current pattern number
    gCurrentTrackNumber = gCurrentTrackNumber + 1;

    //reset the hue to match between nodes
    gHue=0;

    // reset the timers to match between nodes.
    gRestartTimerFlag = true;

    //reseed random number generator
    random16_set_seed(666);

    // If we've come to the end of the playlist, we can either
    // automatically restart it at the beginning, or just stay at the end.
    if( gCurrentTrackNumber == ARRAY_SIZE( gPlaylist) ) {
      gCurrentTrackNumber = 0;
    }

    //Send the mode change to the other non master nodes.
     sendMessage(gCurrentTrackNumber);
  }
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  my_fill_rainbow( leds, NUM_LEDS, gHue, 7, 0);
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter)
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

//mesh functions.
// this gets called when the designated controller sends a command to start a new animation
// init any animation specific vars for the new mode, and reset the timer vars
void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("Setting gCurrentPatternNumber to %s. Received from %u\n", msg.c_str(), from);

  // get the new display mode
  gCurrentTrackNumber = msg.toInt();
  gHue=0;

  //reset timers
  gRestartTimerFlag = true;

  //reseed random number generator
  random16_set_seed(666);

} // receivedCallback


void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
} // newConnectionCallback


// this gets called when a node is added or removed from the mesh, so set the controller to the node with the lowest chip id
void changedConnectionCallback() {
  SimpleList<uint32_t> nodes;
  uint32_t myNodeID = mesh.getNodeId();
  uint32_t lowestNodeID = myNodeID;

  Serial.printf("Changed connections %s\n",mesh.subConnectionJson().c_str());

  nodes = mesh.getNodeList();
  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  for (SimpleList<uint32_t>::iterator node = nodes.begin(); node != nodes.end(); ++node) {
    Serial.printf(" %u", *node);
    if (*node < lowestNodeID) lowestNodeID = *node;
  }
  Serial.println();

  if (lowestNodeID == myNodeID) {
    Serial.printf("Node %u: I am the controller now", myNodeID);
    Serial.println();
    amController = true;
    // restart the current animation - to chatty - remove - better to wait for next animation
    //sendMessage(display_mode);
    //display_step = 0;
    //ul_PreviousMillis = 0UL; // make sure animation triggers on first step
  } else {
    Serial.printf("Node %u is the controller now", lowestNodeID);
    Serial.println();
    amController = false;
  }
} // changedConnectionCallback

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
} // changedConnectionCallback

// sort the given list of nodes
void sortNodeList(SimpleList<uint32_t> &nodes) {
  SimpleList<uint32_t> nodes_sorted;
  SimpleList<uint32_t>::iterator smallest_node;

  // sort the node list
  while (nodes.size() > 0) {
    // find the smallest one
    smallest_node = nodes.begin();
    for (SimpleList<uint32_t>::iterator node = nodes.begin(); node != nodes.end(); ++node) {
      if (*node < *smallest_node) smallest_node = node;
    }

    // add it to the sorted list and remove it from the old list
    nodes_sorted.push_back(*smallest_node);
    nodes.erase(smallest_node);
  } // while

  // copy the sorted list back into the now empty nodes list
  for (SimpleList<uint32_t>::iterator node = nodes_sorted.begin(); node != nodes_sorted.end(); ++node) nodes.push_back(*node);
} // sortNodeList


// // check the timer and do one animation step if needed
// void stepAnimation() {
//   // if the animation delay time has past, run another animation step
//   unsigned long ul_CurrentMillis = millis();
//   if (ul_CurrentMillis - ul_PreviousMillis > ul_Interval) {
//     ul_PreviousMillis = millis();
//     switch (display_mode) {
//       case 1: // run the single color up the bar one LED at a time
//         leds[display_step % NUM_LEDS] = color1;
//         FastLED.show();
//       break;
//
//       case 2: // rainbow colors
//         leds[display_step % NUM_LEDS] = Wheel(display_step % 255);
//         // we could also set all the pixels at once
//         //for(uint16_t i=0; i< strip.numPixels(); i++) {
//         //  strip.setPixelColor(i, Wheel((i+display_step) & 255));
//         //  //strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + display_step) & 255));
//         //}
//         FastLED.show();
//       break;
//
//       case 3: // go from dim to full on one color for all LEDs a few times
//         for(uint16_t i=0; i<NUM_LEDS; i++) {
//           leds[i] = CRGB(0, 0, display_step % 255);
//         }
//         FastLED.show();
//       break;
//
//       case 4: // alternate red and green LEDs
//         if (((display_step + (display_step / NUM_LEDS)) % 2) == 0) {
//           leds[display_step % NUM_LEDS] = CRGB(64, 0, 0);
//         } else {
//           leds[display_step % NUM_LEDS] = CRGB(0, 64, 0);
//         }
//         FastLED.show();
//       break;
//
//       case 5: // animate a dot moving up all of them
//       {
//         int curr_strip_num = display_step / NUM_LEDS;
//         SimpleList<uint32_t> nodes = mesh.getNodeList();
//         int node_index = 0;
//         uint32_t myNodeID = mesh.getNodeId();
//
//         // add this node to the node list and sort the list
//         nodes.push_back(myNodeID);
//         sortNodeList(nodes);
//
//         // blacken all the leds
//         for(uint16_t i=0; i<NUM_LEDS; i++) leds[i] = CRGB(0, 0, 0);
//
//         // if this is the active strip, set the next LED on
//         SimpleList<uint32_t>::iterator node = nodes.begin();
//         while (node != nodes.end()) {
//           if ((*node == myNodeID) && (node_index == curr_strip_num)) {
//             leds[display_step % NUM_LEDS] = CRGB(255, 255, 255);
//           }
//           node_index++;
//           node++;
//         }
//         FastLED.show();
//       }
//       break;
//
//       case 6: // green bars with a red bar moving around them a few times
//       {
//         SimpleList<uint32_t> nodes = mesh.getNodeList();
//         int curr_strip_num = (display_step / MODE6CLICKSPERCOLOR) % (nodes.size() + 1);
//         int node_index = 0;
//         uint32_t myNodeID = mesh.getNodeId();
//
//         // add this node to the node list and sort the list
//         nodes.push_back(myNodeID);
//         sortNodeList(nodes);
//
//         // set all the leds to green
//         for(uint16_t i=0; i<NUM_LEDS; i++) leds[i] = CRGB(0, 255, 0);
//
//         // if this is the active strip, set the next LED on
//         SimpleList<uint32_t>::iterator node = nodes.begin();
//         while (node != nodes.end()) {
//           if ((*node == myNodeID) && (node_index == curr_strip_num)) {
//             for(uint16_t i=0; i<NUM_LEDS; i++) leds[i] = CRGB(255, 0, 0);
//           }
//           node_index++;
//           node++;
//         }
//         FastLED.show();
//       }
//       break;
//     } // display_mode switch
//
//     display_step++;
//     // a debug led to show which is the current controller
//     //if (amController) leds[0] = CRGB(64, 64, 64);
//   } // timing conditional ul_Interval
// } // stepAnimation


// send a broadcast message to all the nodes specifying the new animation mode for all of them
void sendMessage(int display_step) {
  String msg;
  //msg += mesh.getNodeId();
  msg += String(display_step);
  mesh.sendBroadcast( msg );
} // sendMessage

