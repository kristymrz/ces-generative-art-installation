#include <Arduino.h>
#include <TFT_eSPI.h>
#include "display.h"
#include "transitions.h"
#include "words.h"
#include "star.h"

// ─── Sprites ─────────────────────────────────────────────────────────
TFT_eSPI    tft = TFT_eSPI();
TFT_eSprite background = TFT_eSprite(&tft);
TFT_eSprite starSprite = TFT_eSprite(&tft);

// ─── App state ───────────────────────────────────────────────────────
enum AppState { DISPLAYING, TRANSITIONING };
AppState appState = DISPLAYING;

// ─── Current question tracking ───────────────────────────────────────
int  currentChoice  = CHOICE_A;
int  nextChoice     = CHOICE_B;
const char* currentAnswer = "";

// ─── Word arrays ─────────────────────────────────────────────────────
Word currentWords[MAX_WORDS];
int  currentWordCount = 0;

Word nextWords[MAX_WORDS];
int  nextWordCount = 0;

// ─── Answer word slide state ──────────────────────────────────────────
float answerX         = 0;
float answerTargetX   = SCREEN_W / 2.0f;
int   answerDirection = 1;

// ─── Transition state ────────────────────────────────────────────────
TransitionStyle activeTransition;
unsigned long   displayStartTime = 0;

// ─── Fade transition state ───────────────────────────────────────────
int   fadeAlpha     = 255;
bool  fadingOut     = true;
int   fadeWordIndex = 0;
int   fadeInTimer   = 0;

// ─── Scramble state ──────────────────────────────────────────────────
ScrambleWord scrambleWords[MAX_WORDS];
int scrambleCount = 0;
const char scrambleChars[] = "abcdefghijklmnopqrstuvwxyz?!@#";

// ─── Star sine-wave drift state ───────────────────────────────────────
#define STAR_SIZE 32

float starBaseX   = 0;       // base X that drifts across the screen
float starBaseY   = 0;       // base Y that drifts across the screen
float starDirX    = 0.6f;    // drift velocity X
float starDirY    = 0.3f;    // drift velocity Y
float starSineT   = 0;       // time variable for sine offset
float starSineAmp = 18.0f;   // how wide the wave swings (pixels)
float starSineSpd = 0.04f;   // how fast the wave oscillates

// Perpendicular to drift direction — used to apply sine offset
float starPerpX   = 0;
float starPerpY   = 0;

void initStar() {
  starBaseX = random(0, SCREEN_W - STAR_SIZE);
  starBaseY = random(0, SCREEN_H - STAR_SIZE);

  // Random drift direction, normalized
  float angle = random(0, 360) * DEG_TO_RAD;
  starDirX = cos(angle) * 0.7f;
  starDirY = sin(angle) * 0.4f;

  // Perpendicular to drift direction (rotate 90 degrees)
  starPerpX = -starDirY;
  starPerpY =  starDirX;

  starSineT = 0;
}

void updateStar() {
  // Advance base position along drift direction
  starBaseX += starDirX;
  starBaseY += starDirY;

  // Wrap around screen edges
  if (starBaseX > SCREEN_W + STAR_SIZE)  starBaseX = -STAR_SIZE;
  if (starBaseX < -STAR_SIZE)            starBaseX = SCREEN_W + STAR_SIZE;
  if (starBaseY > SCREEN_H + STAR_SIZE)  starBaseY = -STAR_SIZE;
  if (starBaseY < -STAR_SIZE)            starBaseY = SCREEN_H + STAR_SIZE;

  // Advance sine time
  starSineT += starSineSpd;
}

void drawStar() {
  // Apply sine offset perpendicular to drift direction
  float sineOffset = sin(starSineT) * starSineAmp;
  int drawX = (int)(starBaseX + starPerpX * sineOffset);
  int drawY = (int)(starBaseY + starPerpY * sineOffset);

  // Push star sprite onto background at computed position,
  // using TFT_BLACK as the transparent color
  starSprite.pushToSprite(&background, drawX, drawY, TFT_BLACK);
}

