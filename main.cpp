/**
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║         VISION AID — Machine à bidouiller l'image temps réel    ║
 * ║         ESP32-WROVER-E + OV2640 + TFT ST7735 128×160            ║
 * ║                                                                  ║
 * ║  Navigation :  BTN_NEXT  (GPIO 0)  → mode suivant               ║
 * ║                BTN_PREV  (GPIO 17) → mode précédent             ║
 * ║                BTN_PARAM (GPIO 16) → ajuster paramètre courant  ║
 * ╚══════════════════════════════════════════════════════════════════╝
 */

#include <Arduino.h>
#include "esp_camera.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ─── Broches TFT (HSPI — évite conflits caméra) ──────────────────────────────
#define TFT_CS   15
#define TFT_DC   17
#define TFT_RST  16
#define TFT_MOSI 13
#define TFT_SCLK 14

// ─── Broches boutons ──────────────────────────────────────────────────────────
#define BTN_NEXT   0   // BOOT button déjà présent sur la carte
#define BTN_PREV   5
#define BTN_PARAM  2

// ─── Broches caméra OV2640 (Freenove ESP32-WROVER-E) ─────────────────────────
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK     0   // ← vérifier selon ta version de carte
#define CAM_PIN_SIOD    21
#define CAM_PIN_SIOC    22
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      19
#define CAM_PIN_D2      18
#define CAM_PIN_D1       5   // ← conflit avec BTN_PREV si utilisé
#define CAM_PIN_D0       4
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    26

// ─── Dimensions ──────────────────────────────────────────────────────────────
#define SCREEN_W   128
#define SCREEN_H   160
#define CAM_W      176   // QCIF
#define CAM_H      144
// Zone d'affichage : 128×128 centré dans les 128×160
#define DST_W      128
#define DST_H      128
#define DST_Y_OFF   16   // marge haute pour la barre de status
#define X_CROP_OFF  24   // (176-128)/2
#define Y_CROP_OFF   8   // (144-128)/2

// ─── TFT ─────────────────────────────────────────────────────────────────────
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ─── Buffer de travail en PSRAM ───────────────────────────────────────────────
uint16_t* workBuf = nullptr;   // 128×128 pixels RGB565

// ══════════════════════════════════════════════════════════════════════════════
//  MODES DE TRAITEMENT
// ══════════════════════════════════════════════════════════════════════════════

enum ProcessingMode {
  // ── Modes de base ──────────────────────────────────────────────
  MODE_RAW = 0,          // Image brute, aucun traitement
  MODE_BRIGHTNESS,       // Luminosité ajustable
  MODE_CONTRAST,         // Contraste ajustable
  MODE_GAMMA,            // Correction gamma (utile : basse vision)

  // ── Accessibilité visuelle ──────────────────────────────────────
  MODE_HIGH_CONTRAST,    // Contraste extrême N&B — contours nets
  MODE_EDGE_ENHANCE,     // Renforcement des contours (Laplacien)
  MODE_EDGES_ONLY,       // Détection de contours seuls (Sobel simplifié)
  MODE_INVERT,           // Négatif — utile photophobie / albinisme
  MODE_GRAYSCALE,        // Niveaux de gris
  MODE_THRESHOLD,        // Binarisation seuil adaptable

  // ── Palettes pour déficiences chromatiques ──────────────────────
  MODE_PALETTE_WARM,     // Warm (rouge/orange) → daltonisme deutan
  MODE_PALETTE_COOL,     // Cool (bleu/cyan) → daltonisme protan
  MODE_PALETTE_FIRE,     // Feu (noir→rouge→jaune→blanc)
  MODE_PALETTE_ICE,      // Glace (noir→bleu→cyan→blanc)
  MODE_PALETTE_NEON,     // Vert fluo sur noir → amblyopie
  MODE_DEUTERANOPIA,     // Simulation + compensation deutéranopie
  MODE_PROTANOPIA,       // Simulation + compensation protanopie

