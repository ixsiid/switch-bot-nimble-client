#include <stdio.h>

#include "switchbot_client.hpp"

#include <esp_log.h>
#include <nvs_flash.h>
#include <host/ble_gap.h>
#include <services/gap/ble_svc_gap.h>
#include <nimble/nimble_port_freertos.h>
#include <nimble/nimble_port.h>
#include <esp_nimble_hci.h>
#include <host/util/util.h>

#include "misc.h"

#define DEVICE_NAME "SBC"

const char * tag = "SB_CLIENT";

bool SwitchBotClient::is_nimble_started = false;

const uint8_t SwitchBotClient::command_press[3] = {0x57, 0x01, 0x00};
const uint8_t SwitchBotClient::command_push[3]  = {0x57, 0x01, 0x01};
const uint8_t SwitchBotClient::command_pull[3]  = {0x57, 0x01, 0x02};


static int blecent_gap_event(struct ble_gap_event *event, void *arg);

/**
 * Initiates the GAP general discovery procedure.
 */
static void blecent_scan(void) {
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
		MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n",
				  rc);
	}
}

static int chr_disced(uint16_t conn_handle, const struct ble_gatt_error *error,
				  const struct ble_gatt_chr *chr, void *arg) {
	int rc;

	ESP_LOGI(tag, "chara event status: %d", error->status);

	switch (error->status) {
		case 0:
			// chr発見
			rc = ble_gattc_write_no_rsp_flat(conn_handle, chr->val_handle, SwitchBotClient::command_press, 3);
			break;

		case BLE_HS_EALREADY:
		case BLE_HS_EBUSY:
		case BLE_HS_EDONE:
		default:
			rc = error->status;
			break;
	}

	if (rc != 0) {
		ESP_LOGE(tag, "Failed find chr or write characteristic");
	}

	ESP_LOGI(tag, "Finish cmd press");

	ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
	ESP_LOGI(tag, "Connection terminate");

	return rc;
}

static int svc_disced(uint16_t conn_handle, const struct ble_gatt_error *error,
				  const struct ble_gatt_svc *service, void *arg) {
	int rc;

	ESP_LOGI(tag, "service event status: %d", error->status);

	SwitchBotClient * client = (SwitchBotClient *)arg;

	switch (error->status) {
		case 0:
			rc = ble_gattc_disc_chrs_by_uuid(conn_handle, service->start_handle, service->end_handle,
									   (const ble_uuid_t *)&client->characteristic, chr_disced, arg);
			break;

		default:
			rc = error->status;
			break;
	}

	if (rc != 0) {
		ESP_LOGE(tag, "Failed find svc");
	}

	return rc;
}

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
static int blecent_gap_event(struct ble_gap_event *event, void *arg) {
	struct ble_gap_conn_desc desc;
	int rc;

	SwitchBotClient * client = (SwitchBotClient *)arg;

	ESP_LOGV(tag, "blecent_gap_event: %d", event->type);

	switch (event->type) {
		case BLE_GAP_EVENT_DISC:
			MODLOG_DFLT(DEBUG, "discovery; type=%d", event->disc.event_type);
			return 0;

		case BLE_GAP_EVENT_CONNECT:
			/* A new connection was established or a connection attempt failed. */
			if (event->connect.status == 0) {
				/* Connection successfully established. */
				MODLOG_DFLT(INFO, "Connection established ");
				ESP_LOGI(tag, "Connect");

				rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
				assert(rc == 0);
				print_conn_desc(&desc);
				MODLOG_DFLT(INFO, "\n");

				// 自前Chr検索
				uint16_t handle = event->connect.conn_handle;
				rc			 = ble_gattc_disc_svc_by_uuid(handle, (const ble_uuid_t *)&client->service, svc_disced, arg);
				if (rc != 0) {
					ESP_LOGI(tag, "Failed find characteristics");
					return rc;
				}

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

static void blecent_on_reset(int reason) { MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason); }

static void blecent_on_sync(void) {
	int rc;

	/* Make sure we have proper identity address set (public preferred) */
	rc = ble_hs_util_ensure_addr(0);
	assert(rc == 0);

	/* Begin scanning for a peripheral to connect to. */
	blecent_scan();
}

void blecent_host_task(void *param) {
	ESP_LOGI(tag, "BLE Host Task Started");
	/* This function will return only when nimble_port_stop() is executed */
	nimble_port_run();

	nimble_port_freertos_deinit();
}

// ------------------------------

void SwitchBotClient::nimble_start() {
	if (is_nimble_started) return;

	int rc;

	ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());

	nimble_port_init();
	/* Configure the host. */
	ble_hs_cfg.reset_cb		  = blecent_on_reset;
	ble_hs_cfg.sync_cb		  = blecent_on_sync;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

	/* Set the default device name. */
	rc = ble_svc_gap_device_name_set(DEVICE_NAME);
	assert(rc == 0);

	nimble_port_freertos_init(blecent_host_task);

	is_nimble_started = true;
}

SwitchBotClient::SwitchBotClient(const char *peer_address,
						   const char *service_uuid,
						   const char *characteristic_uuid) {
	nimble_start();

	address.type = BLE_ADDR_RANDOM;
	sscanf(peer_address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		  &address.val[5], &address.val[4], &address.val[3],
		  &address.val[2], &address.val[1], &address.val[0]);
	service.u.type = BLE_UUID_TYPE_128;
	sscanf(service_uuid, 
		  "%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-"
		  "%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
		  &service.value[15], &service.value[14], &service.value[13], &service.value[12],
		  &service.value[11], &service.value[10], &service.value[9], &service.value[8],
		  &service.value[7], &service.value[6], &service.value[5], &service.value[4],
		  &service.value[3], &service.value[2], &service.value[1], &service.value[0]);
	characteristic.u.type = BLE_UUID_TYPE_128;
	sscanf(characteristic_uuid, 
		  "%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-"
		  "%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
		  &characteristic.value[15], &characteristic.value[14],
		  &characteristic.value[13], &characteristic.value[12],
		  &characteristic.value[11], &characteristic.value[10],
		  &characteristic.value[9], &characteristic.value[8],
		  &characteristic.value[7], &characteristic.value[6],
		  &characteristic.value[5], &characteristic.value[4],
		  &characteristic.value[3], &characteristic.value[2],
		  &characteristic.value[1], &characteristic.value[0]);
}

int SwitchBotClient::send(const uint8_t * command) {
	int rc;
	/* Scanning must be stopped before a connection can be initiated. */
	rc = ble_gap_disc_cancel();
	if (rc != 0) {
		MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
		return rc;
	}

	// BLE通信安定化のためにWaitを挿入
	// https://github.com/espressif/esp-idf/issues/5105#issuecomment-844641580
	vTaskDelay(100 / portTICK_PERIOD_MS);
	rc = ble_gap_connect(0, &address, 4000, nullptr, blecent_gap_event, (void *)this);

	return rc;
}