// ─── Helper: pick a random answer for a given choice ─────────────────
const char* pickAnswer(int choice) {
  switch (choice) {
    case CHOICE_A: return answersA[random(0, answersA_count)];
    case CHOICE_B: return answersB[random(0, answersB_count)];
    case CHOICE_C: return answersC[random(0, answersC_count)];
  }
  return "";
}

// ─── Helper: pick next choice (not same as current) ──────────────────
int pickNextChoice(int current) {
  int next = random(0, 3);
  while (next == current) next = random(0, 3);
  return next;
}

// ─── Oscillating color for answer word ───────────────────────────────
uint16_t oscillatingColor() {
  float t  = millis() / 800.0f;
  uint8_t r = (uint8_t)((sin(t)         + 1.0f) * 127.5f);
  uint8_t g = (uint8_t)((sin(t + 2.09f) + 1.0f) * 127.5f);
  uint8_t b = (uint8_t)((sin(t + 4.18f) + 1.0f) * 127.5f);
  return tft.color565(r, g, b);
}

// ─── Draw wrapped answer word ─────────────────────────────────────────
// Splits the answer string into lines that fit SCREEN_W, then draws
// each line centered around (centerX, startY).
void drawWrappedAnswer(const char* answer, int centerX, int startY, int font) {
  char buffer[64];
  strncpy(buffer, answer, sizeof(buffer));

  char lines[4][64];
  int  lineCount  = 0;
  int  lineHeight = background.fontHeight(font) + 4;

  char  currentLine[64] = "";
  char* token = strtok(buffer, " ");

  while (token != NULL) {
    char testLine[64];
    if (strlen(currentLine) == 0) {
      strncpy(testLine, token, sizeof(testLine));
    } else {
      snprintf(testLine, sizeof(testLine), "%s %s", currentLine, token);
    }

    if (background.textWidth(testLine, font) > SCREEN_W - 8) {
      strncpy(lines[lineCount++], currentLine, 64);
      strncpy(currentLine, token, sizeof(currentLine));
    } else {
      strncpy(currentLine, testLine, sizeof(currentLine));
    }
    token = strtok(NULL, " ");
  }
  if (strlen(currentLine) > 0) {
    strncpy(lines[lineCount++], currentLine, 64);
  }

  float blockTop = startY - ((lineCount - 1) * lineHeight) / 2.0f;
  background.setTextDatum(MC_DATUM);
  for (int i = 0; i < lineCount; i++) {
    background.drawString(lines[i], centerX, (int)(blockTop + i * lineHeight), font);
  }
  background.setTextDatum(TL_DATUM);
}

// ─── Draw current words + answer + star onto background sprite ────────
void drawDisplayFrame() {
  background.fillSprite(TFT_BLACK);

  // Draw star first so it sits behind text
  updateStar();
  drawStar();

  // Draw each question word
  background.setTextColor(TFT_WHITE);
  for (int i = 0; i < currentWordCount; i++) {
    if (currentWords[i].visible) {
      background.drawString(currentWords[i].text, (int)currentWords[i].x, (int)currentWords[i].y, FONT_SIZE);
    }
  }

  // Draw "?" after the last word
  int lastIdx = currentWordCount - 1;
  if (lastIdx >= 0) {
    int qx = (int)currentWords[lastIdx].x + background.textWidth(currentWords[lastIdx].text, FONT_SIZE);
    background.drawString("?", qx, (int)currentWords[lastIdx].y, FONT_SIZE);
  }

  // Ease answer word toward center
  float dx = answerTargetX - answerX;
  if (abs(dx) > 1.5f) {
    answerX += dx * 0.12f;
  } else {
    answerX = answerTargetX;
  }

  // Draw answer word with oscillating color
  background.setTextColor(oscillatingColor()); 
  drawWrappedAnswer(currentAnswer, (int)answerX, ANSWER_Y, ANSWER_FONT);

  background.pushSprite(0, 0);
}

