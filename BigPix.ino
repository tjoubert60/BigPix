/*
  ---------------------------------- license ------------------------------------
  Copyright (C) 2022 Thierry JOUBERT.  All Rights Reserved.

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
   BigPix ESP8266 firmware, listen HTTP request & draw pixels

   2022-08-13  v0.1  T. JOUBERT  robot, beat, eyes
   2022-08-24  v1.0  T. JOUBERT  SoftAP, ghost, HTTP
   2022-08-25  v1.1  T. JOUBERT  Intensity control, code-Z
   2022-08-25  v1.2  T. JOUBERT  Scroll, Random sequence
   2022-08-26  v1.3  T. JOUBERT  Squid, Matrix_V0
   2022-08-26  v1.4  T. JOUBERT  Matrix, emoji
   2022-08-27  v1.5  T. JOUBERT  Fireworks
   2022-09-01  v1.6  T. JOUBERT  BigPix & 8 choices + 2 options
   2022-09-14  v1.7  T. JOUBERT  Sequences are incremental
   2022-09-20  v1.8  T. JOUBERT  Random mode after 7 sec
   2022-09-24  v1.9  T. JOUBERT  HTTP + favicon
   2022-10-07  v1.1  T. JOUBERT  S=Rainbow
   2022-10-16  v2.0  T. JOUBERT  T=text 
   2022-10-22  v2.1  T. JOUBERT  t=text reversed and ON/OFF
    ================================================================

    Ce code suit la structure generale du code Arduino :
        setup() --> toutes les initialisations
        loop()  --> appel en boucle qui definit l'etat courant

    Dans la fonction setup() le firmware ouvre un Access Point WiFi avec le
    SSID "BigPix" L'adresse IP de cet Access Point est 10.1.1.1 et un serveur WEB
    est ouvert sur http://10.1.1.1:80 une unique page HTML est envoyee au
    navigateur, elle permet de choisir une animation graphique.

    Dans la fonction loop() le firmware regarde si une requete HTTP a ete envoyee
    pour la lire et l'analyser. Une page HTML ou la favicon est envoyee, ensuite
    et de maniere systematique le firmware affiche une sequence animee, il y a
    onze sequences dans la version courante:
        Sequence  0 : ligne défilante (initialisation vert=OK, rouge=NOK)
        Sequence  1 : Robot avec la couleur courante
        Sequence  2 : Coeur rouge
        Sequence  3 : BigPix avec une couleur aleatoire
        Sequence  4 : Ghost
        Sequence  5 : Squid avec la couleur courante
        Sequence  6 : Eyes
        Sequence  7 : Matrix
        Sequence  8 : Fireworks
        Sequence  9 : Squares
        Sequence 10 : APERO avec la couleur courante
        Sequence 11 : Texte libre avec une couleur aleatoire
        Sequence 12 : Texte libre a l'envers avec une couleur aleatoire

    La sequence d'origine est la 11 avec un texte qui donne la version courante du
    logiciel. Les sequences 0, 7 et 8 sont calculees au moment de l'affichage. Les
    autres sequences utilisent des motifs definis au debut du code source dans des
    tableaux d'octets (char) :
        Robot --> inv01 et inv02
        Coeur --> hea01 et hea02
        BigPix -> IE22
        Ghost --> gho01 et gho02
        Squid --> sqi01 et sqi02
        Eyes  --> eye01 et eye02
        Squares > sq01, sq02, sq03 et sq04
        APERO --> apero
        Texte --> typo

    Tous les motifs ci-dessus appartiennent a un parmi trois types :
        Type de motif 1 : Monochrome
        Type de motif 2 : Polychrome (8 couleurs dont le noir)
        Type de motif 3 : Motif defilant monochrome
        
    Par ailleurs le tableau typo contient une fonte de caracteres monochrome 

    Les types de motifs sont les suivants :
        inv01 et inv02 --> 1-monochrome
        hea01 et hea02 --> 1-monochrome
        IE22           --> 3-defilant
        gho01 et gho02 --> 2-polychrome
        sqi01 et sqi02 --> 1-monochrome
        eye01 et eye02 --> 2-polychrome
        sq01, sq02, sq03 et sq04 --> 2-polychrome
        apero          --> 3-defilant

    Le format du motif monochrome est un tableau de 88 octets (8x11), chaque octet
    donne l'etat attendu du pixel :
        0 --> eteint
        1 --> une LED allumee
        2 --> deux LED allumees
        3 --> trois LED allumees

    Le format du motif polychrome est un tableau de 112 octets, les 24 premiers
    definissent huit couleurs en mode RGB (3 octets sachant que la couleur zero
    est noire), les 88 octets suivants (8x11) donnent l'état attendu du pixel :
        0       --> eteint
        1 a 7   --> une LED allumee de la couleur donnee
        11 a 17 --> deux LED allumees de la couleur donnee par l'unite
        21 a 27 --> trois LED allumees de la couleur donnee par l'unite

    Le format du motif defilant est un tableau de Nx8 octets, le motif de
    N colonnes va defiler dans les 11 colonnes d'affichage, chaque octet donne
    l'état attendu du pixel :
        0 --> eteint
        1 --> une LED allumee
        2 --> deux LED allumees
        3 --> trois LED allumees

    Pour chaque type de motif on dispose d'une fonction d'affichage dédiee :
        1-monochrome --> DrawMono(motif, R, G, B)
        2-polychrome --> DrawMulti(motif)
        3-Defilant   --> DrawScroll(motif, R, G, B, N)

    Pour chaque sequence calculee ou texte on dispose d'une fonction :
        Ligne        --> AnimateLine(on/off, ligne, tempo)
        Texte        --> DrawText()
        Texte rev.   --> DrawtxeT()
        Matrix       --> DrawMatrix()
        Fireworks    --> DrawFireworks()

    Le choix de la sequence courante est determinee en fonction de l'URL envoyee
    au serveur WEB :
        10.1.1.1/Z  --> ligne defilante
        10.1.1.1/I  --> robot
        10.1.1.1/B  --> coeur
        10.1.1.1/Bp --> BigPix
        10.1.1.1/G  --> ghost
        10.1.1.1/M  --> squid
        10.1.1.1/E  --> eyes
        10.1.1.1/Mx --> Matrix
        10.1.1.1/Wa --> Fireworks
        10.1.1.1/S  --> squares
        10.1.1.1/A  --> APERO
        10.1.1.1/T  --> texte libre
        10.1.1.1/t  --> texte libre renverse
    La page WEB envoyee au navigateur ne propose que 8 des 11 URL, la premiere
    et les quatre dernieres doivent etre saisies a la main dans le navigateur du
    telephone.
    
    Dans le cas du texte libre, la chaine retenue est ce qui suit le 'T' ou 't' 
    jusqu'au premier caractere espace et dans la limite de 99 caracteres. Les
    expressions doivent être saisies avec des '.' a la place des espaces. Seuls
    sont affichés les chiffres, les majuscules et les trois caracteres '!', '+'
    et '-'. Tout autre caractere est traduit en un espace, et les minuscules sont
    traduites en majuscules.    

    Certaines URL sont des commandes ne provoquant pas le changement de sequence:
        10.1.1.1/X  --> Commutation de couleur courante Cyan-Magenta-Jaune
        10.1.1.1/R  --> ON/OFF du mode sequence aleatoire
        10.1.1.1/F  --> Couleur courante aleatoire

    Le code de chaque sequence contient des pauses (delay(XXX)) qui donnent le
    rythme de l'animation. Pour ne pas bloquer la fonction loop() trop longtemps,
    les sequences affichent leurs motifs en plusieurs passes sequentielles gerees
    avec la variable "stepMotif"

*/

