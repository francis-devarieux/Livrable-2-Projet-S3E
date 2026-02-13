//Definition des structures 
typedef struct {
    char date[11];      // AAAA/MM/JJ
    char heure[9];      // HH:MM:SS
    float pression;
    float temperature;
    float humidite;
    int luminosite;
    float latitude;
    float longitude;
} Mesure;

typedef struct {
    bool pressionActif;
    bool temperatureActif;
    bool humiditeActif;
    bool luminositeActif;
    bool gpsActif;
} Capteurs;

typedef struct {
    bool rtc;
    bool gps;
    bool capteur;
    bool sd;
    bool sdPleine;
} Erreurs;

Acquisition horloge RTC 
unsigned long dernierTempsAcquisition;
unsigned long tempsActuel;

typedef struct {
    unsigned long periodeAcquisition_ms;
    bool sauvegardeActive;
    bool acquisitionAutomatique;
} Configuration;

typedef struct {
    ModeSysteme modeCourant;
    Capteurs capteurs;
    Erreurs erreurs;
    Configuration config;
    Mesure mesureCourante;
} Systeme;