// ─── Glide helper ─────────────────────────────────────────────────────
bool glideWord(Word& w) {
  float dx   = w.targetX - w.x;
  float dy   = w.targetY - w.y;
  float dist = sqrt(dx * dx + dy * dy);
  if (dist < TRANSITION_SPEED) {
    w.x = w.targetX;
    w.y = w.targetY;
    w.settled = true;
    return true;
  }
  w.x += (dx / dist) * TRANSITION_SPEED;
  w.y += (dy / dist) * TRANSITION_SPEED;
  return false;
}

// ─── Check if all nextWords have settled ─────────────────────────────
bool allSettled() {
  for (int i = 0; i < nextWordCount; i++) {
    if (!nextWords[i].settled) return false;
  }
  return true;
}

// ─── Finish transition: swap current ← next ──────────────────────────
void finishTransition() {
  currentChoice = nextChoice;
  currentAnswer = pickAnswer(currentChoice);

  // Alternate answer word slide direction
  answerDirection *= -1;
  answerX = (answerDirection == 1) ? (float)(SCREEN_W + 60) : -60.0f;

  memcpy(currentWords, nextWords, sizeof(Word) * nextWordCount);
  currentWordCount = nextWordCount;

  for (int i = 0; i < currentWordCount; i++) {
    currentWords[i].settled = false;
    currentWords[i].shared  = false;
    currentWords[i].x       = currentWords[i].targetX;
    currentWords[i].y       = currentWords[i].targetY;
  }

  appState         = DISPLAYING;
  displayStartTime = millis();
}

// ─── Initialise a transition ─────────────────────────────────────────
void startTransition(int fromChoice, int toChoice) {
  activeTransition = getTransitionStyle(fromChoice, toChoice);
  nextChoice       = toChoice;

  tokenizeQuestion(questions[toChoice], nextWords, nextWordCount, QUESTION_Y);
  matchSharedWords(currentWords, currentWordCount, nextWords, nextWordCount);

  if (activeTransition == TRANS_SLIDE_LEFT) {
    for (int i = 0; i < currentWordCount; i++) {
      if (!currentWords[i].shared) currentWords[i].targetX = -100;
    }
    for (int i = 0; i < nextWordCount; i++) {
      if (!nextWords[i].shared) {
        nextWords[i].x = SCREEN_W + 10;
        nextWords[i].y = nextWords[i].targetY;
      }
    }
  }

  else if (activeTransition == TRANS_FALL_DOWN) {
    for (int i = 0; i < currentWordCount; i++) {
      if (!currentWords[i].shared) currentWords[i].targetY = SCREEN_H + 40;
    }
    for (int i = 0; i < nextWordCount; i++) {
      if (!nextWords[i].shared) {
        nextWords[i].x = nextWords[i].targetX;
        nextWords[i].y = -30;
      }
    }
  }

  else if (activeTransition == TRANS_SCRAMBLE) {
    scrambleCount = 0;
    for (int i = 0; i < nextWordCount; i++) {
      if (!nextWords[i].shared) {
        strncpy(scrambleWords[scrambleCount].target, nextWords[i].text, 32);
        scrambleWords[scrambleCount].frameCount = 0;
        scrambleWords[scrambleCount].resolved   = false;
        int len = strlen(nextWords[i].text);
        for (int c = 0; c < len; c++) {
          scrambleWords[scrambleCount].display[c] = scrambleChars[random(0, 30)];
        }
        scrambleWords[scrambleCount].display[len] = '\0';
        scrambleCount++;
      }
    }
    for (int i = 0; i < currentWordCount; i++) {
      if (!currentWords[i].shared) currentWords[i].targetX = -120;
    }
    for (int i = 0; i < nextWordCount; i++) {
      if (!nextWords[i].shared) {
        nextWords[i].x = nextWords[i].targetX;
        nextWords[i].y = nextWords[i].targetY;
      }
    }
  }

  else if (activeTransition == TRANS_EXPAND) {
    for (int i = 0; i < nextWordCount; i++) {
      nextWords[i].x = SCREEN_W / 2.0f;
      nextWords[i].y = QUESTION_Y;
    }
    for (int i = 0; i < currentWordCount; i++) {
      currentWords[i].targetX = SCREEN_W / 2.0f;
      currentWords[i].targetY = QUESTION_Y;
    }
  }

  else if (activeTransition == TRANS_FADE) {
    fadeAlpha     = 255;
    fadingOut     = true;
    fadeWordIndex = 0;
    fadeInTimer   = 0;
  }

  else if (activeTransition == TRANS_BOUNCE) {
    for (int i = 0; i < currentWordCount; i++) {
      if (!currentWords[i].shared) {
        float angle = random(0, 360) * DEG_TO_RAD;
        currentWords[i].velX    = cos(angle) * 6.0f;
        currentWords[i].velY    = sin(angle) * 6.0f;
        currentWords[i].targetX = currentWords[i].x + cos(angle) * 200;
        currentWords[i].targetY = currentWords[i].y + sin(angle) * 200;
      }
    }
    for (int i = 0; i < nextWordCount; i++) {
      if (!nextWords[i].shared) {
        nextWords[i].velX = 0;
        nextWords[i].velY = 0;
        nextWords[i].x    = nextWords[i].targetX + random(-80, 80);
        nextWords[i].y    = -40;
      }
    }
  }

  appState = TRANSITIONING;
}