  // ── Zoom numérique ───────────────────────────────────────────────
  MODE_ZOOM_2X,          // Zoom ×2 — quart central
  MODE_ZOOM_4X,          // Zoom ×4 — seizième central

  // ── Traitements créatifs / expérimentaux ─────────────────────────
  MODE_POSTERIZE,        // Postérisation (réduction niveaux)
  MODE_EMBOSS,           // Relief (emboss)
  MODE_BLUR,             // Flou 3×3 (réduction bruit)
  MODE_SHARPEN,          // Netteté (filtre unsharp mask)
  MODE_NIGHT,            // Mode nuit : amplification + dénoise
  MODE_SKETCH,           // Rendu crayon/croquis
  MODE_THERMAL,          // Fausse couleur thermique

  MODE_COUNT             // Toujours en dernier — compteur
};

// Noms courts pour l'affichage (max ~14 chars sur TFT)
const char* modeNames[] = {
  "RAW",
  "LUMINOSITE",
  "CONTRASTE",
  "GAMMA",
  "HI-CONTRAST",
  "CONTOURS+",
  "CONTOURS",
  "NEGATIF",
  "GRIS",
  "SEUIL",
  "PAL CHAUD",
  "PAL FROID",
  "PAL FEU",
  "PAL GLACE",
  "PAL NEON",
  "DEUTAN",
  "PROTAN",
  "ZOOM x2",
  "ZOOM x4",
  "POSTER",
  "RELIEF",
  "FLOU",
  "NETTETE",
  "NUIT",
  "CROQUIS",
  "THERMIQUE"
};

// ─── Paramètre ajustable par mode ────────────────────────────────────────────
struct ModeParam {
  const char* label;
  int value;
  int minVal;
  int maxVal;
  int step;
};

ModeParam modeParams[] = {
  {"",          0,    0,   0,   0},   // RAW
  {"LUMIN",    50,    0, 100,   5},   // BRIGHTNESS  (0=sombre, 100=max)
  {"CONTR",    50,    0, 100,   5},   // CONTRAST
  {"GAMMA",    22,    5,  50,   3},   // GAMMA ×10 (2.2 défaut)
  {"FORCE",    80,   50, 100,   5},   // HIGH_CONTRAST seuil
  {"FORCE",     8,   1,  20,   1},    // EDGE_ENHANCE force
  {"SEUIL",    30,   5,  60,   5},    // EDGES_ONLY seuil magnitude
  {"",          0,   0,   0,   0},    // INVERT
  {"",          0,   0,   0,   0},    // GRAYSCALE
  {"SEUIL",   128,  10, 245,   5},    // THRESHOLD
  {"SATURAT",  80,  20, 100,  10},    // PALETTE_WARM
  {"SATURAT",  80,  20, 100,  10},    // PALETTE_COOL
  {"",          0,   0,   0,   0},    // PALETTE_FIRE
  {"",          0,   0,   0,   0},    // PALETTE_ICE
  {"INTENS",   90,  50, 100,  10},    // PALETTE_NEON
  {"CORR",     70,  30, 100,  10},    // DEUTERANOPIA
  {"CORR",     70,  30, 100,  10},    // PROTANOPIA
  {"",          0,   0,   0,   0},    // ZOOM_2X
  {"",          0,   0,   0,   0},    // ZOOM_4X
  {"NIVEAUX",   4,   2,   8,   1},    // POSTERIZE
  {"",          0,   0,   0,   0},    // EMBOSS
  {"RAYON",     1,   1,   3,   1},    // BLUR
  {"FORCE",    10,   5,  20,   5},    // SHARPEN
  {"GAIN",     60,  20, 100,  10},    // NIGHT
  {"SEUIL",    40,  10,  80,   5},    // SKETCH
  {"",          0,   0,   0,   0},    // THERMAL
};

// ─── État global ─────────────────────────────────────────────────────────────
int  currentMode   = MODE_RAW;
bool paramMode     = false;
unsigned long lastBtn = 0;
#define BTN_DEBOUNCE 200

