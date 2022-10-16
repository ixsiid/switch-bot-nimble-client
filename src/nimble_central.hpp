#pragma once

#define min
#define max

#include <nimble/ble.h>
#include <host/ble_uuid.h>

#undef min
#undef max

// typedef std::function<int(uint16_t, void*)> NimbleCallback;

typedef int (*NimbleCallback)(uint16_t, void*);

class NimbleCentral {
    private:
	static bool is_started;

	static void blecent_on_reset(int reason);
	static void blecent_on_sync();
	static void blecent_host_task(void *param);
	static void blecent_scan();

	static int chr_disced(uint16_t conn_handle, const struct ble_gatt_error *error,
					  const struct ble_gatt_chr *chr, void *arg);
	static int svc_disced(uint16_t conn_handle, const struct ble_gatt_error *error,
					  const struct ble_gatt_svc *svc, void *arg);

	static int blecent_gap_event(struct ble_gap_event *event, void *arg);

    public:
	static int start(const char *device_name);
	static int connect(const ble_addr_t *address, NimbleCallback callback, void *arg);
	static int disconnect(uint16_t handle, NimbleCallback callback, void *arg);
	static int write(uint16_t handle,
				  const ble_uuid_t *service, const ble_uuid_t *characteristic,
				  const uint8_t *value, size_t length, int timeout,
				  NimbleCallback success, NimbleCallback failed, void *arg);
};
