#ifndef BLE_SECURITY_H_
#define BLE_SECURITY_H_

/**
 * Set up BLE pairing/bonding.
 *
 * Registers the passkey-display authentication callbacks and a button
 * handler (Button 1) that clears bonds for testing the re-pair flow.
 *
 * Call this AFTER bt_enable() and BEFORE settings_load(), so the
 * callbacks exist before any bonded device tries to reconnect.
 *
 * @return 0 on success, negative errno on failure.
 */
int ble_security_init(void);

/**
 * Delete every stored bond and force a fresh pairing on the next
 * connection. Wired to Button 1 on the DK, but you can call it
 * directly too (e.g. from a "factory reset" path).
 */
void ble_security_clear_bonds(void);

#endif /* BLE_SECURITY_H_ */