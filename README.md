# VISION AID — Aide visuelle temps réel sur puce
### ESP32-WROVER-E · Caméra OV2640 · Écran TFT ST7735

---

## À quoi sert ce projet ?

Imaginez un petit appareil que l'on peut tenir dans la main, ou fixer près de l'œil, et qui montre en temps réel ce que capte une caméra — mais avec l'image retravaillée, transformée, adaptée à la façon dont *vous* voyez le monde.

C'est exactement ce que fait **Vision Aid** : une caméra miniature filme ce qui est devant vous, et le résultat apparaît instantanément sur un petit écran, après avoir été traité par un ensemble de filtres que vous choisissez et ajustez vous-même. Contours renforcés, couleurs inversées, zoom numérique, conversion en noir et blanc très contrasté… chaque personne ayant une déficience visuelle étant différente, l'appareil laisse explorer librement ce qui fonctionne le mieux pour chaque situation et chaque regard.

Le projet s'inscrit dans la tradition des *loupes électroniques* et des *téléagrandisseurs* déjà disponibles dans le commerce — mais ici, tout est ouvert, modifiable, et fabriqué avec des composants grand public pour une fraction du coût.

---

## Comment l'image voyage, de la scène au regard

Voici ce qui se passe, de la lumière à l'écran, en moins d'un centième de seconde.

### 1. La scène réelle — la lumière du monde

Tout commence par la lumière qui rebondit sur les objets autour de vous : un texte à lire, un visage, un médicament à identifier, un trottoir à traverser. Cette lumière entre dans la caméra.

### 2. Le capteur OV2640 — la rétine artificielle

La **caméra OV2640** est un minuscule capteur photographique de la taille d'un ongle. Elle contient des millions de petits éléments photosensibles — les *pixels* — disposés en grille. Chaque pixel mesure la quantité de rouge, de vert et de bleu dans la lumière qui le frappe, et traduit cette mesure en un nombre.

Le résultat est une **image numérique** : une grille de nombres, chaque nombre décrivant la couleur d'un point précis de la scène. Sur ce montage, l'OV2640 produit des images de 176 × 144 points, renouvelées une dizaine à une vingtaine de fois par seconde — c'est ce qu'on appelle le *flux vidéo*.

> *Pour donner une idée de l'échelle : l'OV2640 fait environ 4 mm × 4 mm. La rétine humaine, qui accomplit un travail comparable, mesure environ 32 mm de diamètre.*

### 3. Le microcontrôleur ESP32-WROVER-E — le cerveau

Le **ESP32-WROVER-E** est une puce électronique dotée de deux processeurs cadencés à 240 MHz et de 4 Mo de mémoire vive supplémentaire (la *PSRAM*). Il reçoit l'image de la caméra des dizaines de fois par seconde et exécute sur chaque image le traitement choisi par l'utilisateur.

Concrètement, le microcontrôleur parcourt chaque pixel de l'image — jusqu'à 128 × 128 = 16 384 points à chaque cycle — et applique un calcul mathématique sur les valeurs de rouge, vert et bleu. Par exemple :

- Pour **inverser** l'image, il soustrait chaque valeur de 255 (le maximum). Ce qui était blanc devient noir, et inversement.
- Pour **renforcer les contours**, il compare chaque pixel avec ses voisins immédiats, et amplifie les différences brusques — là où une couleur change rapidement, ce qui correspond généralement à un bord, une arête, une lettre.
- Pour **appliquer une palette de couleurs**, il convertit chaque pixel en niveau de gris, puis lui réattribue une nouvelle couleur selon un tableau de correspondance — rouge pour les zones sombres, jaune pour les zones claires, par exemple.

Tout cela se produit en quelques millisecondes par image, en boucle continue.

### 4. L'écran TFT ST7735 — la fenêtre sur le monde retravaillé

L'**écran TFT ST7735** est un petit afficheur couleur de 1,8 pouce (environ 4,5 cm de diagonale), capable d'afficher 128 × 160 points en 65 000 couleurs différentes. Il est de la même famille que les écrans que l'on trouve dans les montres connectées ou les appareils photo numériques d'entrée de gamme.

Une fois que le microcontrôleur a fini de transformer l'image, il envoie les nouvelles valeurs de couleur à l'écran via un câble de données rapide (le bus *SPI*). L'écran reçoit ces données et allume chaque pixel dans la bonne couleur — et la nouvelle image apparaît.

Ce cycle — capturer, transformer, afficher — se répète en continu, produisant l'effet d'une vidéo fluide en temps réel.

### 5. L'utilisateur — au centre du système

Trois boutons permettent de naviguer entre les 26 modes de traitement disponibles et d'ajuster les paramètres de chaque mode (intensité du contraste, seuil de binarisation, niveau de zoom…). L'appareil ne décide rien tout seul : c'est toujours l'utilisateur qui choisit ce qui lui convient le mieux, en testant, en comparant, en affinant.

---

## Les équipements en détail

### La caméra : OV2640

