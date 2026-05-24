# VISION AID — Machine à bidouiller l'image temps réel
### ESP32-WROVER-E + OV2640 + TFT ST7735 128×160

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

| Bouton     | Mode normal        | Mode paramètre     |
|------------|--------------------|--------------------|
| BTN_NEXT   | Mode suivant →     | Paramètre + step   |
| BTN_PREV   | Mode précédent ←   | Paramètre - step   |
| BTN_PARAM  | Active mode param  | Désactive mode param|

La barre de status affiche :
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
