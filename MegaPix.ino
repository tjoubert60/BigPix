/*
  ---------------------------------- license ------------------------------------
  Copyright (C) 2023 Thierry JOUBERT.  All Rights Reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy of
  this software and associated documentation files (the "Software"), to deal in
  the Software without restriction, including without limitation the rights to
  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
  the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  -------------------------------------------------------------------------------
*/

/*
   MegaPix ESP32 firmware, listen HTTP request or UDP packet & draw pixels

   2023-06-06  v0.1  T. JOUBERT  Donald, 16 color palette
   2023-06-25  v1.0  T. JOUBERT  Donald & friends 225 color palette
   2023-07-04  v1.1  T. JOUBERT  ESP32 LED_PIN = 16
   2023-07-04  v1.2  T. JOUBERT  UDP server
   2023-07-15  v1.3  T. JOUBERT  Animated images
   2023-07-21  v1.4  T. JOUBERT  Animations tempo
   2023-07-21  v1.5  T. JOUBERT  UDP animation
   2023-07-24  v1.6  T. JOUBERT  Overcome UDP MTU
   ================================================================

    This code follows the general structure of the Arduino code:
        setup() --> all initializations
        loop()  --> periodic call that defines the current state
        
    In the setup() function the firmware opens a WiFi Access Point with the
    SSID "MegaPix" The IP address of this Access Point is 10.1.1.1.
    A WEB server is open on http://10.1.1.1:80 a single HTML page is
    sent to the browser, it allows you to choose a graphic animation.
    A UDP server is open on 10.1.1.1:2023 to directly receive an image
    in MPX binary format. The UDP processing callback is initialized as
    a lambda expression in the setup function.
    
    In the loop() function the firmware checks if an HTTP request has been sent
    to read and analyze it. Then and systematically the firmware displays an
    animated sequence out of seven in the current version:
      0 - Animate a color line (default)
      1 - Heart beat
      2 - Palette
      3 - Donald
      4 - Mickey
      5 - TJO
      6 - Vermeer
      7 - UDP motif
      
    All of the above patterns are in MPX format:
    The MPX Header:
        data[0] --> Nb of colors - excluding pal[0]=Black and pal[1]=White
        data[1] --> Nb of images (max 10)
    The color palette
        The palette starts at data[2]
        From data[2] to data[3*data[0] + 2]: RGB components of the palette
        The final palette will start with Back & White not present in the MPX
    For each image:
        The first image starts at data[3*data[0] + 3]
        Each image follows the final byte of the previous one which is 0x00.
        First byte       --> image display tempo (10ms units, max 255)
        Subsequent bytes --> RLE encoded image
        
    RLE Code:
        Color codes are on one byte from 0x20 to 0xFF, code 0x20 corresponds
        to BLACK and 0x21 to WHITE, so 0x22 is the beginning of the color
        palette in the MPX. There are 223 colors maximum in a MPX, plus B&W which
        makes 225 colors maximum in an image.
        Each pixel contains its color code, when several pixels have the same
        color, the number of repetitions is given after the color code. Since
        each line is encoded separately, the maximum repetition number is 1F
        (31 repeated pixels). So a black line is coded with two bytes:
        0x20, 0x1F,
        
    ---- CONTENT OF AN MPX DATA PACKET ----

         <--- 8 bits --->
        +----------------+  \
        |  NB COLORS     |   |  HEADER
        +----------------+   |
        |  NB IMAGES     |  /
        +----------------+  \
        |  R COLOR-2     |   |
        +----------------+   |
        |  G COLOR-2     |   |  PALETTE AFTER B&W
        +----------------+   |
        |  B COLOR-2     |   |
        +----------------+   |
              ......        /
        +----------------+  \
        |  TEMPO  IMG-1  |   |
        +----------------+   |
        |  BYTE 0 IMG-1  |   |
        +----------------+   |
        |  BYTE 1 IMG-1  |   |  IMAGE 1
        +----------------+   |
              ......         |
        +----------------+   |
        |  0x00          |  /
        +----------------+  \
        |  TEMPO  IMG-2  |   |
        +----------------+   |
        |  BYTE 0 IMG-2  |   |
        +----------------+   |  IMAGE 2
        |  BYTE 1 IMG-2  |   |
        +----------------+   |
              ......         |
        +----------------+   |
        |  0x00          |  /
        +----------------+  \
              ......         |  IMAGE 3
        +----------------+   |
        |  0x00          |  /
        +----------------+  \
              ......         |  OTHER IMAGES
        +----------------+   |
        |  0x00          |  /
        +----------------+  

    The next and prev buttons allow you to change the scrolling line (useful for
    testing the matrix during LED assembly). They also allow you to go up/down the
    global brightness, to control consumption. The basic brightness is 1, it will 
    go to a maximum of 3.
 
*/

