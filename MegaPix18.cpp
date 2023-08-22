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

// MegaPix18.cpp 
//
// 1. Translates a serie of 24bits BMP into one MPX animation file
// --> arg#1  bmp file prefix (aa for aa1.bmp, aa2.bmp...)
// --> arg#2  number of bmp files to process
// --> arg#3  common images tempo (if 0, will ask if no args#5) 
// --> arg#4  output file is a C source ('C') or an MPX binary ('M')
// --> [arg#5+] images tempos (arg#3 must be 0, will ask if missing)
//
// T. JOUBERT
// v0.1   03 Jul. 2023     ASCII and binaire
// v1.1   15 Jul. 2023     Animations
// v1.2   23 Jul. 2023     Global tempo
// v1.3   28 Jul. 2023     Refactoring
// v1.4   18 Aug. 2023     Tempo values as args
// 

/*   ----CONTENT OF AN MPX FILE----

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

*/

#define _CRT_SECURE_NO_WARNINGS

#include<stdlib.h>
#include<stdbool.h>
#include<stdio.h>
#include<math.h>
#include<string.h>
#include<ctype.h>
#include<math.h>
#include "..\RGBconvert\EasyBMP.h"
#include<windows.h>

#define VERSION "v1.4  2023-08-18"

//--------------------------------------------------------
// STRUCTS
//--------------------------------------------------------
typedef struct
{
  unsigned char R;
  unsigned char G;
  unsigned char B;
} RGBdec;

//--------------------------------------------------------
// FUNCTION PROTOTYPES
//--------------------------------------------------------
int filter_exception(LPEXCEPTION_POINTERS);

// -----------
// Data
// -----------
#define MAXCOLORS 230
RGBdec allColors[MAXCOLORS];

bool asciiOut = false;            // ASCII or binary output