#define bpVersion   ".v2-1....."

#define INITSEQUENCE    11      // initialsequence  0=ligne, 11=version, 99=eteint
#define INITRANDOM       1      // initial random, 0=no, 1=yes

#include <ESP8266WiFi.h>
//#include <WiFi.h>
#include <FastLED.h>

#define LED_PIN     4    // MiniD1 pin D2
#define NUM_LEDS    264  // (8x11 matrix) x (3 led)
#define MAXMSG      100
#define MAXTYPO     40

/* --- BigPix access values --- */
const char *ssid = "BigPix";
IPAddress local_IP(10,1,1,1);
IPAddress gateway(10,1,1,0);
IPAddress subnet(255,255,255,0);

WiFiServer server(80);

char bigicon[252] = {
 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00,
 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x02, 0x00,
 0x00, 0x00, 0x90, 0x91, 0x68, 0x36, 0x00, 0x00, 0x00,
 0x15, 0x74, 0x45, 0x58, 0x74, 0x43, 0x72, 0x65, 0x61,
 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x54, 0x69, 0x6D, 0x65,
 0x00, 0x07, 0xE6, 0x09, 0x17, 0x06, 0x09, 0x12, 0x3F,
 0xE0, 0x0A, 0xAB, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49,
 0x4D, 0x45, 0x07, 0xE6, 0x09, 0x18, 0x09, 0x37, 0x35,
 0x5D, 0x11, 0x10, 0xA6, 0x00, 0x00, 0x00, 0x09, 0x70,
 0x48, 0x59, 0x73, 0x00, 0x00, 0x0B, 0x12, 0x00, 0x00,
 0x0B, 0x12, 0x01, 0xD2, 0xDD, 0x7E, 0xFC, 0x00, 0x00,
 0x00, 0x7A, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0xB5,
 0x92, 0x41, 0x0E, 0xC0, 0x20, 0x08, 0x04, 0xC1, 0xF0,
 0xFF, 0x17, 0x93, 0x50, 0xB4, 0xA5, 0x28, 0xD8, 0x36,
 0x1C, 0xCA, 0x41, 0x8D, 0xAE, 0xEB, 0xAC, 0x8A, 0x22,
 0x02, 0x95, 0x6A, 0x25, 0xB5, 0x16, 0xF5, 0x06, 0xF1,
 0x5B, 0x68, 0x20, 0x58, 0x00, 0x1A, 0x7B, 0x50, 0x33,
 0xD0, 0xEB, 0x09, 0x3C, 0xC1, 0xB0, 0x8A, 0x61, 0x74,
 0xD7, 0xDC, 0xBD, 0x73, 0xBE, 0x89, 0xD5, 0x6E, 0x13,
 0x5A, 0x3D, 0xFC, 0x4C, 0x1B, 0xD0, 0x12, 0x7A, 0xE7,
 0x94, 0xA5, 0x27, 0x08, 0x65, 0xFB, 0xC0, 0xBD, 0x30,
 0x3B, 0x52, 0xB0, 0x37, 0x05, 0x43, 0xAC, 0xF2, 0xC3,
 0xB5, 0x8C, 0xDB, 0x01, 0x52, 0x00, 0x8F, 0x26, 0xD3,
 0x72, 0x0E, 0x13, 0x02, 0xD8, 0xFC, 0xF3, 0xDB, 0x71,
 0xFA, 0x9A, 0xF8, 0xFB, 0x6F, 0x3D, 0x00, 0x8D, 0xEB,
 0x2F, 0x23, 0x94, 0xB3, 0xFC, 0x7D, 0x00, 0x00, 0x00,
 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82 };

char inv01[88] = { 0,0,1,0,0,0,0,0,1,0,0,  // robot
                   0,0,0,1,0,0,0,1,0,0,0,
                   0,0,1,1,1,1,1,1,1,0,0,
                   0,1,1,0,1,1,1,0,1,1,0,
                   1,1,1,1,1,1,1,1,1,1,1,
                   1,0,1,1,1,1,1,1,1,0,1,
                   1,0,1,0,0,0,0,0,1,0,1,
                   0,0,0,1,1,0,1,1,0,0,0 };

char inv02[88] = { 0,0,1,0,0,0,0,0,1,0,0,
                   1,0,0,1,0,0,0,1,0,0,1,
                   1,0,1,1,1,1,1,1,1,0,1,
                   1,1,1,0,1,1,1,0,1,1,1,
                   1,1,1,1,1,1,1,1,1,1,1,
                   0,1,1,1,1,1,1,1,1,1,0,
                   0,0,0,1,0,0,0,1,0,0,0,
                   0,0,1,0,0,0,0,0,1,0,0 };

char sqi01[88] = { 0,0,0,0,1,1,1,0,0,0,0,   // Squid
                   0,0,0,1,1,1,1,1,0,0,0,
                   0,0,1,1,1,1,1,1,1,0,0,
                   0,1,1,0,1,1,1,0,1,1,0,
                   0,1,1,1,1,1,1,1,1,1,0,
                   0,0,1,0,0,0,0,0,1,0,0,
                   0,1,0,0,0,0,0,0,0,1,0,
                   0,0,1,0,0,0,0,0,1,0,0 };

char sqi02[88] = { 0,0,0,0,1,1,1,0,0,0,0,
                   0,0,0,1,1,1,1,1,0,0,0,
                   0,0,1,1,1,1,1,1,1,0,0,
                   0,1,1,0,1,1,1,0,1,1,0,
                   0,1,1,1,1,1,1,1,1,1,0,
                   0,0,0,1,0,0,0,1,0,0,0,
                   0,0,1,0,1,0,1,0,1,0,0,
                   0,1,0,1,0,1,0,1,0,1,0 };

char hea01[88] = { 0,0,0,1,1,0,1,1,0,0,0,   // Heart beat
                   0,0,1,1,1,1,1,1,1,0,0,
                   0,1,1,1,1,1,1,1,1,1,0,
                   0,1,1,1,1,1,1,1,1,1,0,
                   0,0,1,1,1,1,1,1,1,0,0,
                   0,0,0,1,1,1,1,1,0,0,0,
                   0,0,0,0,1,1,1,0,0,0,0,
                   0,0,0,0,0,1,0,0,0,0,0 };