// ══════════════════════════════════════════════════════════════════════════════
//  UTILITAIRES COULEUR
// ══════════════════════════════════════════════════════════════════════════════

// RGB565 → composantes 0–255
inline void rgb565_to_rgb(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = (c >> 11) & 0x1F; r = (r << 3) | (r >> 2);
  g = (c >>  5) & 0x3F; g = (g << 2) | (g >> 4);
  b = (c >>  0) & 0x1F; b = (b << 3) | (b >> 2);
}

inline uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

inline uint8_t clamp8(int v) {
  return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// Luminance perceptuelle Y = 0.299R + 0.587G + 0.114B (entiers)
inline uint8_t luma(uint8_t r, uint8_t g, uint8_t b) {
  return (uint8_t)((77 * r + 150 * g + 29 * b) >> 8);
}

// Table gamma précalculée
uint8_t gammaTable[256];
void buildGammaTable(float gamma) {
  for (int i = 0; i < 256; i++) {
    gammaTable[i] = (uint8_t)(powf(i / 255.0f, gamma) * 255.0f + 0.5f);
  }
}

// Palette feu : index 0–255 → RGB565
uint16_t palFire[256];
uint16_t palIce[256];
void buildPalettes() {
  for (int i = 0; i < 256; i++) {
    // ── Feu : noir→rouge→orange→jaune→blanc
    uint8_t r, g, b;
    if (i < 85) {
      r = clamp8(i * 3); g = 0; b = 0;
    } else if (i < 170) {
      r = 255; g = clamp8((i - 85) * 3); b = 0;
    } else {
      r = 255; g = 255; b = clamp8((i - 170) * 3);
    }
    palFire[i] = rgb_to_rgb565(r, g, b);

    // ── Glace : noir→bleu→cyan→blanc
    if (i < 85) {
      r = 0; g = 0; b = clamp8(i * 3);
    } else if (i < 170) {
      r = 0; g = clamp8((i - 85) * 3); b = 255;
    } else {
      r = clamp8((i - 170) * 3); g = 255; b = 255;
    }
    palIce[i] = rgb_to_rgb565(r, g, b);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  PIPELINE DE TRAITEMENT
// ══════════════════════════════════════════════════════════════════════════════

/**
 * Copie + recadrage de la frame caméra dans workBuf.
 * Gère le swap d'octets OV2640 (big-endian → little-endian).
 */
void copyFrameToWorkBuf(camera_fb_t* fb) {
  const uint16_t* src = (const uint16_t*)fb->buf;
  for (int y = 0; y < DST_H; y++) {
    const uint16_t* row = src + (y + Y_CROP_OFF) * CAM_W + X_CROP_OFF;
    for (int x = 0; x < DST_W; x++) {
      uint16_t px = row[x];
      workBuf[y * DST_W + x] = (px >> 8) | (px << 8); // swap endian
    }
  }
}

// ── Zoom ──────────────────────────────────────────────────────────────────────
void applyZoom(camera_fb_t* fb, int factor) {
  const uint16_t* src = (const uint16_t*)fb->buf;
  // Centre de la source
  int cx = CAM_W / 2;
  int cy = CAM_H / 2;
  int half = DST_W / (2 * factor);
  int x0 = cx - half;
  int y0 = cy - half;

  for (int y = 0; y < DST_H; y++) {
    for (int x = 0; x < DST_W; x++) {
      int sx = x0 + x / factor;
      int sy = y0 + y / factor;
      sx = constrain(sx, 0, CAM_W - 1);
      sy = constrain(sy, 0, CAM_H - 1);
      uint16_t px = src[sy * CAM_W + sx];
      workBuf[y * DST_W + x] = (px >> 8) | (px << 8);
    }
  }
}

// ── Luminosité ────────────────────────────────────────────────────────────────
void applyBrightness(int param) {
  int delta = (param - 50) * 2;  // -100..+100
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    r = clamp8(r + delta);
    g = clamp8(g + delta);
    b = clamp8(b + delta);
    workBuf[i] = rgb_to_rgb565(r, g, b);
  }
}

// ── Contraste ────────────────────────────────────────────────────────────────
void applyContrast(int param) {
  // factor 0→0.0 / 50→1.0 / 100→3.0
  float f = 0.02f * param;
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    r = clamp8((int)((r - 128) * f + 128));
    g = clamp8((int)((g - 128) * f + 128));
    b = clamp8((int)((b - 128) * f + 128));
    workBuf[i] = rgb_to_rgb565(r, g, b);
  }
}

// ── Gamma ────────────────────────────────────────────────────────────────────
void applyGamma() {
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    workBuf[i] = rgb_to_rgb565(gammaTable[r], gammaTable[g], gammaTable[b]);
  }
}