#define Version   "MegaPix-v1.6 (c)TJO 2023"

#include <WiFi.h>            // comment for ESP8266
//#include <ESP8266WiFi.h>   // uncomment for ESP8266
#include <FastLED.h>
#include <AsyncUDP.h>
#include "motifsMPX.h"

#define LED_PIN       16
#define NUM_LEDS      512
#define INITSEQUENCE  0
#define MAX_INTENSITY 3
#define TEMPO_UNIT_MS 10

/* --- MegaPix access values --- */
const char *ssid = "MegaPix";
IPAddress local_IP(10,1,1,1);
IPAddress gateway(10,1,1,0);
IPAddress subnet(255,255,255,0);

WiFiServer server(80);  // HTTP server
AsyncUDP udp;           // UDP receiver
CRGB leds[NUM_LEDS];    // LED matrix

//
// MPX Display structures
//
char udpmotif[2300];    // UDP receiver structure
unsigned char palCol[680]; // current palette, saves stack

int sequence  = 0;      // current sequence
int randomSeq = 0;      // random mode ON/OFF
int intensity = 1;      // LED level
int animLine  = 0;      // first line to animate
int cR, cG, cB;         // current color for line & text
int stepMotif = 1;      // animation step number
int imgdone   = 0;      // animation state automaton
int tempoAnim;          // current image temporisation
int startSeq;           // sequence start time
int startUDP = 0;       // sequence start time
int offsetUDP = 0;      // UDP multi packet because MTU=1470
int UDPfirstSz = 0;     // first UDP packet size
//
//  initialize LED, HTTP and UDP
//
void setup()
{
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS); // init LED object
  Serial.begin(115200);
  Serial.println(Version);
  Serial.println();

  for (int i = 0; i< 256; i++)            // clear matrix
    leds[i] = CRGB(0,0,0);
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  if (WiFi.softAP(ssid))                  // set WiFi SSID
  { cR = 0; cG = 200; cB = 0; }           // green line = WiFi OK
  else
  { cR = 200; cG = 0; cB = 0; }           // red line = WiFi NOK
  sequence = INITSEQUENCE;                // initial display sequence
  server.begin();
  Serial.println("HTTP server started");
  
  memcpy(udpmotif,perle,sizeof(perle));   // init UDP zone
  //dumpMem(udpmotif, sizeof(perle));
  
  if(udp.listen(2023))                    // Listen UDP
  {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.softAPIP());

    udp.onPacket([](AsyncUDPPacket packet)
    {
        if (millis() - startUDP > 1000)   // first packet 1470 bytes
        { 
          Serial.print("First UDP Packet, length= ");
          startUDP = millis();
          offsetUDP = 0;
          UDPfirstSz = packet.length();
        }
        else                              // second packet comes shortly
        {
          Serial.print("Second UDP Packet, length= ");
          startUDP = 0;
          offsetUDP = UDPfirstSz;
        }
        Serial.println(packet.length());

        memcpy(udpmotif+offsetUDP,packet.data(),packet.length());  // copy to local data
        udpmotif[offsetUDP+packet.length()] = 0;
        //dumpMem(udpmotif, packet.length()+1);
        sequence = 7;                                    // set as current sequence
        imgdone = 0;
        randomSeq = 0;
    });
  }
}