// ═══════════════════════════════════════════════════════════════════════
// TRANSITION UPDATE FUNCTIONS
// Each one calls updateStar() + drawStar() so the star keeps drifting
// even during transitions.
// ═══════════════════════════════════════════════════════════════════════

// ── Path 1: Slide Left ────────────────────────────────────────────────
void updateSlideLeft() {
  background.fillSprite(TFT_BLACK);
  updateStar(); drawStar();
  background.setTextColor(TFT_WHITE);

  bool exitDone = true;
  for (int i = 0; i < currentWordCount; i++) {
    if (!currentWords[i].shared) {
      glideWord(currentWords[i]);
      if (!currentWords[i].settled) exitDone = false;
    }
    if (currentWords[i].x > -50 && currentWords[i].x < SCREEN_W + 50)
      background.drawString(currentWords[i].text, (int)currentWords[i].x, (int)currentWords[i].y, FONT_SIZE);
  }

  bool allIn = true;
  for (int i = 0; i < nextWordCount; i++) {
    glideWord(nextWords[i]);
    if (!nextWords[i].settled) allIn = false;
    if (nextWords[i].x > -50 && nextWords[i].x < SCREEN_W + 50)
      background.drawString(nextWords[i].text, (int)nextWords[i].x, (int)nextWords[i].y, FONT_SIZE);
  }

  background.pushSprite(0, 0);
  if (exitDone && allIn) finishTransition();
}

// ── Path 2: Fall Down ─────────────────────────────────────────────────
void updateFallDown() {
  background.fillSprite(TFT_BLACK);
  updateStar(); drawStar();
  background.setTextColor(TFT_WHITE);

  bool exitDone = true;
  for (int i = 0; i < currentWordCount; i++) {
    if (!currentWords[i].shared) {
      glideWord(currentWords[i]);
      if (!currentWords[i].settled) exitDone = false;
    }
    if (currentWords[i].y < SCREEN_H + 30)
      background.drawString(currentWords[i].text, (int)currentWords[i].x, (int)currentWords[i].y, FONT_SIZE);
  }

  bool allIn = true;
  for (int i = 0; i < nextWordCount; i++) {
    glideWord(nextWords[i]);
    if (!nextWords[i].settled) allIn = false;
    if (nextWords[i].y > -30)
      background.drawString(nextWords[i].text, (int)nextWords[i].x, (int)nextWords[i].y, FONT_SIZE);
  }

  background.pushSprite(0, 0);
  if (exitDone && allIn) finishTransition();
}

