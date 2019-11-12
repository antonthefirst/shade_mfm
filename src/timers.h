#pragma once
#include "core/basic_types.h"
#include "core/hashed_string.h"
#include <vulkan/vulkan.h>

struct Input;

void ghashtimer_start(HashedString hs);
void ghashtimer_stop();

void gtimer_reset(VkCommandBuffer cb);
#define gtimer_start(s) ghashtimer_start(HS(s))
#define gtimer_stop()   ghashtimer_stop()
void gtimer_gui();

void timerUI(Input& in);

void chashtimer_start(HashedString hs);
void chashtimer_stop();

void    ctimer_reset();
#define ctimer_start(s) chashtimer_start(HS(s))
#define ctimer_stop()   chashtimer_stop()
void    ctimer_gui();