char hea02[88] = { 0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,1,1,0,1,1,0,0,0,
                   0,0,1,2,2,1,2,2,1,0,0,
                   0,0,1,2,3,3,3,2,1,0,0,
                   0,0,0,1,2,3,2,1,0,0,0,
                   0,0,0,0,1,2,1,0,0,0,0,
                   0,0,0,0,0,1,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0 };


char typIdx[MAXTYPO];

char typWdt[MAXTYPO] = {
  1,3,2,3,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,3,1,3,4,3,5,4,3,3,3,3,3,3,3,3,5,3,3,3,2 };
//! + - 0 1 2 3 4 5 6 7 8 9 A B C D E F G H I J K L M N O P Q R S T U V W X Y Z SP

char typo[600] = {
  1,      // !
  1,
  1,
  0,
  1,

  0,0,0,  // +
  0,1,0,
  1,1,1,
  0,1,0,
  0,0,0,
  
  0,0,    // -
  0,0,
  1,1,
  0,0,
  0,0,
  
  0,1,0,  // 0
  1,0,1,
  1,0,1,
  1,0,1,
  0,1,0,
  
  0,1,    // 1
  1,1,
  0,1,
  0,1,
  0,1,
  
  1,1,0,  // 2
  0,0,1,
  0,1,0,
  1,0,0,
  1,1,1,

  1,1,0,  // 3
  0,0,1,
  0,1,0,
  0,0,1,
  1,1,1,

  0,0,1,  // 4
  0,1,0,
  1,0,1,
  1,1,1,
  0,0,1,
  
  1,1,1,  // 5
  1,0,0,
  1,1,0,
  0,0,1,
  1,1,1,

  0,1,1,  // 6
  1,0,0,
  1,1,1,
  1,0,1,
  1,1,1,
  
  1,1,1,  // 7
  0,0,1,
  0,1,0,
  1,0,0,
  1,0,0,
  
  1,1,1,  // 8
  1,0,1,
  0,1,0,
  1,0,1,
  1,1,1,
  
  1,1,1,  // 9
  1,0,1,
  0,1,1,
  0,0,1,
  1,1,1,
  
  0,1,0,  // A
  1,0,1,
  1,1,1,
  1,0,1,
  1,0,1,

  1,1,1,  // B
  1,0,1,
  1,1,1,
  1,0,1,
  1,1,1,
  
  1,1,1,  // C
  1,0,0,
  1,0,0,
  1,0,0,
  1,1,1,

  1,1,0,  // D
  1,0,1,
  1,0,1,
  1,0,1,
  1,1,1,

  1,1,1,  // E
  1,0,0,
  1,1,0,
  1,0,0,
  1,1,1,
  
  1,1,1,  // F
  1,0,0,
  1,1,0,
  1,0,0,
  1,0,0,

  1,1,1,1,  // G
  1,0,0,0,
  1,0,1,1,
  1,0,0,1,
  1,1,1,1,
  
  1,0,1,  // H
  1,0,1,
  1,1,1,
  1,0,1,
  1,0,1,
  
  1,      // I
  1,
  1,
  1,
  1,

  0,0,1,  // J
  0,0,1,
  0,0,1,
  1,0,1,
  0,1,1,
  
  1,0,0,1, //K
  1,0,1,0,
  1,1,0,0,
  1,0,1,0,
  1,0,0,1,

  1,0,0,  // L
  1,0,0,
  1,0,0,
  1,0,0,
  1,1,1,

  1,0,0,0,1, // M
  1,1,0,1,1,
  1,0,1,0,1,
  1,0,0,0,1,
  1,0,0,0,1,
  
  1,0,0,1,  // N
  1,1,0,1,
  1,0,1,1,
  1,0,0,1,
  1,0,0,1,

  1,1,1,  // O
  1,0,1,
  1,0,1,
  1,0,1,
  1,1,1,

  1,1,1,  // P
  1,0,1,
  1,1,1,
  1,0,0,
  1,0,0,
  
  1,1,1,  // Q
  1,0,1,
  1,0,1,
  1,1,1,
  1,1,1,
  
  1,1,1,  // R
  1,0,1,
  1,1,1,
  1,1,0,
  1,0,1,
  
  1,1,1,  // S
  1,0,0,
  1,1,1,
  0,0,1,
  1,1,1,
  
  1,1,1,  // T
  0,1,0,
  0,1,0,
  0,1,0,
  0,1,0,
  
  1,0,1,  // U
  1,0,1,
  1,0,1,
  1,0,1,
  1,1,1,
  
  1,0,1,  // V
  1,0,1,
  1,0,1,
  1,0,1,
  0,1,0,
  
  1,0,0,0,1,  // W
  1,0,0,0,1,
  1,0,1,0,1,
  1,1,0,1,1,
  1,0,0,0,1,
  
  1,0,1,  // X
  1,0,1,
  0,1,0,
  1,0,1,
  1,0,1,

  1,0,1,  // Y
  1,0,1,
  0,1,0,
  0,1,0,
  0,1,0,
  
  1,1,1,  // Z
  0,0,1,
  0,1,0,
  1,0,0,
  1,1,1,

  0,0,    // space  MAXTYPO-1
  0,0,
  0,0,
  0,0,
  0,0 };
  
/*
                   1 2 3 4 5 6 7 8 910 1 2 3 4 5 6 7 8 920 1 2 3 4 5 6 7 8 930 1 2 3
                   ---------------------+++++++++++++++++++++++xxxxxxxxxxxxxxxxxxxxx
*/
char apero[264]= { 0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
                   0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,
                   0,1,1,1,1,1,1,1,0,0,0,1,0,0,1,1,1,0,1,1,0,1,1,1,0,1,1,1,0,1,0,1,0,
                   0,0,1,1,1,1,1,0,0,0,1,0,1,0,1,0,1,0,1,0,0,1,0,1,0,1,0,1,0,1,0,1,0,
                   0,0,0,1,1,1,0,0,0,0,1,1,1,0,1,1,1,0,1,1,0,1,1,1,0,1,0,1,0,1,0,1,0,
                   0,0,0,0,1,0,0,0,0,0,1,0,1,0,1,0,0,0,1,0,0,1,1,0,0,1,0,1,0,1,0,1,0,
                   0,0,0,0,1,0,0,0,0,0,1,0,1,0,1,0,0,0,1,0,0,1,0,1,0,1,0,1,0,0,0,0,0,
                   0,0,1,1,1,1,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,1,0,1,0,1,1,1,0,1,0,1,0
                   };