// ── Path 3: Scramble ──────────────────────────────────────────────────
void updateScramble() {
  background.fillSprite(TFT_BLACK);
  updateStar(); drawStar();
  background.setTextColor(TFT_WHITE);

  bool exitDone = true;
  for (int i = 0; i < currentWordCount; i++) {
    if (!currentWords[i].shared) {
      glideWord(currentWords[i]);
      if (!currentWords[i].settled) exitDone = false;
      if (currentWords[i].x > -80)
        background.drawString(currentWords[i].text, (int)currentWords[i].x, (int)currentWords[i].y, FONT_SIZE);
    }
  }

  for (int i = 0; i < nextWordCount; i++) {
    if (nextWords[i].shared) {
      glideWord(nextWords[i]);
      background.drawString(nextWords[i].text, (int)nextWords[i].x, (int)nextWords[i].y, FONT_SIZE);
    }
  }

  int si = 0;
  for (int i = 0; i < nextWordCount; i++) {
    if (!nextWords[i].shared) {
      ScrambleWord& sw = scrambleWords[si++];
      if (!sw.resolved) {
        sw.frameCount++;
        if (exitDone && sw.frameCount > 8) {
          int len = strlen(sw.target);
          int resolved = 0;
          for (int c = 0; c < len; c++) {
            if (sw.display[c] == sw.target[c]) {
              resolved++;
            } else if (sw.frameCount % 3 == 0) {
              sw.display[c] = (random(0, 3) == 0) ? sw.target[c] : scrambleChars[random(0, 30)];
            }
          }
          if (resolved == len) sw.resolved = true;
        } else {
          int len = strlen(sw.target);
          for (int c = 0; c < len; c++) {
            if (sw.frameCount % 2 == 0)
              sw.display[c] = scrambleChars[random(0, 30)];
          }
        }
        background.setTextColor(oscillatingColor()); 
        background.drawString(sw.display, (int)nextWords[i].x, (int)nextWords[i].y, FONT_SIZE);
        background.setTextColor(TFT_WHITE);
        nextWords[i].settled = sw.resolved;
      } else {
        background.drawString(nextWords[i].text, (int)nextWords[i].x, (int)nextWords[i].y, FONT_SIZE);
        nextWords[i].settled = true;
      }
    }
  }

  background.pushSprite(0, 0);
  if (allSettled()) finishTransition();
}

// ── Path 4: Expand ────────────────────────────────────────────────────
void updateExpand() {
  background.fillSprite(TFT_BLACK);
  updateStar(); drawStar();
  background.setTextColor(TFT_WHITE);

  bool shrinkDone = true;
  for (int i = 0; i < currentWordCount; i++) {
    glideWord(currentWords[i]);
    if (!currentWords[i].settled) shrinkDone = false;
    float dx = currentWords[i].x - SCREEN_W / 2.0f;
    if (abs(dx) > 4)
      background.drawString(currentWords[i].text, (int)currentWords[i].x, (int)currentWords[i].y, FONT_SIZE);
  }

  if (shrinkDone) {
    bool allIn = true;
    for (int i = 0; i < nextWordCount; i++) {
      glideWord(nextWords[i]);
      if (!nextWords[i].settled) allIn = false;
      background.drawString(nextWords[i].text, (int)nextWords[i].x, (int)nextWords[i].y, FONT_SIZE);
    }
    background.pushSprite(0, 0);
    if (allIn) finishTransition();
  } else {
    background.pushSprite(0, 0);
  }
}

// ── Path 5: Fade ──────────────────────────────────────────────────────
void updateFade() {
  background.fillSprite(TFT_BLACK);
  updateStar(); drawStar();

  if (fadingOut) {
    fadeAlpha -= 8;
    if (fadeAlpha <= 0) {
      fadeAlpha     = 0;
      fadingOut     = false;
      fadeWordIndex = 0;
      fadeInTimer   = 0;
      for (int i = 0; i < nextWordCount; i++) nextWords[i].settled = false;
    }
    uint8_t bright   = (uint8_t)max(0, fadeAlpha);
    uint16_t dimColor = tft.color565(bright, bright, bright);
    background.setTextColor(dimColor, TFT_BLACK);
    for (int i = 0; i < currentWordCount; i++)
      background.drawString(currentWords[i].text, (int)currentWords[i].x, (int)currentWords[i].y, FONT_SIZE);
  } else {
    fadeInTimer++;
    if (fadeInTimer > 6 && fadeWordIndex < nextWordCount) {
      nextWords[fadeWordIndex].settled = true;
      fadeWordIndex++;
      fadeInTimer = 0;
    }
    background.setTextColor(TFT_WHITE);
    for (int i = 0; i < fadeWordIndex; i++)
      background.drawString(nextWords[i].text, (int)nextWords[i].x, (int)nextWords[i].y, FONT_SIZE);
    if (fadeWordIndex >= nextWordCount) finishTransition();
  }

  background.pushSprite(0, 0);
}

