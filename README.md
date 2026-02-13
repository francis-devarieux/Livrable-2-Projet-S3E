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
