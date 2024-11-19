#include <ArduinoJson.h>
#include "WiFiProv.h"
#include "WiFi.h"
#include <Preferences.h>
#include <HTTPClient.h>
#include "images.h"
#include "heltec.h"

const char *pop = "abcd1234";           // Proof of possession - otherwise called a PIN - string provided by the device, entered by the user in the phone app
const char *service_name = "PROV_123";  // Name of your device (the Espressif apps expects by default device name starting with "Prov_")
const char *service_key = NULL;         // Password used for SofAP method (NULL = no password needed)
bool reset_provisioned = true;          // When true the library will automatically delete previously provisioned data.
Preferences preferences;
String ssid;
String password;

String town = "Paris";  // EDIT <<<<  your city/town/location name
String Country = "fr";  // EDIT <<<< your country code

String URL = "https://api.openweathermap.org/data/2.5/weather?q=" + town + "," + Country + "&units=metric&lang=fr&appid=";  // URL vers l'API OWM
String ApiKey = "87f121125e92dcf8b25757048ada5ef3";                                                                         // Cle API de OWM

int cycle = 0;  // Init du cycle pour la mise en veille

// Logo de demarrage de l'application
void logo() {
  Heltec.display->clear();
  Heltec.display->drawXbm(0, 0, logo_meteo_width, logo_meteo_height, (const unsigned char *)logo_meteo_bits);
  Heltec.display->display();
}

void displayWeather(String payload) {
  Heltec.display->clear();  // on efface l'affichage
  StaticJsonDocument<2048> doc;
  auto error = deserializeJson(doc, payload);  // debut du parsing JSON
  if (error) {
    Serial.print(F("deserializeJson() Erreur code : "));
    Serial.println(error.c_str());
    return;
  } else {
    const char *location = doc["name"];  // Le lieu
    Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
    Heltec.display->setFont(ArialMT_Plain_16);
    Heltec.display->drawString(64, 0, location);
    Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
    Heltec.display->setFont(ArialMT_Plain_10);
    JsonObject data = doc["main"];
    float temperature = data["temp"];
    int mini = data["temp_min"];
    int maxi = data["temp_max"];
    int pressure = data["pressure"];
    int humidity = data["humidity"];
    Heltec.display->drawString(0, 14, "Temp : " + (String)temperature + "°C");
    Heltec.display->drawString(0, 24, "Min : " + (String)mini + "°C");
    Heltec.display->drawString(0, 34, "Max : " + (String)maxi + "°C");
    Heltec.display->drawString(0, 44, "Pression : " + (String)pressure + " hPa");
    Heltec.display->drawString(0, 54, "Humidite : " + (String)humidity + "%");
    JsonObject weather = doc["weather"][0];
    String sky = weather["icon"];
    displayIcon(sky);
    signalBars();               // Affichage de la qualite du signal wifi
    Heltec.display->display();  // Affichage de l'ecran meteo
  }
}

// Pictogramme meteo
void displayIcon(String sky) {
  String sk = sky.substring(0, 2);
  const char *icon;
  switch (sk.toInt()) {
    case 1:
      icon = one_bits;
      break;
    case 2:
      icon = two_bits;
      break;
    case 3:
      icon = three_bits;
      break;
    case 4:
      icon = four_bits;
      break;
    case 9:
      icon = nine_bits;
      break;
    case 10:
      icon = ten_bits;
      break;
    case 11:
      icon = eleven_bits;
      break;
    case 13:
      icon = thirteen_bits;
      break;
    case 50:
      icon = fifty_bits;
      break;
    default:
      icon = nothing_bits;  // pas d'icône trouvee
      break;
  }
  Heltec.display->drawXbm(100, 15, 30, 30, (const unsigned char *)icon);
}

// Intensite du signal wifi
void signalBars() {
  long rssi = WiFi.RSSI();
  int bars;
  if (rssi > -55) {
    bars = 5;
  } else if (rssi< -55 & rssi > - 65) {
    bars = 4;
  } else if (rssi< -65 & rssi > - 70) {
    bars = 3;
  } else if (rssi< -70 & rssi > - 78) {
    bars = 2;
  } else if (rssi< -78 & rssi > - 82) {
    bars = 1;
  } else {
    bars = 0;
  }
  for (int b = 0; b <= bars; b++) {
    Heltec.display->fillRect(105 + (b * 3), 65 - (b * 2), 2, b * 2);
  }
}