char ie22[264]= {  0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0, // BigPix
                   0,0,0,0,0,0,0,1,0,0,1,0,1,0,1,0,0,0,0,0,0,1,0,1,0,1,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,1,1,1,0,1,1,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,1,0,0,1,0,1,0,1,0,1,1,1,0,0,1,1,0,0,1,0,1,0,1,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,1,0,1,0,1,0,1,0,1,0,0,1,0,0,0,1,0,0,1,0,0,0,0,
                   0,0,0,1,0,0,0,0,0,0,1,1,1,0,1,0,1,1,1,0,0,1,0,0,0,1,0,1,0,1,0,0,0,
                   0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
                   };

char eye01[112] = {   0,  0,  0,   // col0   black
                    130,130,130,   // col1   light grey
                     10, 10,130,   // col2   light blue
                     60, 60,130,   // col3   blue
                     70, 70, 70,   // col4   grey
                      0,  0,  0,   // col5
                      0,  0,  0,   // col6
                      0,  0,  0,   // col7
                    0,0,1,1,0,0,0,0,1,1,0,
                    0,1,1,1,1,0,0,1,1,1,1,
                    1,1,1,1,1,0,1,1,1,1,1,
                    3,2,3,1,1,0,3,2,3,1,1,
                    2,4,2,1,1,0,2,4,2,1,1,
                    2,2,2,1,1,0,2,2,2,1,1,
                    0,2,3,1,1,0,0,2,3,1,1,
                    0,0,1,1,0,0,0,0,1,1,0 };

char eye02[112] = {   0,  0,  0,   // col0   black
                    130,130,130,   // col1   light grey
                     10, 10,130,   // col2   light blue
                     60, 60,130,   // col3   blue
                     70, 70, 70,   // col4   grey
                      0,  0,  0,   // col5
                      0,  0,  0,   // col6
                      0,  0,  0,   // col7
                    0,0,1,1,0,0,0,0,1,1,0,
                    0,1,1,1,1,0,0,1,1,1,1,
                    1,1,1,1,1,0,1,1,1,1,1,
                    1,1,3,2,3,0,1,1,3,2,3,
                    1,1,2,4,2,0,1,1,2,4,2,
                    1,1,2,2,2,0,1,1,2,2,2,
                    0,1,3,2,3,0,0,1,3,2,3,
                    0,0,1,1,0,0,0,0,1,1,0 };

char gho01[112] = {   0,  0,  0,   // col0   black
                    130,130,130,   // col1   light grey
                    130, 10,130,   // col2   magenta
                      0,  0,200,   // col3   blue
                     80, 10, 80,   // col4   light magenta
                      0,  0,  0,   // col5
                      0,  0,  0,   // col6
                      0,  0,  0,   // col7
                    0,0,0,0,2,2,0,0,0,0,0,
                    0,0,2,2,2,2,2,2,0,0,0,
                    0,2,1,3,2,2,1,3,2,0,0,
                    2,2,1,1,2,2,1,1,2,2,0,
                    2,2,2,2,2,2,2,2,2,2,0,
                    2,2,2,2,2,2,2,2,2,2,0,
                    2,2,2,2,2,2,2,2,2,2,0,
                    0,2,2,0,0,2,2,0,0,2,0 };

char gho02[112] = {   0,  0,  0,   // col0   black
                    130,130,130,   // col1   light grey
                    130, 10,130,   // col2   magenta
                      0,  0,200,   // col3   blue
                     80, 10, 80,   // col4   light magenta
                      0,  0,  0,   // col5
                      0,  0,  0,   // col6
                      0,  0,  0,   // col7
                    0,0,0,0,0,2,2,0,0,0,0,
                    0,0,0,2,2,2,2,2,2,0,0,
                    0,0,2,3,1,2,2,3,1,2,0,
                    0,2,2,1,1,2,2,1,1,2,2,
                    0,2,2,2,2,2,2,2,2,2,2,
                    0,2,2,2,2,2,2,2,2,2,2,
                    0,2,2,2,2,2,2,2,2,2,2,
                    0,2,0,0,2,2,0,0,2,2,0 };

char sq01[112] = {   0,  0,  0,   // col0   black
                    215, 10, 90,   // col1   red
                    132, 10,215,   // col2   maj
                     10, 90,215,   // col3   blue
                     10,215,132,   // col4   cyan
                     90,215, 10,   // col5   green
                    215,132, 10,   // col6   Yell
                    90,  90, 90,   // col7   White
                    11,12,13,14,15,16,11,12,13,14,15,
                    11,12,13,14,15,16,11,12,13,14,15,
                    11,12,13,14,15,16,11,12,13,14,15,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0 };

char sq02[112] = {   0,  0,  0,   // col0   black
                    215, 10, 90,   // col1   red
                    132, 10,215,   // col2   maj
                     10, 90,215,   // col3   blue
                     10,215,132,   // col4   cyan
                     90,215, 10,   // col5   green
                    215,132, 10,   // col6   Yell
                    90,  90, 90,   // col7   White
                    0,0,0,0,0,0,0,0,0,0,0,
                    16,11,12,13,14,15,16,11,12,13,14,
                    16,11,12,13,14,15,16,11,12,13,14,
                    16,11,12,13,14,15,16,11,12,13,14,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0 };

char sq03[112] = {   0,  0,  0,   // col0   black
                    215, 10, 90,   // col1   red
                    132, 10,215,   // col2   maj
                     10, 90,215,   // col3   blue
                     10,215,132,   // col4   cyan
                     90,215, 10,   // col5   green
                    215,132, 10,   // col6   Yell
                    90,  90, 90,   // col7   White
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    15,16,11,12,13,14,15,16,11,12,13,
                    15,16,11,12,13,14,15,16,11,12,13,
                    15,16,11,12,13,14,15,16,11,12,13,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0 };

char sq04[112] = {   0,  0,  0,   // col0   black
                    215, 10, 90,   // col1   red
                    132, 10,215,   // col2   maj
                     10, 90,215,   // col3   blue
                     10,215,132,   // col4   cyan
                     90,215, 10,   // col5   green
                    215,132, 10,   // col6   Yell
                    90,  90, 90,   // col7   White
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    14,15,16,11,12,13,14,15,16,11,12,
                    14,15,16,11,12,13,14,15,16,11,12,
                    14,15,16,11,12,13,14,15,16,11,12,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0 };

char sq05[112] = {   0,  0,  0,   // col0   black
                    215, 10, 90,   // col1   red
                    132, 10,215,   // col2   maj
                     10, 90,215,   // col3   blue
                     10,215,132,   // col4   cyan
                     90,215, 10,   // col5   green
                    215,132, 10,   // col6   Yell
                    90,  90, 90,   // col7   White
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    13,14,15,16,11,12,13,14,15,16,11,
                    13,14,15,16,11,12,13,14,15,16,11,
                    13,14,15,16,11,12,13,14,15,16,11,
                    0,0,0,0,0,0,0,0,0,0,0 };
                    