// ── Niveaux de gris ──────────────────────────────────────────────────────────
void applyGrayscale() {
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    uint8_t y = luma(r, g, b);
    workBuf[i] = rgb_to_rgb565(y, y, y);
  }
}

// ── Négatif ──────────────────────────────────────────────────────────────────
void applyInvert() {
  for (int i = 0; i < DST_W * DST_H; i++) {
    workBuf[i] = ~workBuf[i];
  }
}

// ── Binarisation ─────────────────────────────────────────────────────────────
void applyThreshold(int threshold) {
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    uint8_t y = luma(r, g, b);
    workBuf[i] = y >= threshold ? 0xFFFF : 0x0000;
  }
}

// ── Contraste extrême N&B ────────────────────────────────────────────────────
void applyHighContrast(int threshold) {
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    uint8_t y = luma(r, g, b);
    uint8_t out = y > threshold ? 255 : 0;
    workBuf[i] = rgb_to_rgb565(out, out, out);
  }
}

// ── Filtre Laplacien (renforcement contours) ──────────────────────────────────
// Kernel : [0,-1,0 / -1,5,-1 / 0,-1,0] * force/8
void applyEdgeEnhance(int force) {
  // Travaille sur luminance dans un buf temporaire
  static uint8_t tmp[DST_W * DST_H];
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    tmp[i] = luma(r, g, b);
  }
  for (int y = 1; y < DST_H - 1; y++) {
    for (int x = 1; x < DST_W - 1; x++) {
      int idx = y * DST_W + x;
      int c  =  tmp[idx];
      int n  =  tmp[(y-1)*DST_W + x];
      int s  =  tmp[(y+1)*DST_W + x];
      int w  =  tmp[y*DST_W + (x-1)];
      int e  =  tmp[y*DST_W + (x+1)];
      int sharpened = c + force * (4*c - n - s - w - e) / 8;
      uint8_t out = clamp8(sharpened);
      workBuf[idx] = rgb_to_rgb565(out, out, out);
    }
  }
}

// ── Croquis / Sketch (contours sur fond blanc) ────────────────────────────────
void applySketch(int threshold) {
  static uint8_t tmp[DST_W * DST_H];
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    tmp[i] = luma(r, g, b);
  }
  for (int y = 1; y < DST_H - 1; y++) {
    for (int x = 1; x < DST_W - 1; x++) {
      int idx = y * DST_W + x;
      // Sobel 3×3
      int gx = -tmp[(y-1)*DST_W+(x-1)] + tmp[(y-1)*DST_W+(x+1)]
               -2*tmp[y*DST_W+(x-1)]   + 2*tmp[y*DST_W+(x+1)]
               -tmp[(y+1)*DST_W+(x-1)] + tmp[(y+1)*DST_W+(x+1)];
      int gy = -tmp[(y-1)*DST_W+(x-1)] - 2*tmp[(y-1)*DST_W+x] - tmp[(y-1)*DST_W+(x+1)]
               +tmp[(y+1)*DST_W+(x-1)] + 2*tmp[(y+1)*DST_W+x] + tmp[(y+1)*DST_W+(x+1)];
      int mag = abs(gx) + abs(gy);
      uint8_t out = mag > threshold ? 0 : 255;
      workBuf[idx] = rgb_to_rgb565(out, out, out);
    }
  }
}

