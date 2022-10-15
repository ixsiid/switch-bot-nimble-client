#include <esp_log.h>
#include <nvs_flash.h>
#include <host/ble_gap.h>
#include <services/gap/ble_svc_gap.h>
#include <nimble/nimble_port_freertos.h>
#include <nimble/nimble_port.h>
#include <esp_nimble_hci.h>
#include <host/util/util.h>

#include "misc.h"

#include "nimble_central.hpp"

const char *tag = "NimBleCentral";

typedef struct {
} callback_args_t;

bool NimbleCentral::is_started = false;

int NimbleCentral::start(const char *device_name) {
	ESP_LOGI(tag, "start");
	if (is_started) return 0;

	int rc;

	ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());

	nimble_port_init();
	/* Configure the host. */
	ble_hs_cfg.reset_cb		  = blecent_on_reset;
	ble_hs_cfg.sync_cb		  = blecent_on_sync;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

	/* Set the default device name. */
	rc = ble_svc_gap_device_name_set(device_name);

	nimble_port_freertos_init(blecent_host_task);

	is_started = true;
	ESP_LOGI(tag, "started");
	return 0;
}

void NimbleCentral::blecent_on_reset(int reason) {
	MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

void NimbleCentral::blecent_on_sync() {
	int rc;

	/* Make sure we have proper identity address set (public preferred) */
	rc = ble_hs_util_ensure_addr(0);
	assert(rc == 0);

	/* Begin scanning for a peripheral to connect to. */
	blecent_scan();
}

void NimbleCentral::blecent_host_task(void *param) {
	ESP_LOGI(tag, "BLE Host Task Started");
	/* This function will return only when nimble_port_stop() is executed */
	nimble_port_run();

	nimble_port_freertos_deinit();
}

void NimbleCentral::blecent_scan() {
	uint8_t own_addr_type;
	struct ble_gap_disc_params disc_params;
	int rc;

	/* Figure out address to use while advertising (no privacy for now) */
	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc != 0) {
		MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
		return;
	}

	/* Tell the controller to filter duplicates; we don't want to process
	 * repeated advertisements from the same device.
	 */
	disc_params.filter_duplicates = 1;

	/**
	 * Perform a passive scan.  I.e., don't send follow-up scan requests to
	 * each advertiser.
	 */
	disc_params.passive = 1;

	/* Use defaults for the rest of the parameters. */
	disc_params.itvl		 = 0;
	disc_params.window		 = 0;
	disc_params.filter_policy = 0;
	disc_params.limited		 = 0;

	rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, blecent_gap_event, nullptr);
	if (rc != 0) {
		MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n", rc);
	}
}

typedef struct {
	NimbleCallback callback;
	void *param;
} callback_t;

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  blecent uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  blecent.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
int NimbleCentral::blecent_gap_event(struct ble_gap_event *event, void *arg) {
	struct ble_gap_conn_desc desc;
	int rc;

	ESP_LOGV(tag, "blecent_gap_event: %d", event->type);

	switch (event->type) {
		case BLE_GAP_EVENT_DISC:
			MODLOG_DFLT(DEBUG, "discovery; type=%d", event->disc.event_type);
			return 0;

		case BLE_GAP_EVENT_CONNECT:
			/* A new connection was established or a connection attempt failed. */
			if (event->connect.status == 0) {
				/* Connection successfully established. */
				ESP_LOGI(tag, "Connect");
				MODLOG_DFLT(INFO, "Connection established ");

				rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
				assert(rc == 0);
				print_conn_desc(&desc);
				MODLOG_DFLT(INFO, "\n");

				callback_t *cb = (callback_t *)arg;
				ESP_LOGI(tag, "callback function by connect: %d, %p, %p, %p", event->connect.conn_handle, cb, cb->callback, cb->param);
				if (cb->callback == nullptr) return 0;
				cb->callback(event->connect.conn_handle, cb->param);
				delete arg;

				return 0;
			} else {
				/* Connection attempt failed; resume scanning. */
				MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n",
						  event->connect.status);
				blecent_scan();
			}

			return 0;

		case BLE_GAP_EVENT_DISCONNECT:
			/* Connection terminated. */
			MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
			print_conn_desc(&event->disconnect.conn);
			MODLOG_DFLT(INFO, "\n");

			/* Resume scanning. */
			blecent_scan();
			return 0;

		case BLE_GAP_EVENT_DISC_COMPLETE:
			MODLOG_DFLT(INFO, "discovery complete; reason=%d\n",
					  event->disc_complete.reason);
			return 0;

		case BLE_GAP_EVENT_ENC_CHANGE:
			/* Encryption has been enabled or disabled for this connection. */
			MODLOG_DFLT(INFO, "encryption change event; status=%d ",
					  event->enc_change.status);
			rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
			assert(rc == 0);
			print_conn_desc(&desc);
			return 0;

		case BLE_GAP_EVENT_NOTIFY_RX:
			/* Peer sent us a notification or indication. */
			MODLOG_DFLT(INFO,
					  "received %s; conn_handle=%d attr_handle=%d "
					  "attr_len=%d\n",
					  event->notify_rx.indication ? "indication" : "notification",
					  event->notify_rx.conn_handle,
					  event->notify_rx.attr_handle,
					  OS_MBUF_PKTLEN(event->notify_rx.om));

			/* Attribute data is contained in event->notify_rx.om. Use
			 * `os_mbuf_copydata` to copy the data received in notification mbuf */
			return 0;

		case BLE_GAP_EVENT_MTU:
			MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
					  event->mtu.conn_handle,
					  event->mtu.channel_id,
					  event->mtu.value);
			return 0;

		case BLE_GAP_EVENT_REPEAT_PAIRING:
			/* We already have a bond with the peer, but it is attempting to
			 * establish a new secure link.  This app sacrifices security for
			 * convenience: just throw away the old bond and accept the new link.
			 */

			/* Delete the old bond. */
			rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
			assert(rc == 0);
			ble_store_util_delete_peer(&desc.peer_id_addr);

			/* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
			 * continue with the pairing operation.
			 */
			return BLE_GAP_REPEAT_PAIRING_RETRY;

		default:
			return 0;
	}
}

