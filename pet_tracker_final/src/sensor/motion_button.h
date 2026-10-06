#ifndef MOTION_BUTTON_H_
#define MOTION_BUTTON_H_

/*
 * Status: REAL, DK-only.
 *
 * Wires the DK's button0 (devicetree alias "sw0" - SW1 on the
 * nrf52840dk_nrf52840 board) straight into
 * state_machine_on_motion_event(). This is the "motion (button
 * stand-in)" trigger for end-to-end bench testing: press the button,
 * get a deterministic, on-demand motion event, independent of
 * whichever automatic source (real BMI270 or the simulated timer) is
 * active. Boards with no "sw0" alias just log a warning and continue.
 */

int motion_button_init(void);

#endif /* MOTION_BUTTON_H_ */