//---------------------------------------------------------------------------------
// Main
//---------------------------------------------------------------------------------
int main(int argc, char** argv)
{
  BMP bmp;
  RGBdec pixRGB;
  RGBApixel pix;
  unsigned char* mapCol[10];
  int nbFiles = 0;
  int idmap = 0;
  unsigned char colCour;
  int nbRepet;
  // binary output
  unsigned char buffer[2300];
  int  idb = 0;
  ////////////////
  int  nbCol = 0;
  int  nbLin = 0;
  int nbColors = 0;
  int totalBytes = 0;

  FILE* fp;
  char infilename[100];
  char outfilename[100];
  int bmpnumber = 0;
  int fileindex = 0;
  char filenum[10];

  int coline = 0;
  unsigned char baseTempo = 30;
  int imgTempo;

  if (argc < 5)
  {
    goto syntax;
  }
  try
  {
    nbFiles = atoi(argv[2]);        // number of BMP, can't be zero
    if (nbFiles == 0)
        goto syntax;

    if (argv[4][0] == 'C')          // argv[4] --> out to .c file else to .mpx
    {
      asciiOut = true;
    }
    allColors[nbColors++] = { 0,0,0 };          // Black
    allColors[nbColors++] = { 255, 255, 255 };  // White

    baseTempo = (unsigned char)atoi(argv[3]);   // Animation Tempo

    strcpy(outfilename, argv[1]);       // prepare output file
    if (asciiOut)
    {
      strcat(outfilename, ".c");
      fp = fopen(outfilename, "w+");
    }
    else
    {
      strcat(outfilename, ".mpx");
      fp = fopen(outfilename, "wb+");
    }

    //////////////// read each BMP to count colors and do the map ////////////////
    while (fileindex < nbFiles)        // open each BMP file from args
    {
      strcpy(infilename, argv[1]);
      _itoa(fileindex + 1, filenum, 10);
      strcat(infilename, filenum);
      strcat(infilename, ".bmp");
      bmp.ReadFromFile(infilename);     // !!must be a 16x32 BMP image

      if (bmp.TellHeight() != 16 || bmp.TellWidth() != 32) {
          printf("\n!!! %s is not a 16x32 image !!!\n", infilename);
          return 0;
      }

      mapCol[fileindex] = (unsigned char*)malloc(bmp.TellHeight() * bmp.TellWidth());  // allocate color map
      idmap = 0;      // re-init mapCol index

      ///////////////// collect colors and do the color map /////////////
      for (int j = 0; j < bmp.TellHeight(); j++)
      {
        for (int i = 0; i < bmp.TellWidth(); i++)
        {
          pix = bmp.GetPixel(i, j);     // input pixel
          pixRGB.R = pix.Red;
          pixRGB.G = pix.Green;
          pixRGB.B = pix.Blue;

          int idcolPx = -1;             // check existing color
          for (int k = 0; k < nbColors; k++)
          {
            if (allColors[k].B == pixRGB.B &&
              allColors[k].G == pixRGB.G &&
              allColors[k].R == pixRGB.R)
            {
              idcolPx = k;
              break;
            }
          }
          if (-1 == idcolPx)            // unknown color in palette, add it
          {
            allColors[nbColors] = pixRGB;
            //printf("%d -  %3u, %3u, %3u \n",nbColors, pixRGB.R, pixRGB.G, pixRGB.B);
            idcolPx = nbColors;
            nbColors++;
          }
          (mapCol[fileindex])[idmap++] = idcolPx + 0x20;  // image map
          //printf("%02x, ", idcolPx + 0xEE);
        }
      }
      printf("%s ---> %d colors\n", infilename, nbColors);
      fileindex++;
    } // All BMP have been processed

    ///////////////// write the colors palette in output file /////////////// 
    printf("TOTAL %d colors in MPX\n", nbColors - 2);
    if (asciiOut)
    {
      fprintf(fp, "                           \n%3d, ", nbColors - 2);   // nbcolors no B&W
      fprintf(fp, "%3d,\n", atoi(argv[2]));  // nb images
    }
    else
    {
      buffer[idb++] = (unsigned char)nbColors - 2;
      buffer[idb++] = (unsigned char)atoi(argv[2]);
    }
    totalBytes += 2;

    for (int i = 2; i < nbColors; i++)
    {
      if (asciiOut)
      {
        fprintf(fp, "%3u, %3u, %3u, ", allColors[i].R, allColors[i].G, allColors[i].B);
        //printf("%3u, %3u, %3u\n", allColors[i].R, allColors[i].G, allColors[i].B);
      }
      else
      {
        buffer[idb++] = allColors[i].R;
        buffer[idb++] = allColors[i].G;
        buffer[idb++] = allColors[i].B;
        //printf("%02X, %02X, %02X\n", allColors[i].R, allColors[i].G, allColors[i].B);
      }
      totalBytes += 3;
      coline += 3;
      if (coline >= 24)               // line lenght in ASCII file
      {
        coline = 0;
        if (asciiOut)
        {
          fprintf(fp, "\n");
        }
      }
    }
    if (asciiOut)                     // line feed in ASCCI file
    {
      fprintf(fp, "\n");
    }

    ///////////////// write the images maps in output file ///////////////////
    fileindex = 0;                     // Re-init to process 

    while (fileindex < nbFiles)
    {
      if (baseTempo == 0)
      {
          if (argc >= 6 + fileindex)   // get tempo from arguments
              imgTempo = atoi(argv[5 + fileindex]);
          else {                        // or ask for it
              printf("Tempo for image %d ([1-255] unit=10ms) ? ", fileindex + 1);
              scanf("%d", &imgTempo);
          }      
      }
      else                              // invariant tempo from args
      {
        imgTempo = baseTempo;
      }
      printf("image %d - Tempo %d\n", fileindex + 1, imgTempo);
      if (asciiOut)
      {
        fprintf(fp, " %3d,\n", imgTempo);  // given tempo
        //printf("0x%02X, ", mapCol[fileindex][idmap]);
      }
      else
      {
        buffer[idb++] = (unsigned char)imgTempo;
      }
      totalBytes++;

      // RLE compression of the image color map
      idmap = 0;
      colCour = 0;
      nbRepet = 0;
      for (int j = 0; j < bmp.TellHeight(); j++)
      {
        for (int i = 0; i < bmp.TellWidth(); i++)
        {

          if (colCour == 0)           // initialization
          {
            if (asciiOut)
            {
              fprintf(fp, "0x%02X, ", mapCol[fileindex][idmap]);
              //printf("0x%02X, ", mapCol[fileindex][idmap]);
            }
            else
            {
              buffer[idb++] = mapCol[fileindex][idmap];
            }
            totalBytes++;
            colCour = mapCol[fileindex][idmap];
            nbRepet = 0;
          }
          else                        // inside map
          {
            if (mapCol[fileindex][idmap] == colCour)  // same index
            {
              nbRepet++;
            }
            else                      // new color index
            {
              if (nbRepet > 0)        // write RLE byte
              {
                if (asciiOut)
                {
                  fprintf(fp, "0x%02X, ", nbRepet);
                }
                else
                {
                  buffer[idb++] = nbRepet;
                }
                totalBytes++;
              }
              if (asciiOut)           // write new color index
              {
                fprintf(fp, "0x%02X, ", mapCol[fileindex][idmap]);
              }
              else
              {
                buffer[idb++] = mapCol[fileindex][idmap];
              }
              totalBytes++;
              colCour = mapCol[fileindex][idmap];
              nbRepet = 0;
            }
          }
          idmap++;                    // next pixel in the mapcolor
        }

        if (nbRepet > 0)              // End of a pixel line, write RLE information 
        {
          if (asciiOut)
          {
            fprintf(fp, "0x%02x, ", nbRepet);
          }
          else
          {
            buffer[idb++] = nbRepet;
          }
          totalBytes++;
          colCour = 0;
        }
        if (asciiOut)                 // newline in the source file
        {
          fprintf(fp, "\n");
        }
      }

      if (asciiOut)                   // End of image = 0x00
      {
        if (fileindex + 1 < atoi(argv[2]))  // check last image
        {
          fprintf(fp, "0x00,\n");
        }
        else
        {
          fprintf(fp, "0x00 };\n");
        }
      }
      else
      {
        buffer[idb++] = 0;
      }
      totalBytes++;
      free(mapCol[fileindex]);        // clean Heap
      fileindex++;
    }

    if (asciiOut)                     // write array size in source file
    {
      fseek(fp, 0, SEEK_SET);
      fprintf(fp, "char %s[%d] = {", argv[1], totalBytes);
    }
    else                              // write buffer to binary file
    {
      fwrite(buffer, totalBytes, 1, fp);
      //printf("totalBytes = %d   idb = %d\n", totalBytes + 1, idb);
    }
    printf("\n+------------------------------------+\n");
    printf("| %4d bytes in %20s |\n", totalBytes, outfilename);
    printf("+------------------------------------+\n");

    fclose(fp);
  }
  catch (std::exception& e)
  {
    printf("!!! %s did hit exception %s !!!\n",argv[0],e.what());
  }
  return 0;

syntax:
  printf("Version %s\n\n", VERSION);
  printf("Syntaxe %s bmp_name_prefix number_of_bmp tempo_or_0 C_or_M [tempo_values]\n", argv[0]);
  printf("    ex: %s aa 3 30 M\n", argv[0]);
  printf("        Will export aa1.bmp aa2.bmp aa3.bmp in aa.mpx with tempo 30\n");
  printf("    ex: %s bb 2 0 C\n", argv[0]);
  printf("        Will export bb1.bmp bb2.bmp in bb.c asking for tempos\n");
  printf("    ex: %s z 2 0 M 10 100\n", argv[0]);
  printf("        Will export z1.bmp z2.bmp in z.mpx with tempos 10 and 100\n");
  return 0;
}