| Caractéristique | Valeur |
|---|---|
| Fabricant | OmniVision |
| Résolution utilisée | 176 × 144 pixels (QCIF) |
| Format de couleur | RGB565 (5 bits rouge, 6 bits vert, 5 bits bleu) |
| Connexion | Bus parallèle 8 bits + signaux de synchronisation |
| Alimentation | 3,3 V |
| Taille physique | ≈ 4 mm × 4 mm |

L'OV2640 est intégrée directement sur la carte Freenove ESP32-WROVER, reliée au microcontrôleur par un connecteur flexible (FPC). Aucun câblage supplémentaire n'est nécessaire pour la caméra.

### Le cerveau : ESP32-WROVER-E

| Caractéristique | Valeur |
|---|---|
| Fabricant | Espressif Systems |
| Processeurs | 2 × Xtensa LX6, jusqu'à 240 MHz |
| Mémoire flash | 4 Mo (programme) |
| PSRAM | 4 Mo (données, buffers image) |
| Connectivité | WiFi 802.11 b/g/n, Bluetooth 4.2 |
| Alimentation | 3,3 V (via USB 5 V sur la carte) |
| Rôle dans le projet | Réception vidéo, traitement d'image, pilotage écran |

La PSRAM (mémoire externe sur la puce) est particulièrement importante ici : elle permet de stocker les buffers d'image sans saturer la mémoire interne du processeur, et donc de maintenir une cadence vidéo correcte.

### L'écran : TFT ST7735

| Caractéristique | Valeur |
|---|---|
| Contrôleur | Sitronix ST7735S |
| Diagonale | 1,8 pouce (≈ 4,5 cm) |
| Résolution | 128 × 160 pixels |
| Couleurs | 65 536 (16 bits, RGB565) |
| Interface | SPI (bus série synchrone) |
| Alimentation | 3,3 V |
| Rôle dans le projet | Affichage de l'image traitée + barre de statut |

Cet écran est volontairement petit et léger pour faciliter une utilisation portable ou montée sur monture. Sa taille réduite est aussi un avantage dans le cadre d'une aide visuelle : placé à quelques centimètres de l'œil, il occupe un champ visuel utile sans être encombrant.

---

## Pourquoi ce type de dispositif peut aider

Les déficiences visuelles sont très diverses. Quelqu'un atteint de **DMLA** (dégénérescence maculaire liée à l'âge) perd la vision centrale mais conserve la vision périphérique — un fort zoom et un contraste extrême peuvent lui permettre de déchiffrer un texte. Une personne **daltonienne** ne distingue pas certaines paires de couleurs, mais une recoloration artificielle de l'image peut rendre lisibles des informations qui lui étaient jusqu'alors invisibles. Quelqu'un souffrant de **photophobie** (hypersensibilité à la lumière) peut trouver dans le mode *négatif* — qui inverse les zones claires et sombres — un confort que n'offre aucun autre dispositif standard.

Aucun filtre unique ne convient à tout le monde. C'est pourquoi Vision Aid est conçu comme une *boîte à outils explorable* plutôt que comme un produit figé : l'enjeu est de permettre à chaque utilisateur de découvrir par l'expérience ce qui fonctionne pour son regard particulier.

---

## Guide d'utilisation rapide

L'appareil démarre automatiquement sur l'image brute (mode RAW). La barre en haut de l'écran indique le mode actif.

**Changer de mode** : appuyer sur `BTN_NEXT` (bouton BOOT sur la carte) ou `BTN_PREV` pour parcourir les 26 modes dans un sens ou dans l'autre.

