#include <Arduino.h>
#include <math.h>
#include <LiquidCrystal.h>
#include <AutoPID.h>
#include <Timer.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"


// pines encoder
int swt  = 2;   // rojo
int clk  = 3;   // verde 
int data = 5;   // azul
 
// pines lcd
const int rs = 12; // rojo 
const int en = 13; // azul
const int d4 = 8;  // verde
const int d5 = 9;  // negro
const int d6 = 10; // verde 
const int d7 = 11; // azul

// relé SSR
const int SSR = 6; 

// buzzer
const int BUZZ = 7; 

// Thermistor 
int ThermistorPin = 0;
long Vo;
double R1 = 10000;
double logR2, Vprom, R2, T;
double c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

// Vars
Timer timer1;  // timer para display
Timer timer2;  // timer para menu

long segundos;
int h, m, s;

bool temporizador_activo = false;
bool calentador_activo = false;

int  menu_posicion = 0;
int  menu_activar = 0;
bool menu_presionado = 0;

int  encoder_val = 0;
int  encoder_last = 0;
int  encoder = 0;

char   buffer[18];

// **********************************************************************************
double LeeTemperatura(void);
void cada_un_segundo(void);
void desactiva_menu(void);
void doEncoder(void);

// init lcd
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// PID control
#define KP 0.1
#define KI 0
#define KD 0

const double TimeBase=5000;
double Input, Setpoint;
bool relayState;

AutoPIDRelay myPID(&Input, &Setpoint, &relayState, TimeBase,  KP, KI, KD);


void setup() {

    Serial.begin(9600);  

    //pin Mode declaration 
    pinMode (clk, INPUT_PULLUP);
    pinMode (data, INPUT_PULLUP);
    pinMode (swt, INPUT_PULLUP);
    pinMode (SSR, OUTPUT);
    pinMode (BUZZ, OUTPUT);
    
    // Aref -> 3.3v
    analogReference(EXTERNAL);

    //Initialise 16*2 LCD
    lcd.begin(16, 2); 
    lcd.print("Sanyo Incubator"); 
    lcd.setCursor(0, 1);
    lcd.print("V20211008"); 

    //recupera valores
    EEPROM_readAnything(10, Setpoint);
    EEPROM_readAnything(20, calentador_activo);
    EEPROM_readAnything(30, temporizador_activo);
    EEPROM_readAnything(40, segundos);


    delay(2000);
    lcd.clear();
    
    //interrupcion encoder
    attachInterrupt(digitalPinToInterrupt(clk), doEncoder, CHANGE);

    // init timer
    timer1.every(1000, cada_un_segundo);

    // setup PID time interval = 1 minuto
    myPID.setTimeStep(60000);
    
}

// #####################################################################################################