//
//  Play current animation while listening to HTTP requests
//
void loop()
{
  int requestDone = 0;
  WiFiClient client = server.available(); // listen for incoming clients

  if (client.available())                 // if you get a client,
  {
    String currentLine = "";              // make a String to hold incoming data from the client
    while (client.connected())            // loop while the client is connected
    {
      if (client.available())             // if there's bytes to read from the client,
      {
        char c = client.read();           // read bytes one-by-one, then
        if (c == '\n')                    // if the byte is a newline character
        {                                 // got two newline characters in a row.
          if (currentLine.length() == 0)  // end of HTTP request, so send a response
          {
              client.print("HTTP/1.1 200 OK\r\n");
              client.print("Content-type:text/html\r\n");
              client.print("\r\n");

              // the content of the HTTP response follows the header:
              client.print("<head><style>\r\n");
              client.print("body {background-color:black;text-decoration:none;}\r\n");
              client.print("h1 {font-size:120px;font-family:Verdana;}\r\n");
              client.print("h2 {font-size:80px;color:white;font-family:Lucida Console;}\r\n");
              client.print("h3 {font-size:80px;color:black;font-family:Lucida Console;}\r\n");
              client.print("</style></head>\r\n");

              client.print("<html><body>\r\n");
              client.print("<table border=\"20\" width=\"100%\" height=\"20%\">\r\n");
              client.print("<tr><td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:#FD4600\" href=\"/B\"><h1>BEAT</a>\r\n");
              client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:#39E721\" href=\"/Wa\"><h2>Palette</a></tr></table>\r\n");

              client.print("<table border=\"20\" width=\"100%\" height=\"20%\" >\r\n");
              client.print("<tr><td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/I\"><h2>next</a>\r\n");
              client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/M\"><h2>prev</a></tr></table>\r\n");

              client.print("<table border=\"20\" width=\"100%\" height=\"20%\" >\r\n");
              client.print("<tr><td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/G\"><h2>Donald</a>\r\n");
              client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/E\"><h2>Mickey</a></tr></table>\r\n");

              client.print("<table border=\"20\" width=\"100%\" height=\"20%\" >\r\n");
              client.print("<tr><td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/Mx\"><h2>Anim</a>\r\n");
              client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/Bp\"><h2>Perle</a></tr></table>\r\n");

              client.print("<table border=\"20\" width=\"100%\" height=\"20%\" style=\"background-color:#7AECDF\">\r\n");
              client.print("<tr><td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:#9A1CD1\" href=\"/X\"><h3>Guest</a>\r\n");
              if (randomSeq == 0)
                client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:#138B1C\" href=\"/R\"><h3>ON</a></tr></table>\r\n");
              else
                client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:#B3162E\" href=\"/R\"><h3>OFF</a></tr></table>\r\n");

              client.print("</body></html>\r\n");
              client.print("\r\n");           // The HTTP response ends with another blank line
            break;                            // break out of loop while (client.connected())
          }
          else                                // if newline, then clear currentLine
          { currentLine = "";
          }
        }   //// END   if (c == '\n')
        else if (c != '\r')                   // anything but carriage return character,
        { currentLine += c;                   // add it to the end of the currentLine
        }

        // Check the client request
        if (requestDone == 0)                         // not found request yet
        {
          if (currentLine.endsWith("GET /B "))        // Heart
          { sequence = 1;
            requestDone = 1;
          }
          else if (currentLine.endsWith("GET /I "))   // next
          { sequence = 0;
            requestDone = 1;
            if (intensity < 3)
              intensity++;
            RandomColor();
            if (++animLine > 15)
              animLine = 0;
          }
          else if (currentLine.endsWith("GET /M "))  // prev
          { sequence = 0;
            requestDone = 1;
            if (intensity > 1)
              intensity--;
            RandomColor();
            if (--animLine < 0)
              animLine = 0;
          }
          else if (currentLine.endsWith("GET /Wa ")) // palette
          { sequence = 2;
            requestDone = 1;
          }
          else if (currentLine.endsWith("GET /G "))  // donald
          { sequence = 3;
            requestDone = 1;
          }
          else if (currentLine.endsWith("GET /E "))  // mickey
          { sequence = 4;
            requestDone = 1;
          }
          else if (currentLine.endsWith("GET /Mx ")) // Animation
          { sequence = 5;
            requestDone = 1;
            stepMotif = 0;
          }
          else if (currentLine.endsWith("GET /Bp ")) // perle
          { sequence = 6;
            requestDone = 1;
          }
          else if (currentLine.endsWith("GET /X "))  // UDP guest
          { sequence = 7;
            requestDone = 1;
            stepMotif = 0;
          }
           
          if (requestDone == 1)     // prepare display
          { imgdone = 0;
            randomSeq = 0;
          }
        }
      }  //// END if (client.available())
    }  //// END while (client.connected())

    client.stop();                               // close the connection
    Serial.print("sequence : ");
    Serial.println(sequence);
  } //// END if (client.available())
  /// END OF HTTP REQUEST  ////////////////////////////////////////

  switch (sequence)
  {
  case 0:                           // line
    AnimateLine( animLine, 10);
    break;

   case 1:                          // Beat
    AnimateMPX(heart);
    break;

  case 2:                           // palette
    AnimateMPX(palette);
    break;

  case 3:                           // donald
    DisplayMPX(donald);
    break;

  case 4:                           // mickey
    DisplayMPX(mickey);
    break;

  case 5:                           // Animation ///////////////////
    AnimateMPX(tjo);
    break;

  case 6:                           // Perle
    DisplayMPX(perle);
    break;

  case 7:                           // UDP guest
    AnimateMPX(udpmotif);
    break;
  }
}