// ── Path 6: Bounce ────────────────────────────────────────────────────
void updateBounce() {
  background.fillSprite(TFT_BLACK);
  updateStar(); drawStar();
  background.setTextColor(TFT_WHITE);

  bool exitDone = true;
  for (int i = 0; i < currentWordCount; i++) {
    if (!currentWords[i].shared) {
      currentWords[i].x += currentWords[i].velX;
      currentWords[i].y += currentWords[i].velY;
      bool offScreen = (currentWords[i].x < -60 || currentWords[i].x > SCREEN_W + 60 ||
                        currentWords[i].y < -30  || currentWords[i].y > SCREEN_H + 30);
      if (!offScreen) {
        exitDone = false;
        background.drawString(currentWords[i].text, (int)currentWords[i].x, (int)currentWords[i].y, FONT_SIZE);
      }
    }
  }

  for (int i = 0; i < nextWordCount; i++) {
    if (nextWords[i].shared) {
      glideWord(nextWords[i]);
      background.drawString(nextWords[i].text, (int)nextWords[i].x, (int)nextWords[i].y, FONT_SIZE);
    }
  }

  if (exitDone) {
    bool allIn = true;
    for (int i = 0; i < nextWordCount; i++) {
      if (!nextWords[i].shared) {
        float dx = nextWords[i].targetX - nextWords[i].x;
        float dy = nextWords[i].targetY - nextWords[i].y;
        nextWords[i].velX = nextWords[i].velX * 0.7f + dx * 0.15f;
        nextWords[i].velY = nextWords[i].velY * 0.7f + dy * 0.15f;
        nextWords[i].x   += nextWords[i].velX;
        nextWords[i].y   += nextWords[i].velY;
        if (sqrt(dx*dx + dy*dy) < 1.5f) {
          nextWords[i].x = nextWords[i].targetX;
          nextWords[i].y = nextWords[i].targetY;
          nextWords[i].settled = true;
        } else {
          allIn = false;
        }
        background.drawString(nextWords[i].text, (int)nextWords[i].x, (int)nextWords[i].y, FONT_SIZE);
      }
    }
    background.pushSprite(0, 0);
    if (allIn && allSettled()) finishTransition();
  } else {
    background.pushSprite(0, 0);
  }
}

// ═══════════════════════════════════════════════════════════════════════
// SETUP & LOOP
// ═══════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  tft.init();
  tft.setRotation(2);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  background.createSprite(SCREEN_W, SCREEN_H);
  background.setTextWrap(false, false);

  starSprite.createSprite(STAR_SIZE, STAR_SIZE);
  starSprite.setSwapBytes(true);
  starSprite.pushImage(0, 0, STAR_SIZE, STAR_SIZE, star);

  initStar();

  currentChoice = CHOICE_A;
  currentAnswer = pickAnswer(currentChoice);
  answerX       = SCREEN_W + 60;
  tokenizeQuestion(questions[currentChoice], currentWords, currentWordCount, QUESTION_Y);

  displayStartTime = millis();
  appState = DISPLAYING;
}

void loop() {
  switch (appState) {

    case DISPLAYING:
      drawDisplayFrame();
      if (millis() - displayStartTime > DISPLAY_DURATION_MS) {
        int next = pickNextChoice(currentChoice);
        startTransition(currentChoice, next);
      }
      break;

    case TRANSITIONING:
      switch (activeTransition) {
        case TRANS_SLIDE_LEFT: updateSlideLeft(); break;
        case TRANS_FALL_DOWN:  updateFallDown();  break;
        case TRANS_SCRAMBLE:   updateScramble();  break;
        case TRANS_EXPAND:     updateExpand();    break;
        case TRANS_FADE:       updateFade();      break;
        case TRANS_BOUNCE:     updateBounce();    break;
      }
      break;
  }

  delay(FRAME_DELAY_MS);
}