// Doorman arduino by Doctor Bit Projects
// More info at: http://blog.drbit.nl

#include <Wire.h>
#include <SPI.h>
#include <Ethernet.h>


// Outputs
int statusLed = 9;              // ~ LED Green - Power

const int temperature_pin = 0;
const int ldr_pin = 1;

unsigned int seed = 0;

// Misc
bool isConnectedToLAN = false;

// Ethernet
byte mac[] = {0x00, 0xAB, 0xBB, 0xCC, 0xDA, 0x02};
IPAddress ip(192,168,1,2);
int port = 80;
EthernetServer server(80);

// Accept pending connections
EthernetClient client;

String receivedData;
unsigned int receivedSeed;
boolean start_recording = false;
boolean start_seed_number = false;

bool clientButtonPressed = false;
bool buttonPressed = false;
bool buttonPressedLast = false;
bool buttonIsActive = false;


// When adding more channels always update the 3 next parameters.
// Button string and button description MUST NOT be the same. If strings are equal or contain in themselves an error will triger
char* buttonStrings[]=    {"ch1"  , "ch2"     , "ch3" ,"ch4"};          // Channel names
char* buttonDescription[]={"Light", "Sokets"  , "CNC" ,"extraCH"};      // Description
int pinChanels []=        {8      , 7         ,6      ,13};             // Channel pin numbers
bool chFlag [] =          {false  ,false      ,false  ,false};          // Flag to mark which channel has ben requested by http
const int numberOfButtons = sizeof(chFlag);                             // calculates numer of buttons to print
unsigned long portaP_start_open[numberOfButtons] = {};                  // time mark when swithc is activated
unsigned long portaP_time_delay[numberOfButtons] = {};                  // time delay custom set

boolean faviconFlag = false;                                            // Special favicon flags


void setup() {
    Serial.begin(9600);

    Serial.println(F("* Starting Remote Lighs Controller V0.1 *"));
    
    for (int i = 0; i < numberOfButtons; i++ ) pinMode(pinChanels [i], OUTPUT);     // Set all channels pins to OUTPUTS
    pinMode(statusLed, OUTPUT);
        
    digitalWrite(statusLed, HIGH);
    delay(2000);
    digitalWrite(statusLed, LOW);

    while (!isConnectedToLAN) {
        try_lan_connection ();
    }

    Serial.println(F("* Ready"));
   
}


void try_lan_connection () {
    Serial.println (F("Connecting..."));
    Ethernet.begin(mac, ip);
    server.begin();
    // Start listening for connections
    isConnectedToLAN = true;
    Serial.print (F("IP: "));
    Serial.print(Ethernet.localIP());
    Serial.print (F(":"));
    Serial.println (port);
    // Turn on the power LED to indicate that the device is functioning properly
    digitalWrite(statusLed, HIGH);
}

boolean received_slash = false;
unsigned int captured_seed = 0;
int seed_counter =0;


void loop() {
    client = server.available();
    // Receive data
    if (client) {
        Serial.println(F("Client connected"));
        boolean currentLineIsBlank = true;
        boolean firstLine = true;
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                Serial.write(c);
                
                // end of client.. print header
                if (c == '\n' && currentLineIsBlank) {
                    Serial.println(F("Serve Website"));
                    // send a standard http response header
                    client.println(F("HTTP/1.1 200 OK"));
                    client.println(F("Content-Type: text/html"));
                    client.println(F("Connnection: close"));
                    client.println(F(""));
                    client.println(F("<!DOCTYPE HTML>"));
                    client.println(F("<html>"));
                    break;
                }
                if (c == '\n') {
                    // you're starting a new line
                    currentLineIsBlank = true;
                    firstLine = false;
                    start_recording = false;
                } else if (c != '\r') {
                    // you've gotten a character on the current line
                    currentLineIsBlank = false;
                }
                
                
                // Read the address for defined text and open door
                if ((c == '*' || c == '/' || c == '.') && firstLine) {
                    start_recording = true;
                    for (int i = 0; i < numberOfButtons; i++ ) {
                        if (receivedData == buttonStrings[i]) {
                            printOpenDebug (i);
                            chFlag [i] = true;
                            continue;
                        }
                    }
                    if (receivedData == "favicon") {     // If received favicon
                        Serial.print(F("||favicon||"));
                        faviconFlag = true;
                    }
                    receivedData = "";
                }

                if (start_recording && (c != '*' && c != '/' && c != '.' )) receivedData += c;

                // CAPTURE SEED NUMBER
                if (c == '=' && firstLine) {
                    start_seed_number = true;
                    seed_counter = 1;
                    captured_seed = 0;
                }
                if (c == ' ' && start_seed_number && firstLine) {
                    start_seed_number = false;
                    Serial.print ("Captured seed = ");
                    Serial.println (captured_seed);
                }
                if (start_seed_number && (c != '=' && c != ' ')) {
                    if (captured_seed != 0) captured_seed = captured_seed * 10;
                    captured_seed += atoi(&c);
                }// CAPTURE SEED NUMBER

                if (c == '/') received_slash = true;
                if (c != '/') received_slash = false;
            }
        }
        
        // When a door has been opend we preset the confirmation page
        boolean doorOpened = false;
        for (int i = 0; i < numberOfButtons; i++ ) {
            if (chFlag [i]) {
                if ((seed == captured_seed) && (seed != 0)) {
                    printHTMLbuttonAction (buttonDescription[i], "Back");
                    obre_sesam (i,3000);
                }else{
                    // Error at matching seed
                    // could be a refresh from the browser or 2 users connected at the same time
                    client.print(F("Error: seed mismatch"));
                    client.print(F("<BR>"));
                    client.print(buttonStrings[i]);
                    client.print(F(" - "));
                    client.print(buttonDescription[i]);
                    client.print(F("<BR>Expected seed: "));
                    client.print(seed);
                    client.print(F("<BR>Received seed: "));
                    client.print(captured_seed);
                    client.println(F("<CENTER><BR><font size=\"30\">"));
                    client.println(F("<HEAD>"));            
                    client.print(F("<META http-equiv=\"refresh\" content=\"8; url=http://"));
                    client.print(Ethernet.localIP());
                    client.print(F("/\">"));
                    client.println(F("</HEAD>"));
                }
                chFlag [i] = false;
                seed = 0;
                doorOpened = true;
                continue;
            }
        }
        if (!doorOpened) {          // in case no door has been open means we are generating the main page.
            if (!faviconFlag){       // (and it's not Favicon)
                seed = random(10000);   // Creates a seed for this session
                if (seed == 0) {
                    seed = random(10000);  // if by a chance seed equals 0 we regenerate. zero is used as a reset.
                }
                Serial.print ("seed created: ");
                Serial.println (seed);
                // Each time we press the door button we have to check the seed so we wont open the door when we refresh the page
            }

            // Print all buttons names and descriptions
            const int nButtons = 4;
            for (int i = 0; i < nButtons; i++){
                printHTMLbutton (buttonStrings[i],buttonDescription[i]);
            }
        }

        HTMLend ();
        client.stop();
        Serial.println(F("Close Connection"));
        Serial.println(F("*****\n"));

    }else check_timings (); // Check timings and see if we have to switch of a previously opened door
}