char sq06[112] = {   0,  0,  0,   // col0   black
                    215, 10, 90,   // col1   red
                    132, 10,215,   // col2   maj
                     10, 90,215,   // col3   blue
                     10,215,132,   // col4   cyan
                     90,215, 10,   // col5   green
                    215,132, 10,   // col6   Yell
                    90,  90, 90,   // col7   White
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,
                    12,13,14,15,16,11,12,13,14,15,16,
                    12,13,14,15,16,11,12,13,14,15,16,
                    12,13,14,15,16,11,12,13,14,15,16 };
                    
                
char mxFB1[88] = { 1,1,1,1,1,1,1,1,1,1,1,      // Matrix Frame Buffer flip
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0 };

char mxFB2[88] = { 0,0,0,0,0,0,0,0,0,0,0,      // Matrix Frame Buffer flop
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0 };

char fiR[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
char fiG[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
char fiB[11] = { 0,0,0,0,0,0,0,0,0,0,0 };

char msg[MAXMSG] = bpVersion;                // text buffer

CRGB leds[NUM_LEDS];    // LED array
int favicon = 0;        // GET /favicon.ico
int sequence = 0;       // current sequence
int randomSeq = 0;      // random mode ON/OFF
int startSeq;           // sequence start time
int scrollH = 0;        // horizontal scroll step
int cR, cG, cB;         // current color for mono & text
int  getMsg = 0;        // flag msg in request
int  msgIdx = 0;        // current msg character number
int  typoIndex = 0;     // current msg Typo index
int  typoCol = 0;       // current Typo column
int  doSpace = 0;       // inter-character
int stepMotif = 1;      // Animation step

/*
    Initialize Access Point, font, colors, Web server, animation
*/
void setup()
{
  delay(1000);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  
  typIdx[0] = 0;
  for (int i = 1; i < MAXTYPO; i++)       // initialise typo indexes
  {
    typIdx[i] = typIdx[i-1] + typWdt[i-1];
  }
  Serial.begin(115200);
  Serial.println();

  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  if (WiFi.softAP(ssid))                  // set WiFi SSID
  { cR = 0; cG = 200; cB = 0; }           // green line = WiFi OK
  else                                    
  { cR = 200; cG = 0; cB = 0; }           // red line = WiFi NOK
  sequence = INITSEQUENCE;                // initial display sequence
  randomSeq = INITRANDOM;                 // initial random status
  server.begin();
  Serial.println("HTTP server started");
  startSeq = millis();                    // line for 7 seconds
}

/*
    General Automaton
*/
void loop()
{
int requestDone = 0;
WiFiClient client = server.available();   // listen for incoming clients

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
            if (favicon == 1)             // favicon request
            {
              favicon = 0;
              client.print("HTTP/1.1 200 OK\r\n");
              client.print("Content-type:image/png\r\n");
              client.print("\r\n");
              for (int i=0; i< 252; i++)
                client.write(bigicon[i]);
            }
            else                          // command request
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
              client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:#39E721\" href=\"/Wa\"><h2>Firework</a></tr></table>\r\n");

              client.print("<table border=\"20\" width=\"100%\" height=\"20%\" >\r\n");
              client.print("<tr><td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/I\"><h2>Robot</a>\r\n");
              client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/M\"><h2>Squid</a></tr></table>\r\n");

              client.print("<table border=\"20\" width=\"100%\" height=\"20%\" >\r\n");
              client.print("<tr><td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/G\"><h2>Ghost</a>\r\n");
              client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/E\"><h2>Eyes</a></tr></table>\r\n");

              client.print("<table border=\"20\" width=\"100%\" height=\"20%\" >\r\n");
              client.print("<tr><td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/Mx\"><h2>Matrix</a>\r\n");
              client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:white\" href=\"/Bp\"><h2>BigPix</a></tr></table>\r\n");

              client.print("<table border=\"20\" width=\"100%\" height=\"20%\" style=\"background-color:#7AECDF\">\r\n");
              client.print("<tr><td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:#9A1CD1\" href=\"/X\"><h3>C-M-Y</a>\r\n");
              if (randomSeq == 0)
                client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:#138B1C\" href=\"/R\"><h3>ON</a></tr></table>\r\n");
              else
                client.print("<td width=\"50%\" align=\"center\"><a style=\"text-decoration:none;color:#B3162E\" href=\"/R\"><h3>OFF</a></tr></table>\r\n");

              client.print("</body></html>\r\n");
              client.print("\r\n");           // The HTTP response ends with another blank line
            }
            break;                            // break out of loop while (client.connected())
          }
          else                                // if newline, then clear currentLine
          {
            currentLine = "";
          }
        }   //// END   if (c == '\n')
        else if (c != '\r')                   // anything but carriage return character,
        {
          currentLine += c;                   // add it to the end of the currentLine
          if (getMsg == 1)
          {
            if (msgIdx < (MAXMSG-1) && c != ' ')
              msg[msgIdx++] = c;              // collect text message - stop at space
            else
            {
              msg[msgIdx] = '\0';             // zero string
              getMsg = 0;
              msgIdx = 0;
              initTypo();                     // ready
              Serial.print("MSG = ");
              Serial.println(msg);
            }
          }
        }

        // Check the client request
        if (requestDone == 0)                         // not found request yet
        {
          if (currentLine.endsWith("GET /favicon.ico"))   // favicon
          { favicon = 1; requestDone = 1; }
          else if (currentLine.endsWith("GET /Z "))   // line
          { sequence = 0; requestDone = 1; randomSeq = 0; }
          else if (currentLine.endsWith("GET /I "))   // robot
          { sequence = 1; requestDone = 1; randomSeq = 0; }
          else if (currentLine.endsWith("GET /B "))   // beat
          { sequence = 2; requestDone = 1; randomSeq = 0; }
          else if (currentLine.endsWith("GET /Bp "))  // BigPix
          {
            sequence = 3;
            scrollH = 0;
            requestDone = 1; 
            randomSeq = 0;
            clearFB();
          }
          else if (currentLine.endsWith("GET /G "))   // ghost
          { sequence = 4; requestDone = 1; randomSeq = 0; }
          else if (currentLine.endsWith("GET /M "))   // squid
          { sequence = 5; requestDone = 1; randomSeq = 0; }
          else if (currentLine.endsWith("GET /E "))   // eyes
          { sequence = 6; requestDone = 1; randomSeq = 0; }
          else if (currentLine.endsWith("GET /Mx "))  // Matrix
          { sequence = 7; requestDone = 1; randomSeq = 0; }
          else if (currentLine.endsWith("GET /Wa "))  // Fireworks
          {
            sequence = 8;
            startSeq = millis();
            requestDone = 1;
            randomSeq = 0;
          }
          else if (currentLine.endsWith("GET /S "))   // Square
          { sequence = 9; requestDone = 1; randomSeq = 0;}
          else if (currentLine.endsWith("GET /A "))   // apero
          {
            sequence = 10;
            scrollH = 0;
            requestDone = 1;
            randomSeq = 0;
            clearFB();
          }
          else if (currentLine.endsWith("GET /T"))    // text
          { 
            sequence = 11;
            getMsg = 1;
            msgIdx = 0;
            requestDone = 1;
            randomSeq = 0;
            clearFB();
          }
          else if (currentLine.endsWith("GET /t"))    // text reversed
          { 
            sequence = 12;
            getMsg = 1;
            msgIdx = 0;
            requestDone = 1;
            randomSeq = 0;
            clearFB();
          }
          else if (currentLine.endsWith("GET /X "))   // CMY not a sequence
          {
            if (cR == 0) // cyan
            { cR = 200; cG =   0; cB = 200; }  // goto Magenta
            else if (cG == 0) // Magenta
            { cR = 200; cG = 200; cB =   0; }  // goto Yello
            else if (cB == 0) // Yellow
            { cR =   0; cG = 200; cB = 200; }  // goto Cyan
            else
            { cR =   0; cG = 200; cB = 200; }  // start with Cyan
            requestDone = 1;
          }
          else if (currentLine.endsWith("GET /R "))  // random sequence ON/OFF
          {
            if (randomSeq == 0)               // clicked goRandom
            {
              randomSeq = 1;
              startSeq = millis();
              sequence = random(0,11);
            }
            else                              // clicked goChoose
            { 
              randomSeq = 0;
              
              sequence = 99;
            }
            requestDone = 1;
          }
          else if (currentLine.endsWith("GET /F "))  // randomColor not a sequence
          { RandomColor(); requestDone = 1; }
        }
      }  //// END if (client.available())
    }  //// END while (client.connected())

    client.stop();                               // close the connection
    Serial.print("sequence : ");
    Serial.println(sequence);
  } //// END if (client.available())
  /// END OF HTTP REQUEST  ////////////////////////////////////////

  if (randomSeq == 1)                           // start a random sequence
  {
    if ( (millis() - startSeq) > 8000 )         // 8 sec sequences
    {
      sequence = random(1,10);                  // last = squares
      startSeq = millis();
    }
  }

  switch (sequence)
  {
  case 0:                           // line
    switch(stepMotif)
    {
      case 1:
        AnimateLine(true, 0, 50);
        break;
      case 2:
        AnimateLine(false, 0, 50);
        break;
    }
    if (++stepMotif > 2)
      stepMotif = 1;
    break;
    
  case 1:
    switch(stepMotif)
    {
      case 1:
        DrawMono(inv01, cR, cG, cB); // robot base color
        delay(400);
        break;
      case 2:
      default:
        DrawMono(inv02, cR, cG, cB);
        delay(400);
        break;
    }
    if (++stepMotif > 2)
      stepMotif = 1;
    break;

  case 2:
    switch(stepMotif)
    {
      case 1:
        DrawMono(hea01, 130, 0, 0);  // red beat
        delay(800);
        break;
      case 2:
      default:
        DrawMono(hea02, 130, 0, 0);
        delay(200);
        break;
    }
    if (++stepMotif > 2)
      stepMotif = 1;
    break;

  case 3:
    DrawScroll(ie22, cR, cG, cB, 33);   // BigPix
    delay(100);
    break;

  case 4:
    switch(stepMotif)
    {
      case 1:
        DrawMulti(gho01);            // Ghost
        delay(400);
        break;
      case 2:
      default:
        DrawMulti(gho02);
        delay(400);
        break;
    }
    if (++stepMotif > 2)
      stepMotif = 1;
    break;

  case 5:
    switch(stepMotif)
    {
      case 1:
        DrawMono(sqi01, cR, cG, cB); // squid
        delay(200);
        break;
      case 2:
      default:
        DrawMono(sqi02, cR, cG, cB);
        delay(200);
        break;
    }
    if (++stepMotif > 2)
      stepMotif = 1;
    break;

  case 6:
    switch(stepMotif)
    {
      case 1:
        DrawMulti(eye01);            // Eyes
        delay(600);
        break;
      case 2:
      default:
        DrawMulti(eye02);
        delay(600);
        break;
    }
    if (++stepMotif > 2)
      stepMotif = 1;
    break;

  case 7:
    DrawMatrix();                    // Matrix screen
    delay(100);
    break;

  case 8:
    DrawFireworks();                 // Fireworks
    delay(120);
    break;

  case 9:
    switch(stepMotif)                // Square
    {
      case 1:
        DrawMulti(sq01);        
        delay(100);
        break;
      case 2:
        DrawMulti(sq02);
        delay(100);
        break;
      case 3:
        DrawMulti(sq03);
        delay(100);
        break;
      case 4:
        DrawMulti(sq04);
        delay(100);
        break;
      case 5:
        DrawMulti(sq05);
        delay(100);
        break;
      case 6:
        DrawMulti(sq06);
        delay(100);
        break;
      case 7:
        DrawMulti(sq05);
        delay(100);
        break;
      case 8:
        DrawMulti(sq04);
        delay(100);
        break;
      case 9:
        DrawMulti(sq03);
        delay(100);
        break;
      case 10:
      default:
        DrawMulti(sq02);
        delay(100);
        break;
    }
    if (++stepMotif > 10)
      stepMotif = 1;
    break;

  case 10: 
    DrawScroll(apero, cR, cG, cB, 33);   // Apero
    delay(100);
    break;
    
  case 11:
    DrawText(cR, cG, cB);            // text
    delay(100);
    break;
    
  case 12:
    DrawtxeT(cR, cG, cB);            // text reversed
    delay(100);
    break;

  default:                           // screen OFF
    clearFB();
    drawFB(0,0,0);
    delay(500);
    break;
  }
}