void loop() {

    
    if(calentador_activo){
        myPID.run();
        digitalWrite(SSR, relayState);
    }else{
        digitalWrite(SSR, LOW);
    }


    if (menu_posicion == 1){
        if (encoder < 10){
            lcd.setCursor(0,1);
            lcd.print(" [NO]  SI   TE ");
            menu_activar = 0;
        }
        if (encoder > 10){ 
            if (encoder < 20){
                lcd.setCursor(0,1);
                lcd.print("  NO  [SI]  TE ");
                menu_activar = 1;
            }
        }
        if (encoder > 20){
            lcd.setCursor(0,1);
            lcd.print("  NO   SI  [TE]");
            menu_activar = 2;
        }
        if (encoder > 30) encoder = 30;
        if (encoder < 0 ) encoder = 0;
    }

    if (menu_posicion == 2){
        if (encoder_last > encoder_val){
            Setpoint -= 0.1;
            if(Setpoint < 0){
                Setpoint = 0;
            }
            lcd.setCursor(0,1);
            lcd.print("   ");
            lcd.print(Setpoint);
            lcd.print((char)223);
        }
        if (encoder_last < encoder_val){
            Setpoint += 0.1;
            if(Setpoint > 50){
                Setpoint = 50;
            }
            lcd.setCursor(0,1);
            lcd.print("   ");
            lcd.print(Setpoint);
            lcd.print((char)223);
        }
        encoder_last = encoder_val;
    }

    if (menu_posicion == 3){

        if (encoder_last > encoder_val){
            segundos -= 900;
            if(segundos < 0){
                segundos = 0;
            }

            h = segundos / 3600;
            m = ( segundos % 3600 ) / 60;
            s = ( segundos % 3600 ) % 60;

            lcd.setCursor(3, 1);
            sprintf(buffer,"%02d:%02d:%02d",h,m,s);
            lcd.print(buffer);
            
        }
        if (encoder_last < encoder_val){
            segundos += 900;

            h = segundos / 3600;
            m = ( segundos % 3600 ) / 60;
            s = ( segundos % 3600 ) % 60;
            
            lcd.setCursor(3, 1);
            sprintf(buffer,"%02d:%02d:%02d",h,m,s);
            lcd.print(buffer);

        }
        encoder_last = encoder_val;
    }

    // click Menu
    if( !digitalRead(swt) & !menu_presionado){
        menu_presionado = 1;

        // activa menu principal
        if(menu_posicion == 0){
            lcd.clear();
            lcd.setCursor(0,0);  
            lcd.print("Activar ?");
            encoder = 0;
            menu_posicion = 1;
        }

        // vuelve del menu Activar NO SI TEMP
        else if(menu_posicion == 1){

            if(menu_activar == 0){

                menu_posicion = 0;
                calentador_activo = 0;
                temporizador_activo = 0;

                EEPROM_writeAnything(20, calentador_activo);
                EEPROM_writeAnything(30, temporizador_activo);

            }
            else if(menu_activar == 1){

                lcd.clear();
                lcd.setCursor(0,0);  
                lcd.print("Temperatura ?");
                lcd.setCursor(0,1);
                lcd.print("   ");
                lcd.print(Setpoint);
                lcd.print((char)223);
                menu_posicion = 2;
                temporizador_activo = 0;
                encoder = 512;
            
            }
            else if(menu_activar == 2){

                lcd.clear();
                lcd.setCursor(0,0);  
                lcd.print("Temporizador ?");

                lcd.setCursor(3,1);
                lcd.print("00:00:00");

                segundos = 0;
                menu_posicion = 3;
                temporizador_activo = 1;
                encoder = 0;

            }
            
        }

        // vuelve del menu setea temperatura
        else if(menu_posicion == 2){

            if (menu_activar == 1){
                 
                menu_posicion = 0;
                calentador_activo = 1;
                temporizador_activo = 0;

            }
            else if (menu_activar == 2){

                lcd.clear();
                lcd.setCursor(0,0);  
                lcd.print("Temporizador ?");
                lcd.setCursor(0,1);

                segundos = 0;

                h = segundos / 3600;
                m = ( segundos % 3600 ) / 60;
                s = ( segundos % 3600 ) % 60;

                lcd.setCursor(3, 1);
                sprintf(buffer,"%02d:%02d:%02d",h,m,s);
                lcd.print(buffer);

                menu_posicion = 3;
                temporizador_activo = 1;
                encoder = 512;
            }

            EEPROM_writeAnything(10, Setpoint);
            EEPROM_writeAnything(20, calentador_activo);
            EEPROM_writeAnything(30, temporizador_activo);
            
        }

        // vuelve del menu setea temporizador
        else if(menu_posicion == 3){ 

            menu_posicion = 0;    
            calentador_activo = 1;
            temporizador_activo = 1;

            EEPROM_writeAnything(20, calentador_activo);
            EEPROM_writeAnything(30, temporizador_activo);
            EEPROM_writeAnything(40, segundos);

        }

    }

    if(digitalRead(swt)){
        menu_presionado = 0;
    }

    // Actualiza display
    timer1.update();
  
}

// #####################################################################################################

void doEncoder(){
    if (digitalRead(clk) == digitalRead(data))  {
        encoder++;
    }else{
        encoder--;
    }
    encoder_val = encoder / 2.5;
}


void cada_un_segundo(){

    // lee temperatura
    Input = LeeTemperatura(); 


    if(menu_posicion == 0){
        lcd.clear();

        lcd.setCursor(0,0);  
        lcd.print("T: ");
        lcd.print(Input);
        lcd.print("/");
        lcd.print(Setpoint);
        lcd.print((char)223);

        lcd.setCursor(0,1);
        lcd.print("E: ");

        if(temporizador_activo){

            h = segundos / 3600;
            m = ( segundos % 3600 ) / 60;
            s = ( segundos % 3600 ) % 60;

            lcd.setCursor(3, 1);
            sprintf(buffer,"%02d:%02d:%02d",h,m,s);
            lcd.print(buffer);

            segundos--;

            if(segundos < 0){
                segundos = 0;
                calentador_activo = 0;
                temporizador_activo = 0;

                EEPROM_writeAnything(20, calentador_activo);
                EEPROM_writeAnything(30, temporizador_activo);
            }

            // Solo cada 1 minuto
            if ( s == 0 ) {
                // grabo los segundos en la eeprom
                EEPROM_writeAnything(40, segundos);
                // imprimo temperatura actual por puerto serie
                Serial.println(Input);
            } 

        }else{

            if(calentador_activo){
                lcd.print("Activo    ");
            }else{
                lcd.print("Inactivo  ");
            }

        }

    }
    
}

double LeeTemperatura(){

    delay(10);
    Vo = analogRead(ThermistorPin);
    delay(10);
    Vo += analogRead(ThermistorPin);
    delay(10);
    Vo += analogRead(ThermistorPin);

    Vprom = (double)Vo / 3;

    R2 = R1 *  ( 1 / (1023.0 / Vprom - 1.0));
    logR2 = log(R2);

    T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
    T = T - 273.15;

    return T;

}