//
// Set a single pixel with color & intensity
//
void DoPixel(int li, int co, char red, char green, char blue, char intensite)
{
int idpix;

  if (li%2 == 0)   // even line
  { idpix = co + li*32;
  }
  else             // odd line reversed
  { idpix = (31 - co) + li*32;
  }

  switch(intensite)
  {
  case 0:
    leds[idpix] = CRGB ( 0, 0, 0);
    break;

  case 1:
    leds[idpix] = CRGB ( red/5, green/5,  blue/5);
    break;

  case 2:
    leds[idpix] = CRGB ( red/3, green/3,  blue/3);
    break;

  case 3:
    leds[idpix] = CRGB ( red, green,  blue);
    break;

  default:
    leds[idpix] = CRGB ( red/2, green/2,  blue/2);
    break;
  }
}

//
// Set a single Pixel to black
//
void ClearPixel(int li, int co)
{
  int idpix = co + li*32;
  leds[idpix] =     CRGB ( 0,   0,   0);
}

//
// animate a line
//
void AnimateLine(int line, int dt)
{
  int startPixl = line*32;

  for (int i=0; i< 16; i++)
    for (int j=0; j<32; j++)
      ClearPixel(i,j);

  for (int i=0; i< 32; i++)   // on
  {
    DoPixel(line,i, cR, cG, cB, intensity);
    delay(dt);
    FastLED.show();
  }

  for (int i=0; i< 32; i++)   // off
  {
    ClearPixel(line, 31-i);
    delay(dt);
    FastLED.show();
  }
}

//
// create a random color
//
void RandomColor()
{
  int fort;
  int faible;
  int type = random(0,3);
  switch (type)
  {
  case 0:
    cR = random(0,256);
    cG = random(0,256);
    cB = random(0,256);
    break;

  case 1:
    faible = random(0,3);
    switch (faible)
    {
    case 0:
      cR = random(0,192);
      cG = random(128,256);
      cB = random(128,256);
      break;
    case 1:
      cR = random(128,256);
      cG = random(0,192);
      cB = random(128,256);
      break;
    case 2:
      cR = random(128,256);
      cG = random(128,256);
      cB = random(0,192);
      break;
    }
    break;

  case 2:
    fort = random(0,3);
    switch (fort)
    {
    case 0:
      cR = random(0,192);
      cG = random(0,192);
      cB = random(128,256);
      break;
    case 1:
      cR = random(0,192);
      cG = random(128,256);
      cB = random(0,192);
      break;
    case 2:
      cR = random(128,256);
      cG = random(0,192);
      cB = random(0,192);
      break;
    }
    break;
  }
}