/*
 * Random color
 */
void RandomColor()
{
  cR = random(0, 26) * 10;
  cG = random(0, 26) * 10;
  cB = random(0, 26) * 10;
  
  if (cR < 140 && cG < 140 && cB < 140)
  {
    switch(random(1,4))
    {
      case 1:
        cR = 250;
        break;
      case 2:
        cG = 250;
        break;
      case 3:
      default:
        cB = 250;
        break;
    }
  }
}

/*
 * Set a single pixel with color & intensity
 */
void DoPixel(int pixel, int red, int green, int blue, int intensite)
{
int idpix = 3*pixel;

  switch(intensite)
  {
  case 0:
    leds[idpix] =     CRGB ( 0, 0, 0);
    leds[idpix + 1] = CRGB ( 0, 0, 0);
    leds[idpix + 2] = CRGB ( 0, 0, 0);
    break;

  case 1:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( red, green,  blue);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;

  case 2:
    leds[idpix] =     CRGB ( red, green,  blue);
    leds[idpix + 1] = CRGB ( 0,   0,   0);
    leds[idpix + 2] = CRGB ( red, green,  blue);
    break;

  case 3:
    leds[idpix] =     CRGB ( red, green,  blue);
    leds[idpix + 1] = CRGB ( red, green,  blue);
    leds[idpix + 2] = CRGB ( red, green,  blue);
    break;

  default:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( red, green,  blue);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;
  }
}

