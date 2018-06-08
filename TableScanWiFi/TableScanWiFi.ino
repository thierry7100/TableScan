/*
 *  Contrôle d'une table tournante pour Scanner 3D 
 *  Commande via le port 80 et un serveur Web embarqué
 *  Affiche une page Web permettant de commander le mouvement de la table et donc de l'objet
 *  Affiche sur le port série les informations de contôle et l'adresse IP du module
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Stepper.h>

#include "AccesWifi.h"



float AngleCourant = 0.0;

#define MOTEUR_M42SP 1
#if MOTEUR_M42SP > 0
//  Pour moteur M42SO. Vitesse assez élevée et 48 pas par tour
#define STEPS 48       // Nb steps for one revolution
#define RPM 20         // Max RPM
#define MIN_DELAY 50    // Delay to allow Wifi to work//  Pour moteur M42SP-5, même délai min et max.
#define MAX_DELAY  50
#else
//  Pour moteur 28BYJ-48. Vitesse assez faible et 2048 pas par tour
#define STEPS 2048      // NB steps for one revolution
#define RPM 2          // Max RPM
#define MIN_DELAY 2    // Delay to allow Wifi to work
#define MAX_DELAY 50    // Pour accélération progressive
#endif


int STBY = 5;     // GPIO 5 TB6612 Standby
int LED = 2;      // Build in LED for D1 Mini

// GPIO Pins for Motor Driver board
Stepper stepper(STEPS, 16, 12, 14, 13);
//  16 --> D0
//  12 --> D6
//  14 --> D5
//  13 --> D7

// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server(80);
String PageWeb;

//  Fait tourner le moteur d'un nombre de pas
//  Pas d'appel direct pour ne pas bloquer le Wifi

void Tourne(int nb_step)
{
unsigned long Ts = millis();
unsigned long wait;
int Half = nb_step/2;

  for (int i=0;i<nb_step;i++) 
  {   // This loop is needed to allow Wifi to not be blocked by step
    //  Calcule temps d'attente pour simuler accélration déccélération
    if ( i < MAX_DELAY && i <= Half)
      wait = 50 - i;
    else if ( i >= Half && nb_step - i < MAX_DELAY )
      wait = MAX_DELAY + 2 - nb_step + i;
    else
      wait = MIN_DELAY;
     if ( wait < MIN_DELAY ) wait = MIN_DELAY;
    stepper.step(1);
    delay(wait);   
  }
  Serial.print("Tourne de "); Serial.print(nb_step); Serial.print(" pas en "); Serial.print(millis() - Ts); Serial.println(" ms");
}

//  Construit la chaine de la page Web
void InitStrWeb()
{
  PageWeb = "<!DOCTYPE html>\n<html>\n<head>\n<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n<title>TableTournante</title>\n";
  PageWeb += "<script type = \"text/javascript\">\nvar iNbPas = 8;\nvar dAngle = 0.0;\nvar iCurVue = 0;\nvar urlOption = 'vide';";
  PageWeb += "function Init()\n{\nvar i1 = 0, i2 = 0;\nvar substr1;\nurlOption = window.location.search;\nvar searchParams = new URLSearchParams(urlOption);\nif ( searchParams.has(\"Angle\") )\n{\ndocument.getElementById(\"CurVue\").value = searchParams.get(\"Vue\");\n   document.getElementById(\"Angle\").value = searchParams.get(\"Angle\");\n    document.getElementById(\"NbPas\").value = searchParams.get(\"NbPas\");\n    NbPas = parseInt(document.getElementById(\"NbPas\").value);\ndAngle = parseFloat(document.getElementById(\"Angle\").value);\n  iCurVue = parseInt(document.getElementById(\"CurVue\").value);\n}\n}\n";
  PageWeb += "function Tourne()\n{\nNbPas = document.getElementById(\"NbPas\").value;\niCurVue++;\ndocument.getElementById(\"CurVue\").value = iCurVue;\ndAngle += 360.0 / NbPas;\nif ( dAngle >= 360 ) dAngle -= 360;\ndocument.getElementById(\"Angle\").value = dAngle;\nurlOption = 'TableTournante.html?NbPas=' + NbPas + '&Vue=' + iCurVue + '&Angle=' + dAngle;\nwindow.location = urlOption;\n}";
  PageWeb += "</script>\n</head>\n<body onload=\"Init();\"><h2> Table tournante scanner 3D </h2>\n";
  PageWeb += "<label>Nombre de vues (360°)</label><input name=\"NbPas\" id=\"NbPas\" value=\"8\" type=\"number\" style=\"width: 40px;\"/><br>\n";
  PageWeb += "<label>Vue courante</label><input name=\"CurVue\" id=\"CurVue\" value=\"0\" readonly=\"readonly\" style=\"width: 40px;\" /><label>  Angle</label><input name=\"Angle\" id=\"Angle\" value=\"0\" readonly=\"readonly\" style=\"width: 40px;\" /><br>\n";
  PageWeb += "<button  onclick=Tourne(); style=\"width: 300px; height: 100px; font-size: x-large;\">Tourne table</button><p><br></p></body>\n</html>\n";
}

//  Affiche la page HTML du serveur
void TableTournanteInit()
{
   server.send (200, "text/html", PageWeb );
   AngleCourant = 0;
}

//  Appel page Web après action  "Touner"
void TableTournante()
{
float DeltaAngle;
int nb_step;

//  Serial.printf("Réception URL avec %d parametres\n", server.args());
//  for (int i = 0; i < server.args(); i++) 
//  {
//    Serial.print("Param[");  Serial.print(i); Serial.print("] : name = "); Serial.print( server.argName(i)); Serial.print("  Valeur :"); Serial.println(server.arg(i));
//  }
  if ( server.argName(2) == "Angle" )
  {
    DeltaAngle = server.arg(2).toFloat() - AngleCourant; 
    while ( DeltaAngle >= 360.0 ) DeltaAngle -= 360.0;
    while ( DeltaAngle < 0 ) DeltaAngle += 360.0;
    nb_step = (int) (DeltaAngle * STEPS / 360.0);
//    Serial.print("Mouvement moteur "); Serial.print(DeltaAngle); Serial.print("  "); Serial.print(nb_step); Serial.println(" pas."); 
    Tourne(nb_step);
    AngleCourant += DeltaAngle;
    while ( AngleCourant >= 360.0 ) AngleCourant -= 360.0;
    while ( AngleCourant < 0 ) AngleCourant += 360.0;
    Serial.print("Nouvelle position moteur :"); Serial.println(AngleCourant);
  }
  server.send (200, "text/html", PageWeb );

}

//  En cas d'URL non reconnue

void handleNotFound() {
  String message = "Non reconnu\n\n";

  Serial.print("URI non reconnue : "); Serial.println(server.uri() );
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }
  message += "\n\nCommandes possibles:\n'/'    Page de démarrage table tournante\n";
  message += "'/TableTournante?NbPas=X&Vue=Y&Angle=dd' Fait tourner le moteur de l'angle choisi.\n";
  server.send ( 404, "text/plain", message );
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // prepare onboard LED
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  
  // Set default speed to Max (doesn't move motor)
  stepper.setSpeed(RPM);
  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("STEPPER: Connecting to ");
  Serial.println(ssid_1);
  
  WiFi.begin(ssid_1, password_1);
  
  for (int nbtry = 0; nbtry < 50; nbtry++ )
  {
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    if ( WiFi.status() == WL_NO_SSID_AVAIL ) //  KO, SSID indisponible ?
      break;

    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("No connection\nTrying ");
    Serial.println(ssid_2);
    WiFi.begin(ssid_2, password_2);
    
    for (int nbtry = 0; nbtry < 50; nbtry++ )
    {
      if (WiFi.status() == WL_CONNECTED) {
        break;
      if ( WiFi.status() == WL_NO_SSID_AVAIL ) //  KO, SSID indisponible ?
        break;
      }
      Serial.print(".");
      delay(500);
    }
  }
  Serial.println("");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("No connection, aborting");
    while (1);
  }
  Serial.println("WiFi connected");
  


  // Print the IP address
  Serial.println(WiFi.localIP());

 
  // Blink onboard LED to signify its connected
  blink();
  blink();
  //  Start mDNS 
  if ( !MDNS.begin("TableScan") )
  {
     Serial.println("Error setting up mDNS responder");
  }
  else
  {
    Serial.println("mDNS responder started OK, will answer to TableScan.local");
  }
  blink();
  blink();
  blink();
  blink();

    //  On indique ensuite les "pages" qui doivent etre traitées
   
  server.on("/", TableTournanteInit);
  server.on("/TableTournante.html", TableTournante);
  server.onNotFound ( handleNotFound );
  InitStrWeb();   //  COnstrui la chaine de la page Web
   // on commence a ecouter les requetes venant de l'exterieur
    server.begin();

  // Start the server
  server.begin();
  Serial.println("HTTP server started");

}

void loop() {
  server.handleClient();
}


void blink() {
  digitalWrite(LED, LOW);
  delay(100); 
  digitalWrite(LED, HIGH);
  delay(100);
}

