#include <SPI.h>
#include <SD.h>
#include <Stepper.h>
#include <ThreeWire.h>  
#include <RtcDS1302.h>

ThreeWire myWire(A5, A4, 3);
RtcDS1302<ThreeWire> Rtc(myWire);
const int stepsPerRevolution = 2048;

// Configurazione Stepper Motor
Stepper myStepper(stepsPerRevolution, A0, A2, A1, A3);
float currentAzimuth = 0.0;

const int chipSelect = 4;
const int pinPulsanteStart = 2; // Pulsante START
const int pinPulsanteSx = 8;    // Pulsante rotazione sinistra
const int pinPulsanteDx = 9;    // Pulsante rotazione destra

const int ledR = 5, ledA = 6, ledV = 7;

// Epoch UTC relativa ai TLE usati in GMAT
RtcDateTime epochTime(2026, 6, 3, 23, 13, 8);

File dataFile; 

float t1 = -1, az1 = 0, el1 = 0;
float t2 = -1, az2 = 0, el2 = 0;

void setup() {
  Serial.begin(9600);
  pinMode(ledR, OUTPUT); pinMode(ledA, OUTPUT); pinMode(ledV, OUTPUT);
  
  pinMode(pinPulsanteStart, INPUT_PULLUP);
  pinMode(pinPulsanteSx, INPUT_PULLUP);
  pinMode(pinPulsanteDx, INPUT_PULLUP);
  
  Rtc.Begin();

  if (!SD.begin(chipSelect)) {
    Serial.println("ERRORE SD!");
    while(1);
  }

  myStepper.setSpeed(10);

  Serial.println("\n--- FASE DI CALIBRAZIONE ---");
  Serial.println("Usa i pulsanti per allineare la lancetta al Nord.");
  Serial.println("Quando la bussola coincide, premi il pulsante START");

  // Loop di calibrazione manuale
  while (digitalRead(pinPulsanteStart) == HIGH) {
    // Lampeggio del LED giallo senza bloccare il codice per indicare stato di calibrazione
    digitalWrite(ledA, (millis() / 200) % 2);
    
    if (digitalRead(pinPulsanteSx) == LOW) {
      myStepper.step(1);  
      delay(16);      
    }
    
    if (digitalRead(pinPulsanteDx) == LOW) {
      myStepper.step(-1);
      delay(16);     
    }
  } 

  currentAzimuth = 0.0;
  
  // Spengo le bobine dopo lo sforzo della calibrazione per non far scaldare lo stepper
  digitalWrite(A0, LOW); digitalWrite(A1, LOW);
  digitalWrite(A2, LOW); digitalWrite(A3, LOW);

  dataFile = SD.open("IssArdu.csv");
  if (!dataFile) {
    Serial.println("File IssArdu.csv non trovato sulla SD!");
    while(1);
  }

  Serial.println("\nZero memorizzato! Tracking Avviato. Ricerca delle coordinate corrispondenti a quest'orario in corso...");
  digitalWrite(ledA, LOW);  
  digitalWrite(ledV, HIGH); // Accendo il LED verde per dire che ho trovato le coordinate corrispondenti all'orario presente
  delay(1000);
} 