//
// MPX image with 224 color palette and animation
//
void DrawMPX(char*  motif, int animidx)
{
int lipix = 0;
int copix = 0;
int idcolor;
char data;
int imgidx;
int imagenb;
int currimage = 0;

  // palette initialization
  palCol[0] = 0;     // pal0 = Black  0xEE
  palCol[1] = 0;
  palCol[2] = 0;
  palCol[3] = 255;   // pal1 = White  0xFF
  palCol[4] = 255;
  palCol[5] = 255;

  // byte 0 is number of colors after B&W
  imgidx = (motif[0]*3) + 2;            // index of the first image byte
  imagenb  = motif[1];                  // number of images in animation

  for (int i=2; i < imgidx; i++)        // copy palette bytes to palCol array
     palCol[i+4] = motif[i];

  while (currimage < animidx%imagenb)   // go to requested animidx image
  {
    while ((data = motif[imgidx]) != 0) // jumps over images
    { imgidx++;
    }
    currimage++;
    imgidx++;
  }

  //Serial.print("   currimage: ");
  //Serial.println(currimage);
  tempoAnim = motif[imgidx];            // first image byte is tempo information
  imgidx++;
  
  while ((data = motif[imgidx]) != 0)   // read pixels data
  {
    if (data >= 0x20)     // color code
    {
      idcolor = data - 0x20;
      DoPixel(lipix,copix, palCol[idcolor*3], palCol[idcolor*3 + 1], palCol[idcolor*3 + 2], intensity);
      copix++;
      if (copix > 31)     // line done
      { copix = 0;
        lipix++;
      }
    }
    else                  // RLE information
    {
      while (data > 0)
      {
        DoPixel(lipix,copix, palCol[idcolor*3], palCol[idcolor*3 + 1], palCol[idcolor*3 + 2], intensity);
        copix++;
        if (copix > 31)  // line done
        { copix = 0;
          lipix++;
        }
        data--;
      }
    }
    imgidx++;
  }
  FastLED.show();
}

//
// Still Image automaton
//
void DisplayMPX(char* motif)
{
  if (imgdone == 0)
  {
    if (intensity <= MAX_INTENSITY)
      DrawMPX(motif, 0);
    else
      DrawPalette(motif);

    imgdone = 1;
  }
}

//
// Animation automaton
//
void AnimateMPX(char* motif)
{
  if (imgdone == 0)
  {
    if (intensity <= MAX_INTENSITY)
      DrawMPX(motif, stepMotif++);
    else
      DrawPalette(motif);

    startSeq = millis();
    imgdone = 1;
  }
  else
  {
    if (millis() - startSeq > TEMPO_UNIT_MS*tempoAnim)
      imgdone = 0;                // here we go again
  }
}

//
// Draw first 16 palette enties of MPX image
//
void DrawPalette(char*  motif)
{
int colpix = 0;
int idcolor = 0;

  for (int ipal=0; ipal < 16; ipal++)
  {
    //Serial.print("palette : ");
    //Serial.println(ipal);
    colpix = ipal*2;
    idcolor = ipal*3 + 8;   // skip header and B&W

    DoPixel(0,colpix, motif[idcolor], motif[idcolor + 1], motif[idcolor + 2], 3);
    DoPixel(0,colpix+1, motif[idcolor], motif[idcolor + 1], motif[idcolor + 2], 3);
    DoPixel(1,colpix, motif[idcolor], motif[idcolor + 1], motif[idcolor + 2], 3);
    DoPixel(1,colpix+1, motif[idcolor], motif[idcolor + 1], motif[idcolor + 2], 3);
  }
  FastLED.show();
}


//
// Debug code for UDP buffer
//
void dumpMem(char* buffer, int nbytes)
{
  for (int i=0; i < nbytes; i++)
  {
    if (i%32 == 0)
      Serial.println();
    
    Serial.printf("%02x ",buffer[i]);
  }
  Serial.println();
}