// ── Sobel (contours seuls sur fond noir) ──────────────────────────────────────
void applyEdgesOnly(int threshold) {
  static uint8_t tmp[DST_W * DST_H];
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    tmp[i] = luma(r, g, b);
  }
  for (int y = 1; y < DST_H - 1; y++) {
    for (int x = 1; x < DST_W - 1; x++) {
      int idx = y * DST_W + x;
      int gx = -tmp[(y-1)*DST_W+(x-1)] + tmp[(y-1)*DST_W+(x+1)]
               -2*tmp[y*DST_W+(x-1)]   + 2*tmp[y*DST_W+(x+1)]
               -tmp[(y+1)*DST_W+(x-1)] + tmp[(y+1)*DST_W+(x+1)];
      int gy = -tmp[(y-1)*DST_W+(x-1)] - 2*tmp[(y-1)*DST_W+x] - tmp[(y-1)*DST_W+(x+1)]
               +tmp[(y+1)*DST_W+(x-1)] + 2*tmp[(y+1)*DST_W+x] + tmp[(y+1)*DST_W+(x+1)];
      int mag = abs(gx) + abs(gy);
      mag = constrain(mag * 2, 0, 255);
      uint8_t out = mag > threshold ? clamp8(mag) : 0;
      workBuf[idx] = rgb_to_rgb565(out, out, out);
    }
  }
}

// ── Relief / Emboss ───────────────────────────────────────────────────────────
void applyEmboss() {
  static uint8_t tmp[DST_W * DST_H];
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    tmp[i] = luma(r, g, b);
  }
  for (int y = 1; y < DST_H - 1; y++) {
    for (int x = 1; x < DST_W - 1; x++) {
      int idx = y * DST_W + x;
      int v = 128
              - (int)tmp[(y-1)*DST_W+(x-1)]
              + (int)tmp[(y+1)*DST_W+(x+1)];
      uint8_t out = clamp8(v);
      workBuf[idx] = rgb_to_rgb565(out, out, out);
    }
  }
}

// ── Flou 3×3 ─────────────────────────────────────────────────────────────────
void applyBlur() {
  static uint16_t tmp[DST_W * DST_H];
  memcpy(tmp, workBuf, DST_W * DST_H * 2);
  for (int y = 1; y < DST_H - 1; y++) {
    for (int x = 1; x < DST_W - 1; x++) {
      int sr = 0, sg = 0, sb = 0;
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          uint8_t r, g, b;
          rgb565_to_rgb(tmp[(y+dy)*DST_W+(x+dx)], r, g, b);
          sr += r; sg += g; sb += b;
        }
      }
      workBuf[y*DST_W+x] = rgb_to_rgb565(sr/9, sg/9, sb/9);
    }
  }
}

// ── Netteté (Unsharp Mask) ────────────────────────────────────────────────────
void applySharpen(int force) {
  static uint16_t tmp[DST_W * DST_H];
  memcpy(tmp, workBuf, DST_W * DST_H * 2);
  for (int y = 1; y < DST_H - 1; y++) {
    for (int x = 1; x < DST_W - 1; x++) {
      uint8_t r0, g0, b0;
      rgb565_to_rgb(tmp[y*DST_W+x], r0, g0, b0);
      int sr = 0, sg = 0, sb = 0;
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          uint8_t r, g, b;
          rgb565_to_rgb(tmp[(y+dy)*DST_W+(x+dx)], r, g, b);
          sr += r; sg += g; sb += b;
        }
      }
      int fr = (int)r0 + force * ((int)r0 - sr/9) / 8;
      int fg = (int)g0 + force * ((int)g0 - sg/9) / 8;
      int fb = (int)b0 + force * ((int)b0 - sb/9) / 8;
      workBuf[y*DST_W+x] = rgb_to_rgb565(clamp8(fr), clamp8(fg), clamp8(fb));
    }
  }
}

