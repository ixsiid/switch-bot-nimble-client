#include <nimble/ble.h>
#include <host/ble_uuid.h>

class NimbleCentral {
private:
	static bool is_started;

	static void blecent_on_reset(int reason);
	static void blecent_on_sync();
	static void blecent_host_task(void *param);
	static void blecent_scan();
	static int blecent_gap_event(struct ble_gap_event *event, void *arg);
public:
	static int start();
	static int connect(ble_addr_t address, uint16_t& out_handle);
	static int disconnect(uint16_t handle);
	static int write(uint16_t handle,
		ble_uuid_t service, ble_uuid_t characteristic,
		const uint8_t * value, size_t length, int timeout = 10000);
};
