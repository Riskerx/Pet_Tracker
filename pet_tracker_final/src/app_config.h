#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

/*
 * Set to 0 once a physical BMI270 is wired to the DK and you want to
 * run off the real any-motion interrupt instead of the simulated
 * timer. Kept as a plain #define rather than a Kconfig option so it
 * doesn't depend on getting an out-of-tree Kconfig fragment sourced
 * correctly - flip this one line and rebuild.
 */
#define TRACKER_SIM_MOTION 1

#endif /* APP_CONFIG_H_ */