int NimbleCentral::connect(const ble_addr_t *address, NimbleCallback callback, void *args) {
	int rc;
	/* Scanning must be stopped before a connection can be initiated. */
	rc = ble_gap_disc_cancel();
	if (rc != 0) {
		MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
		return rc;
	}

	callback_t *cb = new callback_t();
	cb->callback	= callback;
	cb->param		= args;

	// BLE通信安定化のためにWaitを挿入
	// https://github.com/espressif/esp-idf/issues/5105#issuecomment-844641580
	vTaskDelay(100 / portTICK_PERIOD_MS);

	ESP_LOGI(tag, "connect args: %p, %p, %p", cb, cb->callback, cb->param);
	rc = ble_gap_connect(0, address, 4000, nullptr, blecent_gap_event, cb);

	return rc;
}

int NimbleCentral::disconnect(uint16_t handle, NimbleCallback callback, void *args) {
	int rc;

	rc = ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
	ESP_LOGI(tag, "Connection terminate");

	if (callback != nullptr) callback(0, args);

	return rc;
}

typedef struct {
	NimbleCallback callback_success;
	NimbleCallback callback_failed;
	const ble_uuid_t *service;
	const ble_uuid_t *characteristic;
	bool found_service;
	bool found_characteristic;
	const uint8_t *value;
	size_t length;
	void *param;
} gattc_callback_args_t;

int NimbleCentral::chr_disced(uint16_t conn_handle, const struct ble_gatt_error *error,
						const struct ble_gatt_chr *chr, void *arg) {
	int rc;

	ESP_LOGI(tag, "chara event status: %d", error->status);

	gattc_callback_args_t *client = (gattc_callback_args_t *)arg;

	switch (error->status) {
		case 0:
			client->found_characteristic = true;

			rc = ble_gattc_write_no_rsp_flat(conn_handle, chr->val_handle, client->value, client->length);
			if (client->callback_success != nullptr) rc = client->callback_success(conn_handle, client->param);
			break;

		case BLE_HS_EALREADY:
		case BLE_HS_EBUSY:
		case BLE_HS_EDONE:
		default:
			rc = error->status;

			ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
			delete arg;
			ESP_LOGI(tag, "Connection terminate");
			break;
	}

	if (rc != 0 && !client->found_characteristic) {
		ESP_LOGE(tag, "Failed find chr or write characteristic");
		if (client->callback_failed != nullptr) client->callback_failed(conn_handle, client->param);
	}

	ESP_LOGI(tag, "Finish cmd press");

	return rc;
}

int NimbleCentral::svc_disced(uint16_t conn_handle, const struct ble_gatt_error *error,
						const struct ble_gatt_svc *service, void *arg) {
	int rc;

	ESP_LOGI(tag, "service event status: %d", error->status);

	gattc_callback_args_t *client = (gattc_callback_args_t *)arg;

	switch (error->status) {
		case 0:  // Success
			client->found_service = true;

			rc = ble_gattc_disc_chrs_by_uuid(conn_handle, service->start_handle, service->end_handle,
									   client->characteristic, chr_disced, client);
			break;

		case BLE_HS_EDONE:
			if (client->found_service) {
				ESP_LOGI(tag, "Finding service finish.");
				rc = 0;
			} else {
				ESP_LOGE(tag, "Couldn't find service uuid.");
				ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
				if (client->callback_failed != nullptr) client->callback_failed(conn_handle, client->param);
				rc = error->status;
				delete arg;
			}
			break;

		default:
			rc = error->status;
			break;
	}

	if (rc != 0) {
		ESP_LOGE(tag, "Service discover error: %d", rc);
	}

	return rc;
}

int NimbleCentral::write(uint16_t handle,
					const ble_uuid_t *service, const ble_uuid_t *characteristic,
					const uint8_t *value, size_t length, int timeout,
					NimbleCallback success, NimbleCallback failed, void *args) {
	int rc;
	gattc_callback_args_t *arg = new gattc_callback_args_t();
	arg->callback_success	  = success;
	arg->callback_failed	  = failed;
	arg->service			  = service;
	arg->characteristic		  = characteristic;
	arg->found_service		  = false;
	arg->found_characteristic  = false;
	arg->value			  = value;
	arg->length			  = length;
	arg->param			  = args;

	rc = ble_gattc_disc_svc_by_uuid(handle, service, svc_disced, arg);

	if (rc != 0) {
		ESP_LOGI(tag, "Failed find characteristics");
	}

	return rc;
}