**Ajuster un paramètre** : appuyer sur `BTN_PARAM` pour entrer en mode réglage (le paramètre s'affiche en jaune). `BTN_NEXT` et `BTN_PREV` font alors varier la valeur. Rappuyer sur `BTN_PARAM` pour revenir à la navigation entre modes.

**Modes conseillés pour commencer** selon le profil :

| Profil | Modes à essayer en premier |
|---|---|
| Malvoyance générale | HI-CONTRAST, CONTOURS+, ZOOM x2 |
| Photophobie | NÉGATIF, PAL GLACE |
| Daltonisme rouge-vert | DEUTAN, PAL CHAUD |
| Daltonisme bleu-jaune | PROTAN, PAL FROID |
| Vision nocturne réduite | NUIT, THERMIQUE |
| Lecture de texte | SEUIL, CROQUIS, ZOOM x4 |
| Faible sensibilité au contraste | CONTRASTE (valeur haute), NETTETÉ |

---

## Câblage (HSPI — évite conflits GPIO caméra)

```
ESP32 GPIO   →   TFT ST7735
─────────────────────────────
3.3V         →   VCC (pin 6)
GND          →   GND (pin 8)
GPIO 15      →   CS  (pin 2)
GPIO 16      →   RST (pin 1)
GPIO 17      →   D/C (pin 3)
GPIO 13      →   DIN (pin 4)   ← HSPI MOSI
GPIO 14      →   CLK (pin 5)   ← HSPI SCLK
3.3V (33Ω)   →   LED (pin 7)
```

### Boutons (vers GND, pullup interne)
```
GPIO 0   →  BTN_NEXT   (= bouton BOOT déjà sur la carte)
GPIO 5   →  BTN_PREV
GPIO 2   →  BTN_PARAM
```

---

## Navigation

| Bouton     | Mode normal        | Mode paramètre      |
|------------|--------------------|---------------------|
| BTN_NEXT   | Mode suivant →     | Paramètre + step    |
| BTN_PREV   | Mode précédent ←   | Paramètre - step    |
| BTN_PARAM  | Active mode param  | Désactive mode param|

La barre de statut affiche :
- Nom du mode (cyan)
- Paramètre actif (jaune si mode param, blanc sinon)
- Index mode (gris)

---

## Les 26 modes disponibles

### Modes de base
| # | Nom | Paramètre | Description |
|---|-----|-----------|-------------|
| 1 | RAW | — | Image brute OV2640 |
| 2 | LUMINOSITÉ | 0–100 | Décalage d'exposition |
| 3 | CONTRASTE | 0–100 | Contraste linéaire centré |
| 4 | GAMMA | 0.5–5.0 | Correction gamma |

### Accessibilité visuelle (malvoyance)
| # | Nom | Paramètre | Description |
|---|-----|-----------|-------------|
| 5 | HI-CONTRAST | seuil 50–100 | N&B extrême, contours nets |
| 6 | CONTOURS+ | force 1–20 | Laplacien : renforce les bords |
| 7 | CONTOURS | seuil 5–60 | Sobel : contours seuls sur noir |
| 8 | NÉGATIF | — | Inversion complète (photophobie) |
| 9 | GRIS | — | Niveaux de gris perceptuels |
| 10 | SEUIL | 10–245 | Binarisation noir/blanc |

### Palettes (daltonisme / amblyopie)
| # | Nom | Description |
|---|-----|-------------|
| 11 | PAL CHAUD | Renforce rouges/oranges (deutéranopie) |
| 12 | PAL FROID | Renforce bleus/cyans (protanopie) |
| 13 | PAL FEU | LUT noir→rouge→jaune→blanc |
| 14 | PAL GLACE | LUT noir→bleu→cyan→blanc |
| 15 | PAL NEON | Vert vif sur fond sombre (amblyopie) |
| 16 | DEUTAN | Compensation deutéranopie (rouge-vert) |
| 17 | PROTAN | Compensation protanopie (insensibilité rouge) |

### Zoom numérique
| # | Nom | Description |
|---|-----|-------------|
| 18 | ZOOM x2 | Centre 64×64 → écran 128×128 |
| 19 | ZOOM x4 | Centre 32×32 → écran 128×128 |

### Traitements créatifs
| # | Nom | Paramètre | Description |
|---|-----|-----------|-------------|
| 20 | POSTER | niveaux 2–8 | Postérisation |
| 21 | RELIEF | — | Emboss / relief |
| 22 | FLOU | — | Moyenne 3×3 (débruitage) |
| 23 | NETTETÉ | force 5–20 | Unsharp mask |
| 24 | NUIT | gain 20–100 | Amplification + teinte NV verte |
| 25 | CROQUIS | seuil 10–80 | Sobel inversé (crayon sur blanc) |
| 26 | THERMIQUE | — | Fausse couleur MATLAB "jet" |

---

## Architecture du code

```
main.cpp
├── Déclarations GPIO / dimensions
├── Utilitaires couleur (rgb565 ↔ rgb, luma, clamp8)
├── Tables précalculées (gamma, LUT palettes feu/glace)
├── apply*() — 1 fonction par traitement
├── processFrame() — dispatch selon currentMode
├── displayWorkBuf() — flush PSRAM → TFT via SPI
├── drawStatusBar() — barre d'état 14px
├── handleButtons() — debounce + navigation
├── initCamera() — config OV2640 QCIF RGB565
├── setup() / loop()
```

---

## Ajouter un nouveau mode

1. Ajouter une entrée dans `enum ProcessingMode` (avant `MODE_COUNT`)
2. Ajouter le nom dans `modeNames[]`
3. Ajouter les params dans `modeParams[]`
4. Écrire la fonction `applyMonNouveauMode()`
5. Ajouter le `case` dans `processFrame()`

---

## Performance attendue

| Config | FPS approx |
|--------|-----------|
| RAW / INVERT | 12–18 fps |
| Filtres pixels (gamma, contraste...) | 8–14 fps |
| Filtres spatiaux 3×3 (Sobel, blur...) | 4–8 fps |
| Zoom ×4 | 10–15 fps |

Le goulot d'étranglement est le bus SPI vers le TFT.
Pour améliorer : augmenter `SPI_FREQUENCY` dans Adafruit_ST7735 (max ~40 MHz pour ce module).

---

## Dépendances PlatformIO

```
adafruit/Adafruit ST7735 and ST7789 Library @ ^1.10.4
adafruit/Adafruit GFX Library @ ^1.11.9
esp32-camera (inclus dans le board package Espressif)
```
