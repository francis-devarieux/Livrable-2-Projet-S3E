# Livrable-2-Projet-S3E
Début du livrable 2<br>
# Livrable 2 – Architecture du programme (structure)

## Objectif
Présenter la structure générale du programme embarqué (modules, fonctions, variables principales).
Ce livrable décrit l’organisation du code, sans fournir un programme finalisé.

---

## Rappels des modes (d’après le cahier des charges)
- **Standard** : acquisition périodique + enregistrement sur SD (LOG_INTERVAL, TIMEOUT, FILE_MAX_SIZE).
- **Économie** : accessible depuis Standard (bouton vert 5s), LOG_INTERVAL x2, GPS une mesure sur deux.
- **Maintenance** : accessible depuis Standard ou Économie (bouton rouge 5s), pas d’écriture SD, données consultables via série, retrait SD sécurisé. Sortie : bouton rouge 5s → retour au mode précédent.
- **Configuration** : au démarrage bouton rouge appuyé, acquisition désactivée, configuration via série, retour en Standard au bout de 30 min sans activité.

---

## Découpage général
Le programme est découpé en 3 blocs principaux :

1) **Gestion des modes**
- transitions Standard / Économie / Maintenance / Configuration
- règles d’entrée/sortie (boutons, temporisations, retour mode précédent)

2) **Gestion des composants**
- capteurs + GPS (acquisition, gestion TIMEOUT, valeurs NA)
- carte SD (fichier log, gestion taille, archivage)
- interface série (configuration et maintenance)
- boutons (appui long/court)
- LED (couleur selon mode et/ou erreurs)

3) **Gestion des erreurs**
- erreurs SD / capteurs (timeout) / GPS
- signalisation par LED (une LED multicolore)

---

## Organisation des fichiers (proposée)


/src  
│   
├── main.c   
│   
├── mode_manager.c   
├── mode_manager.h   
│   
├── buttons.c   
├── buttons.h   
│   
├── led.c   
├── led.h  
│    
├── sensors.c  
├── sensors.h  
│   
├── gps.c   
├── gps.h   
│   
├── sd_manager.c   
├── sd_manager.h   
│   
├── serial_console.c   
├── serial_console.h  
│   
├── config_params.c   
├── config_params.h     
  
> Remarque : si certains éléments sont regroupés (ex: GPS dans sensors), la structure reste identique au niveau logique.

---

## Fonctions principales (liste)

### main.c
- `void system_init(void);`
- `void system_run(void);` *(boucle principale, appelle les modules ci-dessous)*

### mode_manager.c
- `void mode_init(void);`
- `void mode_update(void);`
- `void mode_enter_standard(void);`
- `void mode_enter_eco(void);`
- `void mode_enter_maintenance(void);`
- `void mode_enter_config(void);`

### buttons.c
- `void buttons_init(void);`
- `void buttons_update(void);`
- `int button_is_pressed_long(int button_id, unsigned long duration_ms);`

> Boutons utilisés : rouge, vert (principaux), bleu/blanc (auxiliaires/debug si besoin).

### led.c (mode + erreurs)
- `void led_init(void);`
- `void led_set_mode_color(int mode);`
- `void led_set_error(int error_code);`
- `void led_update(void);`

### sensors.c
- `void sensors_init(void);`
- `int sensors_read_all(void);` *(retourne OK/KO, gère TIMEOUT et NA)*

### gps.c
- `void gps_init(void);`
- `int gps_read(void);`
- `int gps_should_read_this_cycle(void);` *(utile en mode Économie : 1 mesure sur 2)*

### sd_manager.c
- `void sd_init(void);`
- `int sd_write_log_line(void);`
- `int sd_rotate_or_archive_if_full(void);`
- `void sd_safe_eject_prepare(void);` *(utile en maintenance)*

### serial_console.c
- `void serial_init(void);`
- `void serial_update(void);`
- `void serial_print_live_measures(void);` *(maintenance)*
- `void serial_handle_config_commands(void);` *(configuration)*

### config_params.c
- `void config_load_from_eeprom(void);`
- `void config_save_to_eeprom(void);`
- `void config_reset_defaults(void);`
- `void config_print_version(void);`
- `int  config_set_param(const char* key, const char* value);`
