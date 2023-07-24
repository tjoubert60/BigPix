// MegaPix18.cpp 
//
// 1. Translates a serie of 24bits BMP into one MPX animation file
// --> output is a C source or an MPX binary
// --> Images tempo is given as argument
//
// T. JOUBERT
// v0.1   03 Juil. 2023     ASCII and binaire
// v1.1   15 Juil. 2023     Animations
// v1.2   23 Juil. 2023     Tempo
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

#define VERSION "v1.2  2023-07-23"

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
	unsigned char baseTempo = 30;

	FILE* fp;
	char infilename[100];
	char outfilename[100];
	int bmpnumber = 0;
	int fileindex = 0;
	char filenum[10];

	int coline = 0;
	int imgTempo;

	if (argc < 4) {
		goto syntaxe;
	}
	try
	{
		if (argc > 4)                   // out to .c file
			asciiOut = true;

		allColors[nbColors++] = { 0,0,0 };          // Black
		allColors[nbColors++] = { 255, 255, 255 };  // White

		baseTempo = (unsigned char)atoi(argv[3]);   // Tempo

		strcpy(outfilename, argv[1]);				// prepare output
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
		while (fileindex < atoi(argv[2]))  // open each BMP file from args
		{
			strcpy(infilename, argv[1]);
			_itoa(fileindex + 1, filenum, 10);
			strcat(infilename, filenum);
			strcat(infilename, ".bmp");
			printf("reading %s for colors\n", infilename);
			bmp.ReadFromFile(infilename);   // must be a 16x32 BMP image

			mapCol[fileindex] = (unsigned char*)malloc(bmp.TellHeight() * bmp.TellWidth());  // allocate index table
			idmap = 0;      // re-init mapCol index

			///////////////// collect colors and do the images maps /////////////
			for (int j = 0; j < bmp.TellHeight(); j++)
			{
				for (int i = 0; i < bmp.TellWidth(); i++)
				{
					pix = bmp.GetPixel(i, j);   // input pixel
					pixRGB.R = pix.Red;
					pixRGB.G = pix.Green;
					pixRGB.B = pix.Blue;

					// check existing color
					int idcolPx = -1;
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
					if (-1 == idcolPx)     // add color
					{
						allColors[nbColors] = pixRGB;
						//printf("%d -  %3u, %3u, %3u \n",nbColors, pixRGB.R, pixRGB.G, pixRGB.B);
						idcolPx = nbColors;
						nbColors++;
					}
					(mapCol[fileindex])[idmap++] = idcolPx + 0x20;
					//printf("%02x, ", idcolPx + 0xEE);
				}
			}
			printf("---> %d colors \n", nbColors);
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
				fprintf(fp, "%02X, %02X, %02X, ", allColors[i].R, allColors[i].G, allColors[i].B);
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
			if (coline >= 24)		// line lenght in ASCII file
			{
				coline = 0;
				if (asciiOut)
				{
					fprintf(fp, "\n");
				}
			}
		}
		if (asciiOut)				// line feed in ASCCI file
		{
			fprintf(fp, "\n");
		}

		fileindex = 0;              // Re-init to process 

		///////////////// write the images maps in output file ///////////////////
		printf("do the RLE part\n");
		while (fileindex < atoi(argv[2]))
		{
			if (baseTempo == 0)
			{
				printf("Tempo for image %d ([1-255] unit=10ms) ? ", fileindex + 1);
				scanf("%d", &imgTempo);
			}
			else
				imgTempo = baseTempo;

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
			idmap = 0;
			colCour = 0;
			nbRepet = 0;

			// RLE compression of the image color map
			for (int j = 0; j < bmp.TellHeight(); j++)
			{
				for (int i = 0; i < bmp.TellWidth(); i++)
				{

					if (colCour == 0)
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
					else
					{
						if (mapCol[fileindex][idmap] == colCour)  // same index
						{
							nbRepet++;
						}
						else
						{
							if (nbRepet > 0)
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
							if (asciiOut)
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
					idmap++;
				}
				// Write RLE
				if (nbRepet > 0)
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
				// lines in the source file
				if (asciiOut)
				{
					fprintf(fp, "\n");
				}
			}
			// End of image = 0x00
			if (asciiOut)
			{
				if (fileindex + 1 < atoi(argv[2]))
					fprintf(fp, "0x00,\n");
				else
					fprintf(fp, "0x00 };\n");
			}
			else
			{
				buffer[idb++] = 0;
			}
			totalBytes++;
			free(mapCol[fileindex]);   // clean Heap
			fileindex++;
		}
	}
	catch (std::exception& e)
	{
		printf("Exception handler\n");
	}

	if (asciiOut)					// write array size in source file
	{
		fseek(fp, 0, SEEK_SET);
		fprintf(fp, "char %s[%d] = {", argv[1], totalBytes + 1);
	}
	else							// write buffer in binary file
	{
		//printf("totalBytes+1 = %d   idb = %d\n", totalBytes + 1, idb);
		fwrite(buffer, totalBytes, 1, fp); 
	}
	printf("\n+------------------------------------+\n");
	printf("| %4d bytes in %20s |\n", totalBytes, outfilename);
	printf("+------------------------------------+\n");

finito:
	fclose(fp);
	return 0;

syntaxe:
	printf("Version %s\n\n", VERSION);
	printf("Syntaxe %s bmp_name_prefix number_of_bmp tempo [ascii]\n", argv[0]);
	printf("    ex: %s aa 3 30\n", argv[0]);
	printf("    Will export aa1.bmp aa2.bmp aa3.bmp in aa.mpx with tempo 30\n");
	printf("    ex: %s bb 2 20 a\n", argv[0]);
	printf("    Will export bb1.bmp bb2.bmp in aa.c with tempo 20\n");
}
