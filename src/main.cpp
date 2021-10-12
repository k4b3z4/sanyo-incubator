#include <Arduino.h>
#include <LiquidCrystal.h>
#include <PID_v1.h>
#include <Timer.h>


// pines encoder
int swt  = 2;   // rojo
int clk  = 3;   // verde 
int data = 5;   // azul
 
// pines lcd
const int rs = 12;  // rojo 
const int en = 13;  // azul
const int d4 = 8;  // verde
const int d5 = 9;  // negro
const int d6 = 10; // verde 
const int d7 = 11; // azul

// relé SSR
const int SSR = 6; 

// buzzer
const int BUZZ = 7; 

// sensor temperatura 100k
const int TEMP = 0; // A0


// Vars
Timer timer1;  // timer para display
Timer timer2;  // timer para menu

bool calentador_activo = false;
int  menu_posicion = 0;
bool menu_activar = 0;
bool menu_presionado = 0;

int  encoder_val = 0;
int  encoder_last = 0;
int  encoder = 0;

int Vo1, Vo2, Vo3;
double R1 = 1000;
double Vo, logR2, R2, T, Tc;
double c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

char   buffer[18];

// **********************************************************************************
double LeeTemperatura(void);
void cada_un_segundo(void);
void desactiva_menu(void);
void doEncoder(void);

// init lcd
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// PID control
int Kp = 40;
int Ki = 2;
int Kd = 800;
double Setpoint, Input, Output;
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);

unsigned long WindowSize = 5000;
unsigned long windowStartTime;

void setup() {

    Serial.begin(9600);  

    timer1.every(1000, cada_un_segundo);

    lcd.begin(16, 2); //Initialise 16*2 LCD
    lcd.print("Sanyo Incubator "); 
    lcd.setCursor(0, 1);
    lcd.print("V20211008"); 
    
    delay(2000);
    lcd.clear();
    
    //pin Mode declaration 
    pinMode (clk, INPUT_PULLUP);
    pinMode (data, INPUT_PULLUP);
    pinMode (swt, INPUT_PULLUP);
    pinMode (SSR, OUTPUT);
    pinMode (BUZZ, OUTPUT);

    //interrupcion encoder
    attachInterrupt(digitalPinToInterrupt(clk), doEncoder, CHANGE);

    windowStartTime = millis();
    Setpoint = 20;
    myPID.SetOutputLimits(0, WindowSize);
    myPID.SetMode(AUTOMATIC);

}

// #####################################################################################################

void loop() {

    // lee temperatura
    Input = LeeTemperatura();

    
    if(calentador_activo){

        // Cálculos de PID
        myPID.Compute();

        unsigned long now = millis();
        if (now - windowStartTime > WindowSize)
        { //time to shift the Relay Window
            windowStartTime += WindowSize;
        }
        if (Output > now - windowStartTime) {
            digitalWrite(SSR, HIGH);
        }else{
            digitalWrite(SSR, LOW);
        }

    }else{

        digitalWrite(SSR, LOW);
    }


    if (menu_posicion == 1){
        if (encoder < 10){
            lcd.setCursor(0,1);
            lcd.print("  [NO]    SI   ");
            menu_activar = 0;
        }
        if (encoder > 10){
            lcd.setCursor(0,1);
            lcd.print("   NO    [SI]  ");
            menu_activar = 1;
        }
        if (encoder > 20) encoder = 20;
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

    // Menu
    if( !digitalRead(swt) & !menu_presionado){
        menu_presionado = 1;

        if(menu_posicion == 0){
            lcd.clear();
            lcd.setCursor(0,0);  
            lcd.print("Activar ?");
            lcd.setCursor(0,1);       
            if(calentador_activo){
                lcd.print("   NO    [SI]  ");  
                menu_activar = 1;
                encoder = 20;
            }else{
                lcd.print("  [NO]    SI   ");  
                menu_activar = 0;
                encoder = 0;
            }
            menu_posicion = 1;
        }
        else if(menu_posicion == 1){
            lcd.clear();
            lcd.setCursor(0,0);  
            lcd.print("Temperatura ?");
            lcd.setCursor(0,1);
            lcd.print("   ");
            lcd.print(Setpoint);
            lcd.print((char)223);
            menu_posicion = 2;
            encoder = 512;
        }
        else if(menu_posicion == 2){
            menu_posicion = 0;
            calentador_activo = menu_activar;
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
        if(calentador_activo){
            lcd.print("Activo    ");
        }else{
            lcd.print("Inactivo  ");
        }
    }
    
    Serial.println(Input);
}


double LeeTemperatura(){


    delay(20);
    Vo1 = analogRead(TEMP);
    delay(5);
    Vo2 = analogRead(TEMP);
    delay(5);
    Vo3 = analogRead(TEMP);

    Vo = (double)(Vo1 + Vo2 + Vo3) / 3;

    R2 = R1 * (1023.0 / Vo - 1.0);
    logR2 = log(R2);
    T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
    T = T - 273.15;

    return T;
}