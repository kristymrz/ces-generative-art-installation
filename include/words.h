#pragma once

// ─── Question indices ───────────────────────────────────────────────
#define CHOICE_A 0
#define CHOICE_B 1
#define CHOICE_C 2

// ─── Questions ──────────────────────────────────────────────────────
const char* questions[] = {
  "what do you want to be",   // A  (no ? so punctuation doesn't break word matching)
  "are you who you want to be",  // B
  "who are you"              // C
};

// Punctuation displayed separately at end (optional visual touch)
const char* questionMarks[] = { "?", "?", "?" };

// ─── Answer arrays ───────────────────────────────────────────────────
const char* answersA[] = { "brave", "happy", "confident", "successful", "loving"};
const int   answersA_count = 5;

const char* answersB[] = { "yes", "no", "huh?", "i guess" };
const int   answersB_count = 4;

const char* answersC[] = { "i am me", "i am you", "i dont know", "am i supposed to know?" };
const int   answersC_count = 4;