/*
 * set a single Pixel to black
 */
void ClearPixel(int pixel)
{
int idpix = 3*pixel;

  leds[idpix] =     CRGB ( 0,   0,   0);
  leds[idpix + 1] = CRGB ( 0,   0,   0);
  leds[idpix + 2] = CRGB ( 0,   0,   0);
}

/*
 * animate a line
 */
void AnimateLine(bool turnon, int line, int dt)
{
int startPixl = line*11;

  if (turnon)
  {
    for (int i=0; i< 88; i++)
      ClearPixel(i);

    for (int i=0; i< 11; i++)   // on
    {
      DoPixel(startPixl + i,cR, cG, cB, 1);
      delay(dt);
      FastLED.show();
    }
  }
  else
  {
    for (int i=0; i< 11; i++)   // off
    {
      ClearPixel(startPixl + i);
      delay(dt);
      FastLED.show();
    }
  }
}

/*
 *  monochrome shape char[8, 11] = intensity map
 */
void DrawMono(char* motif, int aR, int aG, int aB)
{
int pixel;
int intensite;

  for (int lin=0; lin < 8; lin++)
  {
    for (int col = 0; col < 11; col++)
    {
      pixel = (lin*11 + col);
      if (lin%2 == 0)    // even line
      {
        intensite = lin*11 + col;
      }
      else               // odd line reversed
      {
        intensite = lin*11 +10 - col;
      }
      DoPixel(pixel, aR, aG, aB, motif[intensite]);
    }
  }
  FastLED.show();
}

/*
*   calculate Typo index from ASCII code
*/
char TypoFromAscii(char asc)
{
  if (asc < '0')       // special chars 
  {
    if (asc == '!')
      return 0;
    else if (asc == '+')
      return 1;
    else if (asc == '-')
      return 2;
    else
      return (MAXTYPO-1);// > space
  }
  else if (asc <= '9')   // numbers
    return (asc - '0' + 3);
  else if (asc < 'A')
    return (MAXTYPO-1);
  else if (asc <= 'Z')   // uppercase letters
    return (asc - 52);
  else if (asc < 'a')
    return (MAXTYPO-1);
  else if (asc <= 'z')   // lowercase letters -> uppercase
    return (asc - 84);
  else
    return (MAXTYPO-1);
}

/*
*   initialize a new letter to display
*/
void initTypo()
{
  typoIndex = TypoFromAscii(msg[msgIdx]);  // get typo rank
  typoCol = 0;                             // start width
  doSpace = 1;
}

/*
*   Clear FB matrix
*/
void clearFB()
{
   for (int lin=0; lin < 8; lin++)      // draw FB matrix
    for (int col = 0; col < 11; col++)
      mxFB1[lin*11 + col] = 0;
}

/*
*   Left shift FB matrix
*/
void LshiftFB(int first, int last)
{
  for (int lin=first; lin < last; lin++)      // Left shift FB matrix
    for (int col = 0; col < 10; col++)
      mxFB1[lin*11 + col] = mxFB1[lin*11 + col + 1];
}

/*
*   Right shift FB matrix
*/
void RshiftFB(int first, int last)
{
  for (int lin=first; lin < last; lin++)      // Right shift FB matrix
    for (int col = 10; col > 0; col--)
      mxFB1[lin*11 + col] = mxFB1[lin*11 + col - 1];
}

/*
*   draw FB matrix
*/
void drawFB(int aR, int aG, int aB)
{
int pixel;
int intensite;

  for (int lin=0; lin < 8; lin++)
  {
    for (int col = 0; col < 11; col++)
    {
      pixel = lin*11 + col;
      if (lin%2 == 0)    // even line
      {
        intensite = lin*11 + col;
      }
      else               // odd line reversed
      {
        intensite = lin*11 + 10 - col;
      }
      DoPixel(pixel,aR, aG, aB, mxFB1[intensite]);
    }
  }
  FastLED.show();
}

/*
 *  monochrome scrollable motif char[8, largeur] = intensity map
 */
void DrawScroll(char* motif, int aR, int aG, int aB, int largeur)
{
  LshiftFB(0,8);

  for (int lin = 0; lin < 8; lin++)    // draw last column
    mxFB1[lin*11 + 10] = motif[lin*largeur + scrollH];

  if (++scrollH > largeur-1)           // restart motif
  {
    scrollH = 0;
    RandomColor();
  }
  drawFB(aR, aG, aB);
}

/*
 *  monochrome scrollable text, up to 99 characters
 */
void DrawText(int aR, int aG, int aB)
{
  // current letter
  if (doSpace == 0 && ++typoCol > typWdt[typoIndex])   // end of current typo
  {
    msgIdx++;   // next letter in msg
    if (msg[msgIdx] == '\0')  
    {
      msgIdx = 0;     // rewind
      RandomColor();
    }
    initTypo();
  }

  LshiftFB(2, 7);            // Left shift FB matrix for text

  if (doSpace == 1)
  {
    for (int lin = 2; lin < 7; lin++)  // clear last column
    {
      mxFB1[lin*11 + 10] = 0;
    }
    doSpace = 0;
  }
  else
  {
    for (int lin = 2; lin < 7; lin++)  // draw last column
      mxFB1[lin*11 + 10] = typo[typIdx[typoIndex]*5 + (lin-2)*typWdt[typoIndex] + typoCol - 1];
  }
  
  drawFB(aR, aG, aB);
}

/*
 *  monochrome scrollable text reversed, up to 99 characters
 */
void DrawtxeT(int aR, int aG, int aB)
{
  // current letter
  if (doSpace == 0 && ++typoCol > typWdt[typoIndex])   // end of current typo
  {
    msgIdx++;   // next letter in msg
    if (msg[msgIdx] == '\0')  
    {
      msgIdx = 0;     // rewind
      RandomColor();
    }
    initTypo();
  }

  RshiftFB(2, 7);            // Right shift FB matrix for text

  if (doSpace == 1)
  {
    for (int lin = 2; lin < 7; lin++)  // clear first column
    {
      mxFB1[lin*11] = 0;
    }
    doSpace = 0;
  }
  else
  {
    for (int lin = 2; lin < 7; lin++)  // draw first column
      mxFB1[lin*11] = typo[typIdx[typoIndex]*5 + (lin-2)*typWdt[typoIndex] + typoCol - 1];
  }
  
  drawFB(aR, aG, aB);
}

/*
 * colored shape char[8, 3] = RGB + char[8, 11] = color & intensity
 */
