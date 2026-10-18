#pragma once
#include <cstdint>
using gpio_num_t = int;
constexpr int GPIO_MODE_OUTPUT = 1;
constexpr int GPIO_INTR_DISABLE = 0;
struct gpio_config_t { uint64_t pin_bit_mask = 0; int mode = 0; int intr_type = 0; };
inline int gpio_config(const gpio_config_t *) { return 0; }
int gpio_set_level(gpio_num_t, uint32_t);