void lightSleep() {
  Serial.println(F("light sleep"));
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  esp_sleep_enable_gpio_wakeup();
  Heltec.display->displayOff();
  int ls = esp_light_sleep_start();
  Serial.printf("light_sleep: %d\n", ls);
  Heltec.display->displayOn();
}

void CredsRecvEvent(arduino_event_t *sys_event) {
  Serial.println(F("\nReceived Wi-Fi credentials"));
  Serial.print(F("\tSSID : "));
  Serial.println((const char *)sys_event->event_info.prov_cred_recv.ssid);
  Serial.print(F("\tPassword : "));
  Serial.println((char const *)sys_event->event_info.prov_cred_recv.password);

  Serial.println(F("They are now stored in flash memory"));
  preferences.putString("ssid", (const char *)sys_event->event_info.prov_cred_recv.ssid);
  preferences.putString("password", (char const *)sys_event->event_info.prov_cred_recv.password);
}

void setup() {
  // Initialisation ecran oled
  Heltec.begin(true /* DisplayEnable */, false /* LoRaDisable */, true /* SerialEnable */);
  logo();  // Affiche le "splash screen"
  delay(3000);
  // Nettoyer ecran
  Heltec.display->clear();

  Serial.begin(115200);
  preferences.begin("credentials", false);

  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");

  if (ssid == "" || password == "") {
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "No value saved for ssid or");
    Heltec.display->drawString(0, 16, "password.");
    Heltec.display->display();
  } else {
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "Wi-Fi credentials found.");
    Heltec.display->drawString(0, 16, "Trying to connect to: ");
    Heltec.display->drawString(0, 32, ssid);
    Heltec.display->display();
    delay(500);
    WiFi.begin(ssid.c_str(), password.c_str());

    // On verifie que les credentiels en memoire permettent la connexion.
    byte count = 0;
    while (WiFi.status() != WL_CONNECTED && count < 10) {
      delay(500);
      count++;
      Serial.print(".");
    }
    WiFi.onEvent(CredsRecvEvent, ARDUINO_EVENT_PROV_CRED_RECV);
    if (count < 10) {
      Heltec.display->clear();
      Heltec.display->drawString(0, 0, "Connected to access point!");
      Heltec.display->display();
      delay(500);
      return;
    }
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "Stored credentials seem");
    Heltec.display->drawString(0, 16, "incorrect or known AP");
    Heltec.display->drawString(0, 32, "is not disponible.");
  }
  // Echec de connexion
  delay(1000);
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Please use Espressif");
  Heltec.display->drawString(0, 16, "application for BLE");
  Heltec.display->display();
  // UUID
  uint8_t uuid[16] = { 0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
                       0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02 };
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BLE, NETWORK_PROV_SECURITY_1, pop, service_name, service_key, uuid, reset_provisioned);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void loop() {
  WiFi.onEvent(CredsRecvEvent, ARDUINO_EVENT_PROV_CRED_RECV);
  // Si connexion au wifi
  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    // URL de la requete HTTP + clef API
    http.begin(URL + ApiKey);

    // Demarrage connexion
    int httpCode = http.GET();

    // Si erreur, httpcode est negatif
    if (httpCode > 0) {

      // Lecture des donnees comme une chaine json
      String JSON_Data = http.getString();
      displayWeather(JSON_Data);

    } else {
      Heltec.display->clear();
      Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
      Heltec.display->setFont(ArialMT_Plain_16);
      Heltec.display->drawString(64, 4, "Reconnexion");
      Heltec.display->drawString(64, 20, "au service météo");
      Heltec.display->drawString(64, 36, "Patientez...");
      Heltec.display->display();
      cycle = 0;  // On ne repasse pas en veille tant que nous sommes en erreur de requete http
      Serial.println(F("Erreur requete HTTP"));
    }

    http.end();  // liberation des ressources
  } else {
    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
    Heltec.display->setFont(ArialMT_Plain_16);
    Heltec.display->drawString(64, 4, "Reconnexion");
    Heltec.display->drawString(64, 20, "au Wifi");
    Heltec.display->drawString(64, 36, "Patientez...");
    Heltec.display->display();
    Serial.println("Erreur connexion Wifi");
    cycle = 0;  // On ne passe pas en veille tant que la connexion wifi n'est pas etablie
  }

  //Wait for 10 seconds
  delay(10000);
  cycle++;
  Serial.print("Cycle = ");
  Serial.println(cycle);
  if (cycle >= 6) {  // Declenchement de la veille au bout de la 6eme boucle
    cycle = 0;
    lightSleep();
  }
}