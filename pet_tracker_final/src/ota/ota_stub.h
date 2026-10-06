#ifndef OTA_STUB_H_
#define OTA_STUB_H_

/* Status: STUB. No DFU backend (MCUboot/SMP) wired in yet. These
 * exist purely as the integration points power_mgmt and ble already
 * expect, so dropping in a real OTA implementation later doesn't
 * require touching any other module. */

void ota_init(void);

/* Would kick off an image transfer; currently just demonstrates the
 * power_ota_lock()/unlock() handshake with a fake instant "transfer". */
void ota_start(void);

#endif /* OTA_STUB_H_ */