// ── Postérisation ─────────────────────────────────────────────────────────────
void applyPosterize(int levels) {
  int step = 256 / levels;
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    r = (r / step) * step;
    g = (g / step) * step;
    b = (b / step) * step;
    workBuf[i] = rgb_to_rgb565(r, g, b);
  }
}

// ── Mode Nuit (amplification + clamping doux) ─────────────────────────────────
void applyNight(int gain) {
  float f = 0.5f + gain / 50.0f;  // 1.0 → 2.5
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    // Amplification + légère teinte verte pour ambiance NV
    uint8_t y = luma(r, g, b);
    int ny = (int)(y * f);
    workBuf[i] = rgb_to_rgb565(
      clamp8((int)(ny * 0.7f)),
      clamp8(ny),
      clamp8((int)(ny * 0.7f))
    );
  }
}

// ── Palette chaude ────────────────────────────────────────────────────────────
void applyPaletteWarm(int sat) {
  float s = sat / 100.0f;
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    uint8_t y = luma(r, g, b);
    // Mélange gris + couleur chaude
    workBuf[i] = rgb_to_rgb565(
      clamp8(y + (int)(s * (r - y) * 1.5f)),
      clamp8(y + (int)(s * (g - y) * 0.8f)),
      clamp8(y + (int)(s * (b - y) * 0.3f))
    );
  }
}

// ── Palette froide ────────────────────────────────────────────────────────────
void applyPaletteCool(int sat) {
  float s = sat / 100.0f;
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    uint8_t y = luma(r, g, b);
    workBuf[i] = rgb_to_rgb565(
      clamp8(y + (int)(s * (r - y) * 0.3f)),
      clamp8(y + (int)(s * (g - y) * 0.9f)),
      clamp8(y + (int)(s * (b - y) * 1.6f))
    );
  }
}

// ── Palette Neon (vert vif sur fond sombre) ───────────────────────────────────
void applyPaletteNeon(int intens) {
  float f = intens / 100.0f;
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    uint8_t y = luma(r, g, b);
    workBuf[i] = rgb_to_rgb565(0, clamp8((int)(y * f)), 0);
  }
}

// ── Palette Feu / Glace via LUT ───────────────────────────────────────────────
void applyPaletteLUT(uint16_t* lut) {
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    workBuf[i] = lut[luma(r, g, b)];
  }
}

// ── Fausse couleur thermique ──────────────────────────────────────────────────
// Noir→bleu→vert→jaune→rouge→blanc (MATLAB "jet" simplifié)
void applyThermal() {
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    uint8_t v = luma(r, g, b);
    uint8_t tr, tg, tb;
    if (v < 64) {
      tr = 0; tg = 0; tb = clamp8(v * 4);
    } else if (v < 128) {
      tr = 0; tg = clamp8((v - 64) * 4); tb = 255 - clamp8((v - 64) * 4);
    } else if (v < 192) {
      tr = clamp8((v - 128) * 4); tg = 255; tb = 0;
    } else {
      tr = 255; tg = 255 - clamp8((v - 192) * 4); tb = 0;
    }
    workBuf[i] = rgb_to_rgb565(tr, tg, tb);
  }
}

// ── Compensation deutéranopie (rouge-vert) ────────────────────────────────────
// Redistribution partielle du canal vert vers rouge
void applyDeuteranopia(int corr) {
  float c = corr / 100.0f;
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    uint8_t nr = clamp8(r + (int)(g * c * 0.4f));
    uint8_t nb = clamp8(b + (int)(g * c * 0.3f));
    workBuf[i] = rgb_to_rgb565(nr, g, nb);
  }
}