void printOpenDebug (int num) {
    Serial.print(F("||opens "));
    Serial.print(buttonDescription[num]);
    Serial.print(F("||"));
}

void obre_sesam (int porta, int temps) {
    portaP_start_open[porta] = millis();
    portaP_time_delay[porta] = temps;
    digitalWrite (porta, HIGH);
    Serial.print (F("Open: "));
    Serial.print (porta);
    Serial.print (F(" - Time: "));
    Serial.println (temps);
}

void check_timings () {
    for (int i = 0; i < numberOfButtons; i++ ) {
        if (portaP_time_delay[i] > 0) {
            if ((millis() - portaP_time_delay[i]) > portaP_start_open[i]) {
                digitalWrite (pinChanels [i], LOW);
                portaP_time_delay[i] = 0;
                Serial.print (F("***** "));
                Serial.print (buttonStrings[i]);
                Serial.print (F(" - "));
                Serial.print (buttonDescription[i]);
                Serial.println (F(" OFF *****"));
            }
        }
    }
}

void printHTMLbutton (char* buttoncall, char* buttontext) {

    client.println(F("<BR>"));
    
    client.println(F("<CENTER>"));

    client.print(F("<FORM ACTION=\"http://"));
    client.print(Ethernet.localIP());
    client.print(F("/"));
    client.print(F("*"));
    client.print(buttoncall);
    client.print(F("*\""));
    client.println(F(" method=get >"));

    client.print(F("<INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\""));
    client.print(buttontext);
    client.println(F("\" onClick=\"return confirm(\'Are you sure?\')\" style=\"height:150px;width:400px;font-size:30px;font-weight:bold;\">"));
    
    client.print(F("<INPUT TYPE=\"HIDDEN\" name=\"seed\" value=\""));
    client.print(seed);
    client.println(F("\">"));
            
    client.println(F("</FORM>"));
    client.println(F("</CENTER>"));
}

void printHTMLbuttonAction (char* accio, char* returntext) {
    
    client.println(F("<HEAD>"));            
    client.print(F("<META http-equiv=\"refresh\" content=\"3; url=http://"));
    client.print(Ethernet.localIP());
    client.print(F("/\">"));
    client.println(F("</HEAD>"));
    
    client.println(F("<CENTER><BR><font size=\"30\">"));
    client.println(F("FET!"));
    client.println(F("<BR>"));
    client.println(accio);
    client.println(F("</font><BR>"));
    client.println(F("<BR>"));
    
    client.println(F("<BR>"));
    client.println(F("Press back if you are not returned to main page automatically"));
    client.println(F("<BR>"));
    client.print(F("seed: "));
    client.println(seed);
    client.println(F("<BR>"));
    
    client.print(F("<FORM ACTION=\"http://"));
    client.print(Ethernet.localIP());
    client.print(F("/\" method=get >"));


    client.print(F("<INPUT TYPE=SUBMIT VALUE=\""));
    client.print(returntext);
    client.println(F("\" style=\"height:150px;width:400px;font-size:30px;font-weight:bold;\">"));
    
    client.println(F("</FORM>"));
    client.println(F("</CENTER>"));
}

void HTMLend () {
    client.println(F("</html>"));
    client.println(F(""));
    faviconFlag=false;
}

float get_temp () {
    float temp;
    temp = (5.0 * analogRead(temperature_pin) * 100.0) / 1024;
    return temp;
}

float get_light () {
    float light;
    light = (5.0 * analogRead(ldr_pin) * 100.0) / 1024;
    return light;
}