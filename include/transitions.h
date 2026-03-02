#pragma once
#include <Arduino.h>

// ─── Max words in any question ───────────────────────────────────────
#define MAX_WORDS 9

// ─── Word token ──────────────────────────────────────────────────────
struct Word {
  char  text[32];
  float x,  y;          // current position
  float targetX, targetY;
  float startX,  startY;
  float velX,    velY;  // used by bounce path
  bool  visible;
  bool  shared;         // true = this word exists in both questions
  bool  settled;        // true = animation complete for this word
};

// ─── Scramble state for Path 3 ───────────────────────────────────────
struct ScrambleWord {
  char  display[32];    // what's currently shown (random chars while scrambling)
  char  target[32];     // the real text to resolve to
  int   frameCount;
  bool  resolved;
};

// ─── Transition styles ───────────────────────────────────────────────
enum TransitionStyle {
  TRANS_SLIDE_LEFT,   // Path 1: A→B
  TRANS_FALL_DOWN,    // Path 2: B→A
  TRANS_SCRAMBLE,     // Path 3: A→C
  TRANS_EXPAND,       // Path 4: C→A
  TRANS_FADE,         // Path 5: C→B
  TRANS_BOUNCE        // Path 6: B→C
};

// ─── Determine which path/style to use ───────────────────────────────
TransitionStyle getTransitionStyle(int fromChoice, int toChoice);

// ─── Tokenizer ───────────────────────────────────────────────────────
void tokenizeQuestion(const char* question, Word* words, int& wordCount, float yPos);

// ─── Shared word matcher ─────────────────────────────────────────────
void matchSharedWords(Word* from, int fromCount, Word* to, int toCount);