// ── Compensation protanopie (insensibilité rouge) ─────────────────────────────
void applyProtanopia(int corr) {
  float c = corr / 100.0f;
  for (int i = 0; i < DST_W * DST_H; i++) {
    uint8_t r, g, b;
    rgb565_to_rgb(workBuf[i], r, g, b);
    uint8_t nr = clamp8((int)(r + g * c * 0.5f));
    workBuf[i] = rgb_to_rgb565(nr, g, b);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  DISPATCH DU PIPELINE
// ══════════════════════════════════════════════════════════════════════════════

void processFrame(camera_fb_t* fb) {
  int p = modeParams[currentMode].value;

  // Zoom : copier depuis fb directement
  if (currentMode == MODE_ZOOM_2X) { applyZoom(fb, 2); return; }
  if (currentMode == MODE_ZOOM_4X) { applyZoom(fb, 4); return; }

  // Copie standard
  copyFrameToWorkBuf(fb);

  switch (currentMode) {
    case MODE_RAW:           break;
    case MODE_BRIGHTNESS:    applyBrightness(p);   break;
    case MODE_CONTRAST:      applyContrast(p);     break;
    case MODE_GAMMA:
      buildGammaTable(p / 10.0f);
      applyGamma();
      break;
    case MODE_HIGH_CONTRAST: applyHighContrast(p); break;
    case MODE_EDGE_ENHANCE:  applyEdgeEnhance(p);  break;
    case MODE_EDGES_ONLY:    applyEdgesOnly(p);    break;
    case MODE_INVERT:        applyInvert();        break;
    case MODE_GRAYSCALE:     applyGrayscale();     break;
    case MODE_THRESHOLD:     applyThreshold(p);   break;
    case MODE_PALETTE_WARM:  applyPaletteWarm(p); break;
    case MODE_PALETTE_COOL:  applyPaletteCool(p); break;
    case MODE_PALETTE_FIRE:  applyPaletteLUT(palFire); break;
    case MODE_PALETTE_ICE:   applyPaletteLUT(palIce);  break;
    case MODE_PALETTE_NEON:  applyPaletteNeon(p); break;
    case MODE_DEUTERANOPIA:  applyDeuteranopia(p); break;
    case MODE_PROTANOPIA:    applyProtanopia(p);  break;
    case MODE_POSTERIZE:     applyPosterize(p);   break;
    case MODE_EMBOSS:        applyEmboss();       break;
    case MODE_BLUR:          applyBlur();         break;
    case MODE_SHARPEN:       applySharpen(p);     break;
    case MODE_NIGHT:         applyNight(p);       break;
    case MODE_SKETCH:        applySketch(p);      break;
    case MODE_THERMAL:       applyThermal();      break;
    default: break;
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  AFFICHAGE
// ══════════════════════════════════════════════════════════════════════════════

void drawStatusBar() {
  tft.fillRect(0, 0, SCREEN_W, 14, ST77XX_BLACK);
  tft.setTextSize(1);

  // Indicateur mode
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(2, 3);
  tft.print(modeNames[currentMode]);

  // Paramètre courant (si applicable)
  ModeParam& mp = modeParams[currentMode];
  if (mp.maxVal > mp.minVal) {
    tft.setTextColor(paramMode ? ST77XX_YELLOW : ST77XX_WHITE);
    char buf[12];
    snprintf(buf, sizeof(buf), " %s:%d", mp.label, mp.value);
    tft.print(buf);
  }

  // Numéro de mode
  tft.setTextColor(0x4228);  // gris sombre
  char idx[6];
  snprintf(idx, sizeof(idx), "%d/%d", currentMode + 1, (int)MODE_COUNT);
  tft.setCursor(SCREEN_W - 30, 3);
  tft.print(idx);

  // Ligne de séparation
  tft.drawFastHLine(0, 14, SCREEN_W, 0x4228);
}

void displayWorkBuf() {
  tft.startWrite();
  tft.setAddrWindow(0, DST_Y_OFF, DST_W - 1, DST_Y_OFF + DST_H - 1);
  for (int i = 0; i < DST_W * DST_H; i++) {
    tft.pushColor(workBuf[i]);
  }
  tft.endWrite();
}

// ══════════════════════════════════════════════════════════════════════════════
//  GESTION BOUTONS
// ══════════════════════════════════════════════════════════════════════════════

void handleButtons() {
  if (millis() - lastBtn < BTN_DEBOUNCE) return;

  bool changed = false;

  if (digitalRead(BTN_NEXT) == LOW) {
    if (paramMode && modeParams[currentMode].maxVal > modeParams[currentMode].minVal) {
      modeParams[currentMode].value = min(
        modeParams[currentMode].value + modeParams[currentMode].step,
        modeParams[currentMode].maxVal
      );
    } else {
      currentMode = (currentMode + 1) % MODE_COUNT;
    }
    changed = true;
  }

  if (digitalRead(BTN_PREV) == LOW) {
    if (paramMode && modeParams[currentMode].maxVal > modeParams[currentMode].minVal) {
      modeParams[currentMode].value = max(
        modeParams[currentMode].value - modeParams[currentMode].step,
        modeParams[currentMode].minVal
      );
    } else {
      currentMode = (currentMode - 1 + MODE_COUNT) % MODE_COUNT;
    }
    changed = true;
  }

  if (digitalRead(BTN_PARAM) == LOW) {
    paramMode = !paramMode;
    changed = true;
  }

  if (changed) lastBtn = millis();
}

// ══════════════════════════════════════════════════════════════════════════════
//  INIT CAMÉRA
// ══════════════════════════════════════════════════════════════════════════════

bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = CAM_PIN_D0;
  config.pin_d1       = CAM_PIN_D1;
  config.pin_d2       = CAM_PIN_D2;
  config.pin_d3       = CAM_PIN_D3;
  config.pin_d4       = CAM_PIN_D4;
  config.pin_d5       = CAM_PIN_D5;
  config.pin_d6       = CAM_PIN_D6;
  config.pin_d7       = CAM_PIN_D7;
  config.pin_xclk     = CAM_PIN_XCLK;
  config.pin_pclk     = CAM_PIN_PCLK;
  config.pin_vsync    = CAM_PIN_VSYNC;
  config.pin_href     = CAM_PIN_HREF;
  config.pin_sscb_sda = CAM_PIN_SIOD;
  config.pin_sscb_scl = CAM_PIN_SIOC;
  config.pin_pwdn     = CAM_PIN_PWDN;
  config.pin_reset    = CAM_PIN_RESET;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_QCIF;  // 176×144
  config.jpeg_quality = 10;
  config.fb_count     = 2;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init failed: 0x%x\n", err);
    return false;
  }

  // Réglages capteur OV2640
  sensor_t* s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QCIF);
  s->set_quality(s, 10);
  s->set_whitebal(s, 1);    // AWB on
  s->set_awb_gain(s, 1);
  s->set_exposure_ctrl(s, 1); // AEC on
  s->set_aec2(s, 1);

  return true;
}

// ══════════════════════════════════════════════════════════════════════════════
//  SETUP & LOOP
// ══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);

  // Boutons
  pinMode(BTN_NEXT,  INPUT_PULLUP);
  pinMode(BTN_PREV,  INPUT_PULLUP);
  pinMode(BTN_PARAM, INPUT_PULLUP);

  // Alloc buffer PSRAM
  workBuf = (uint16_t*)ps_malloc(DST_W * DST_H * sizeof(uint16_t));
  if (!workBuf) {
    Serial.println("[MEM] PSRAM malloc failed!");
    while(1);
  }

  // TFT
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 70);
  tft.setTextSize(1);
  tft.println("VISION AID v1.0");
  tft.setCursor(10, 85);
  tft.println("Init camera...");

  // Palettes
  buildPalettes();
  buildGammaTable(2.2f);

  // Caméra
  if (!initCamera()) {
    tft.fillScreen(ST77XX_RED);
    tft.setCursor(5, 70);
    tft.setTextSize(1);
    tft.println("CAMERA FAILED");
    tft.println("Verif GPIO!");
    while(1) delay(1000);
  }

  tft.fillScreen(ST77XX_BLACK);
  Serial.println("[OK] VISION AID ready");
}

void loop() {
  handleButtons();

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAM] Frame failed");
    delay(10);
    return;
  }

  processFrame(fb);
  esp_camera_fb_return(fb);

  displayWorkBuf();
  drawStatusBar();
}
