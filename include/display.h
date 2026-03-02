#pragma once
#include <TFT_eSPI.h>

// ─── Screen dimensions (portrait, setRotation(2)) ───────────────────
#define SCREEN_W     135
#define SCREEN_H     240

// ─── Layout ──────────────────────────────────────────────────────────
#define QUESTION_Y   60    // Y center of the question line
#define ANSWER_Y     130   // Y center of the answer word
#define FONT_SIZE    2     // TFT_eSPI built-in font for question words
#define ANSWER_FONT  4     // larger font for the answer word
#define WORD_SPACING 4     // pixels between words

// ─── Timing ──────────────────────────────────────────────────────────
#define DISPLAY_DURATION_MS   4000   // how long to show a question before transitioning
#define TRANSITION_SPEED      3.0f   // pixels per frame for word gliding
#define FRAME_DELAY_MS        16     // ~60fps

// ─── Extern sprites (defined in main.cpp) ───────────────────────────
extern TFT_eSPI       tft;
extern TFT_eSprite    background;