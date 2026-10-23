#pragma once
#define DT_ALIAS(x) 1
#define DT_NODE_EXISTS(x) 0
#define DT_NODE_HAS_PROP(node, prop) 1
#define GPIO_DT_SPEC_GET(node, prop) {}
#define GPIO_OUTPUT_INACTIVE 0
struct gpio_dt_spec {};
inline bool gpio_is_ready_dt(const gpio_dt_spec *) { return true; }
inline int gpio_pin_configure_dt(const gpio_dt_spec *, int) { return 0; }
inline int gpio_pin_set_dt(const gpio_dt_spec *, int) { return 0; }
