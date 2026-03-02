#include "transitions.h"
#include "display.h"
#include <string.h>

// ─── Map (from, to) pair to a TransitionStyle ────────────────────────
TransitionStyle getTransitionStyle(int fromChoice, int toChoice) {
  if (fromChoice == 0 && toChoice == 1) return TRANS_SLIDE_LEFT;  // A→B
  if (fromChoice == 1 && toChoice == 0) return TRANS_FALL_DOWN;   // B→A
  if (fromChoice == 0 && toChoice == 2) return TRANS_SCRAMBLE;    // A→C
  if (fromChoice == 2 && toChoice == 0) return TRANS_EXPAND;      // C→A
  if (fromChoice == 2 && toChoice == 1) return TRANS_FADE;        // C→B
  if (fromChoice == 1 && toChoice == 2) return TRANS_BOUNCE;      // B→C
  return TRANS_SLIDE_LEFT;
}

// ─── Tokenizer with word-wrap support ────────────────────────────────
void tokenizeQuestion(const char* question, Word* words, int& wordCount, float yPos) {
  wordCount = 0;

  char buffer[128];
  strncpy(buffer, question, sizeof(buffer));

  // ── Step 1: collect all tokens ────────────────────────────────────
  char tokens[MAX_WORDS][32];
  int tokenCount = 0;
  char* token = strtok(buffer, " ");
  while (token != NULL && tokenCount < MAX_WORDS) {
    strncpy(tokens[tokenCount++], token, 32);
    token = strtok(NULL, " ");
  }

  // ── Step 2: split tokens into lines ──────────────────────────────
  int lineStarts[MAX_WORDS];
  int lineCounts[MAX_WORDS];
  int lineCount = 0;
  int i = 0;

  while (i < tokenCount) {
    lineStarts[lineCount] = i;
    lineCounts[lineCount] = 0;
    int lineWidth = 0;

    while (i < tokenCount) {
      int wordW = background.textWidth(tokens[i], FONT_SIZE);
      int gap   = (lineCounts[lineCount] > 0) ? WORD_SPACING : 0;
      if (lineWidth + gap + wordW > SCREEN_W - 8 && lineCounts[lineCount] > 0) {
        break; // overflow — start a new line
      }
      lineWidth += gap + wordW;
      lineCounts[lineCount]++;
      i++;
    }
    lineCount++;
  }

  // ── Step 3: assign X/Y centered per line ─────────────────────────
  int lineHeight = background.fontHeight(FONT_SIZE) + 4;
  // Center the whole block around yPos
  float blockTop = yPos - ((lineCount - 1) * lineHeight) / 2.0f;

  wordCount = 0;
  for (int l = 0; l < lineCount; l++) {
    // Measure this line width for horizontal centering
    int lineWidth = 0;
    for (int w = 0; w < lineCounts[l]; w++) {
      int idx = lineStarts[l] + w;
      lineWidth += background.textWidth(tokens[idx], FONT_SIZE);
      if (w < lineCounts[l] - 1) lineWidth += WORD_SPACING;
    }

    float cursorX = (SCREEN_W - lineWidth) / 2.0f;
    float cursorY = blockTop + l * lineHeight;

    for (int w = 0; w < lineCounts[l]; w++) {
      int idx = lineStarts[l] + w;
      strncpy(words[wordCount].text, tokens[idx], 32);
      words[wordCount].x       = cursorX;
      words[wordCount].y       = cursorY;
      words[wordCount].targetX = cursorX;
      words[wordCount].targetY = cursorY;
      words[wordCount].startX  = cursorX;
      words[wordCount].startY  = cursorY;
      words[wordCount].velX    = 0;
      words[wordCount].velY    = 0;
      words[wordCount].visible = true;
      words[wordCount].shared  = false;
      words[wordCount].settled = false;
      cursorX += background.textWidth(tokens[idx], FONT_SIZE) + WORD_SPACING;
      wordCount++;
    }
  }
}

// ─── Shared word matcher ─────────────────────────────────────────────
void matchSharedWords(Word* from, int fromCount, Word* to, int toCount) {
  bool usedFrom[MAX_WORDS] = { false };

  for (int i = 0; i < toCount; i++) {
    for (int j = 0; j < fromCount; j++) {
      if (!usedFrom[j] && strcmp(to[i].text, from[j].text) == 0) {
        to[i].shared   = true;
        from[j].shared = true;

        // Shared word in nextWords starts at the old word's position
        to[i].startX = from[j].x;
        to[i].startY = from[j].y;
        to[i].x      = from[j].x;
        to[i].y      = from[j].y;

        usedFrom[j] = true;
        break;
      }
    }
  }
}