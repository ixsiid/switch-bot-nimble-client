#include <esp_log.h>
#include "switchbot_client.hpp"

// SwitchBot Bot:
//        Service UUID: "cba20d00-224d-11e6-9fb8-0002a5d5c51b"
// Characteristic UUID: "cba20002-224d-11e6-9fb8-0002a5d5c51b"

#define tag "SwithBotClient"

const ble_uuid128_t SwitchBotClient::service = {
    .u	 = {.type = BLE_UUID_TYPE_128},
    .value = {0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0xb8, 0x9f, 0xe6, 0x11, 0x4d, 0x22, 0x00, 0x0d, 0xa2, 0xcb},
};

const ble_uuid128_t SwitchBotClient::characteristic = {
    .u	 = {.type = BLE_UUID_TYPE_128},
    .value = {0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0xb8, 0x9f, 0xe6, 0x11, 0x4d, 0x22, 0x02, 0x00, 0xa2, 0xcb},
};

const uint8_t SwitchBotClient::command_press[3] = {0x57, 0x01, 0x00};
const uint8_t SwitchBotClient::command_push[3]  = {0x57, 0x01, 0x03};
const uint8_t SwitchBotClient::command_pull[3]  = {0x57, 0x01, 0x04};

SwitchBotClient::SwitchBotClient(const char *peer_address) {
	address.type = BLE_ADDR_RANDOM;
	sscanf(peer_address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		  &address.val[5], &address.val[4], &address.val[3],
		  &address.val[2], &address.val[1], &address.val[0]);

	central = new NimbleCentral();
	int rc  = central->start("SBClient");
	assert(rc == 0);
}

typedef struct {
	const ble_addr_t *address;
	NimbleCentral *central;
	const ble_uuid_t *service;
	const ble_uuid_t *characteristic;
	const uint8_t *command;
	size_t length;
	NimbleCallback callback;
	NimbleCallback connecting;
	NimbleCallback writing;
} callback_args_t;

int SwitchBotClient::send(const uint8_t *command, size_t length) {
	static callback_args_t *args = nullptr;

	if (args != nullptr) {
		ESP_LOGI(tag, "busy");
		return 1;
	}

	args = new callback_args_t();
	args->address		  = &address;
	args->central		  = central;
	args->service		  = (const ble_uuid_t *)&service;
	args->characteristic  = (const ble_uuid_t *)&characteristic;
	args->command		  = command;
	args->length		  = length;

	ESP_LOGI(tag, "start connect");

	args->callback = [](uint16_t handle, NimbleCallbackReason reason) {
		switch(reason) {
			case NimbleCallbackReason::SUCCESS:
				ESP_LOGI(tag, "command write success");
				delete args;
				args = nullptr;
				break;
			case NimbleCallbackReason::CONNECTION_START:
			case NimbleCallbackReason::CHARACTERISTIC_WRITE_FAILED:
			case NimbleCallbackReason::CHARACTERISTIC_FIND_FAILED:
			case NimbleCallbackReason::SERVICE_FIND_FAILED:
			case NimbleCallbackReason::STOP_CANCEL_FAILED:
			case NimbleCallbackReason::CONNECTION_FAILED:
			case NimbleCallbackReason::OTHER_FAILED:
				ESP_LOGI(tag, "command failed");
				delete args;
				args = nullptr;
				break;
			case NimbleCallbackReason::CONNECTION_ESTABLISHED:
				ESP_LOGI(tag, "connected, start write");
				args->central->write(handle, args->service, args->characteristic,
						args->command, args->length, 10000,
						args->callback);
				break;
			case NimbleCallbackReason::UNKNOWN:
				ESP_LOGI(tag, "Yobarenai hazu");
				break;
		}
		return 0;
	};
	
	args->central->connect(args->address, args->callback);

	return 0;
}

bool SwitchBotClient::send_async(const uint8_t *command, size_t length) {
	static callback_args_t *args = nullptr;

	if (args != nullptr) {
		ESP_LOGI(tag, "busy");
		return false;
	}

	args = new callback_args_t();
	args->address		  = &address;
	args->central		  = central;
	args->service		  = (const ble_uuid_t *)&service;
	args->characteristic  = (const ble_uuid_t *)&characteristic;
	args->command		  = command;
	args->length		  = length;

	ESP_LOGI(tag, "start connect");

	static int try_count;
	static bool writed;
	
	try_count = 0;
	writed = false;

	args->callback = [](uint16_t handle, NimbleCallbackReason reason) {
		switch(reason) {
			case NimbleCallbackReason::SUCCESS:
				ESP_LOGI(tag, "command write success");
				writed = true;
				break;
			case NimbleCallbackReason::CONNECTION_START:
			case NimbleCallbackReason::CHARACTERISTIC_WRITE_FAILED:
			case NimbleCallbackReason::CHARACTERISTIC_FIND_FAILED:
			case NimbleCallbackReason::SERVICE_FIND_FAILED:
			case NimbleCallbackReason::STOP_CANCEL_FAILED:
			case NimbleCallbackReason::CONNECTION_FAILED:
			case NimbleCallbackReason::OTHER_FAILED:
				if (++try_count > 3) return 1;
				ESP_LOGI(tag, "start connecting");
				args->central->connect(args->address, args->callback);
				break;
			case NimbleCallbackReason::CONNECTION_ESTABLISHED:
				ESP_LOGI(tag, "connected, start write");
				args->central->write(handle, args->service, args->characteristic,
						args->command, args->length, 10000,
						args->callback);
				break;
			case NimbleCallbackReason::UNKNOWN:
				ESP_LOGI(tag, "Yobarenai hazu");
				break;
		}
		return 0;
	};

	args->callback(0, NimbleCallbackReason::CONNECTION_START);

	while (try_count < 3 && !writed) {
		ESP_LOGI(tag, "Try...");
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

	ESP_LOGI(tag, "Writing result: %d, by %d try", writed, try_count);
	delete args;
	args = nullptr;

	return writed;
}