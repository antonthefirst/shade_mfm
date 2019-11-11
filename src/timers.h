#pragma once
#include "core/basic_types.h"
#include "core/hashed_string.h"

struct Input;

void gtimer_reset();
void gtimer_start(const char* name);
void gtimer_stop();

void timerUI(Input& in);

void chashtimer_start(HashedString hs);
void chashtimer_stop();

void    ctimer_reset();
#define ctimer_start(s) chashtimer_start(HS(s))
#define ctimer_stop()   chashtimer_stop()
void    ctimer_gui();