void loop() {
  RtcDateTime now = Rtc.GetDateTime();
  
  // Sincronizzazione Temporale:
  // A Maggio a Padova siamo UTC + 2 (1 fuso orario, 1 ora solare), perciò aggiungo 7200 s. Inoltre per motivi ignoti c'è un ritardo di 101 s fra RTC e orario UTC, che perciò aggiugo
  long elapsedSeconds = (long)(now.TotalSeconds64() - epochTime.TotalSeconds64()) - 7200 + 101;

  bool trovatoIntervallo = false;

  if (elapsedSeconds > t2 || t1 == -1) {
    
    while (dataFile.available()) {
      // Avanza nello stream partendo dall'ultimo byte letto, senza mai riaprire il file da capo visto che il tempo scorre in una sola direzione
      long csvTime = dataFile.parseInt();
      if (csvTime == 0 && !dataFile.available()) break;

      float csvAz = dataFile.parseFloat();
      float csvEl = dataFile.parseFloat();

      if (csvTime <= elapsedSeconds) {
        t1 = (float)csvTime;
        az1 = csvAz;
        el1 = csvEl;
      } else {
        t2 = (float)csvTime;
        az2 = csvAz;
        el2 = csvEl;
        trovatoIntervallo = true;
        break; // Trovato l'intervallo temporale corretto, esco interrompendo la lettura
      }
    }
  } else {
    // Sono ancora dentro l'intervallo temporale già memorizzato nei float globali
    trovatoIntervallo = true;
  }

  // Se l'intervallo è valido ed è stato trovato, calcolo l'interpolazione
  if (trovatoIntervallo && t1 != -1) {
    float t_gap = t2 - t1;
    float t_frazione = (elapsedSeconds - t1) / t_gap;
    
    float interpAz = az1 + (az2 - az1) * t_frazione;
    float interpEl = el1 + (el2 - el1) * t_frazione;

    // Sottraggo le due ore e sommo quei 101 s 
    uint64_t totalSecondsUtc = now.TotalSeconds64() - 7200 + 101;
    RtcDateTime realUtcTime(totalSecondsUtc);
    
    char timeString[20];
    snprintf_P(timeString, sizeof(timeString), PSTR("%02u:%02u:%02u "), 
               realUtcTime.Hour(), realUtcTime.Minute(), realUtcTime.Second());

    Serial.print("Orario UTC: ");Serial.print(timeString);
    Serial.print(" | Elapsed seconds: "); Serial.print(elapsedSeconds);
    Serial.print(" | Azimuth: "); Serial.print(interpAz);
    Serial.print(" | Elevation: "); Serial.println(interpEl);

    muoviOttimizzato(interpAz);
    aggiornaLED(interpEl);
    
    // Spengo i transistor del driver per non far scaldare il motore in statica
    digitalWrite(A0, LOW); digitalWrite(A1, LOW);
    digitalWrite(A2, LOW); digitalWrite(A3, LOW);
  } else {
    Serial.println("Fine delle effemeridi nel file o dati non disponibili per questo orario.");
  }
  
  delay(1000); // Scansione del loop tarata sul clock dell'RTC (1 secondo)
}

void muoviOttimizzato(float targetAz) {
  // Compensazione della declinazione magnetica
  // Sottraiamo 4.1° per convertire l'Azimuth Geografico di GMAT in Azimuth Magnetico per la bussola a Padova
  float declinazionePadova = 4.1;
  targetAz = targetAz - declinazionePadova;

  // Normalizzazione dell'angolo tra 0 e 360 gradi
  while (targetAz >= 360) targetAz -= 360;
  while (targetAz < 0) targetAz += 360;

  // Algoritmo di calcolo del cammino minimo di rotazione (evita inversioni di 300°)
  float diff = targetAz - currentAzimuth;
  if (diff > 180) diff -= 360;
  if (diff < -180) diff += 360;

  float passiFloat = (diff * (float)stepsPerRevolution) / 360.0;
  long passiInteri = (long)passiFloat;

  if (abs(passiInteri) >= 1) {
    myStepper.step(passiInteri);
    currentAzimuth += (float)passiInteri * 360.0 / (float)stepsPerRevolution;
    Serial.print(" -> Motore in movimento! Passi: "); Serial.println(passiInteri);
  }
}

void aggiornaLED(float el) {
  digitalWrite(ledR, LOW); digitalWrite(ledA, LOW); digitalWrite(ledV, LOW);
  if (el < 0) digitalWrite(ledR, HIGH);          // ISS sotto l'orizzonte
  else if (el < 10) digitalWrite(ledA, HIGH);    // ISS vicina all'orizzonte
  else digitalWrite(ledV, HIGH);                 // ISS visibile in cielo
}