void DrawMulti(char*  motif)
{
int idpix;
int idcolor;

  for (int lin=0; lin < 8; lin++)
  {
    for (int col = 0; col < 11; col++)
    {
      idpix = 3*(lin*11 + col);
      if (lin%2 == 0)   // even line
      {
        idcolor = motif[24 + lin*11 + col];
      }
      else              // odd line reversed
      {
        idcolor = motif[24 + lin*11 + 10 - col];
      }

      if (idcolor > 19)      // 20 to 27  intensity=3
      {
        idcolor = 3*(idcolor - 20);  // RGB intensity 3
        leds[idpix] = CRGB ( motif[idcolor], motif[idcolor+1], motif[idcolor+2]);
        leds[idpix + 1] = CRGB ( motif[idcolor], motif[idcolor+1], motif[idcolor+2]);
        leds[idpix + 2] = CRGB ( motif[idcolor], motif[idcolor+1], motif[idcolor+2]);

      }
      else if (idcolor > 9)  //  10 to 17  intensity=2
      {
        idcolor = 3*(idcolor - 10);  // RGB intensity 2
        leds[idpix] = CRGB ( motif[idcolor], motif[idcolor+1], motif[idcolor+2]);
        leds[idpix + 1] = CRGB ( 0, 0, 0);
        leds[idpix + 2] = CRGB ( motif[idcolor], motif[idcolor+1], motif[idcolor+2]);
      }
      else                   //  0 to 7  intensity=1
      {
        idcolor *= 3;
        leds[idpix] = CRGB ( 0, 0, 0);
        leds[idpix + 1] = CRGB ( motif[idcolor], motif[idcolor+1], motif[idcolor+2]);
        leds[idpix + 2] = CRGB ( 0, 0, 0);
      }
    }
  }
  FastLED.show();
}

/*
 * Set a single Matrix pixel
 */
void DoMxPixel(int pixel, int intensite)
{
int idpix = 3*pixel;

  switch(intensite)
  {
  case 0:
    leds[idpix] =     CRGB ( 0, 0, 0);
    leds[idpix + 1] = CRGB ( 0, 0, 0);
    leds[idpix + 2] = CRGB ( 0, 0, 0);
    break;

  case 1:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( 0,  20,   0);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;

  case 2:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( 0,  80,   0);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;

  case 3:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( 0, 140,   0);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;

  case 4:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( 0, 200,   0);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;

  case 5:
    leds[idpix] =     CRGB ( 0, 200,   0);
    leds[idpix + 1] = CRGB ( 0,   0,   0);
    leds[idpix + 2] = CRGB ( 0, 200,   0);
    break;

  case 6:
    leds[idpix] =     CRGB ( 0, 200,   0);
    leds[idpix + 1] = CRGB ( 0, 200,   0);
    leds[idpix + 2] = CRGB ( 0, 200,   0);
    break;

  default:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( 0,   0,   0);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;
  }
}

/*
 * Matrix screen
 */
void DrawMatrix()
{
int pixel;
char intensite;
char startval;

  pixel = random(0,11);
  startval=random(4,7);
  mxFB1[pixel] = startval;

  for (int lin=0; lin < 8; lin++)    // draw matrix 1
  {
    for (int col = 0; col < 11; col++)
    {
      pixel = lin*11 + col;
      if (lin%2 == 0)    // even line
      {
        intensite = lin*11 + col;
      }
      else               // odd line reversed
      {
        intensite = lin*11 + 10 - col;
      }
      DoMxPixel(pixel, mxFB1[intensite]);
    }
  }

  for (int lin=0; lin < 8; lin++)    // modify matrix 2
  {
    for (int col = 0; col < 11; col++)
    {
      intensite = mxFB1[lin*11 + col];
      if (intensite > 0)
      {
        mxFB2[lin*11 + col] = intensite - 1;
        if (lin < 7)
          mxFB2[(lin+1)*11 + col] = intensite;
      }
    }
  }
  for (int lin=0; lin < 8; lin++)    // copy 2 to 1
  {
    for (int col = 0; col < 11; col++)
    {
      mxFB1[lin*11 + col] = mxFB2[lin*11 + col];
    }
  }
  FastLED.show();
}

/*
 * Set a single Fireworks pixel
 */
void DoFwPixel(int pixel, int R, int G, int B,int intensite)
{
int idpix = 3*pixel;

  switch(intensite)
  {
  case 0:
    leds[idpix] =     CRGB ( 0, 0, 0);
    leds[idpix + 1] = CRGB ( 0, 0, 0);
    leds[idpix + 2] = CRGB ( 0, 0, 0);
    break;

  case 1:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( R/10, G/10, B/10);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;

  case 2:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( R/5, G/5, B/5);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;

  case 3:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( R/2, G/2, B/2);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;

  case 4:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( R,  G, B);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;

  case 5:
    leds[idpix] =     CRGB ( R,  G, B);
    leds[idpix + 1] = CRGB ( 0,   0,   0);
    leds[idpix + 2] = CRGB ( R,  G, B);
    break;

  case 6:
    leds[idpix] =     CRGB ( R,  G, B);
    leds[idpix + 1] = CRGB ( R,  G, B);
    leds[idpix + 2] = CRGB ( R,  G, B);
    break;

  default:
    leds[idpix] =     CRGB ( 0,   0,   0);
    leds[idpix + 1] = CRGB ( 0,   0,   0);
    leds[idpix + 2] = CRGB ( 0,   0,   0);
    break;
  }
}

/*
 * Fireworks screen
 */
void DrawFireworks()
{
int pixel;
int column;
char intensite;
char startval;

  for (int i=0; i< 2; i++)
  {
    column = random(0,11);
    startval=random(4,7);
    mxFB1[column] = startval;
    RandomColor();
    fiR[column] = cR;
    fiG[column] = cG;
    fiB[column] = cB;
  }

  for (int lin=0; lin < 8; lin++)            // draw matrix 1
  {
    for (int col = 0; col < 11; col++)
    {
      pixel = lin*11 + col;
      if (lin%2 == 0)    // even line
      {
        intensite = lin*11 + col;
      }
      else               // odd line reversed
      {
        intensite = lin*11 + 10 - col;
      }
      DoFwPixel(pixel, fiR[col],fiG[col],fiB[col], mxFB1[intensite]);
    }
  }

  for (int lin=0; lin < 8; lin++)            // modify matrix 2
  {
    for (int col = 0; col < 11; col++)
    {
      intensite = mxFB1[lin*11 + col];
      if (intensite > 0)
      {
        mxFB2[lin*11 + col] = intensite - 1;
        if (lin < 7)
          mxFB2[(lin+1)*11 + col] = intensite;
      }
    }
  }
  for (int lin=0; lin < 8; lin++)            // copy 2 to 1
  {
    for (int col = 0; col < 11; col++)
    {
      mxFB1[lin*11 + col] = mxFB2[lin*11 + col];
    }
  }
  FastLED.show();
}
