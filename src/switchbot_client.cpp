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
	NimbleCallback disconnecting;
	NimbleCallback connecting;
	NimbleCallback writing;
} callback_args_t;

int SwitchBotClient::send(const uint8_t *command, size_t length) {
	callback_args_t *args = new callback_args_t();
	args->address		  = &address;
	args->central		  = central;
	args->service		  = (const ble_uuid_t *)&service;
	args->characteristic  = (const ble_uuid_t *)&characteristic;
	args->command		  = command;
	args->length		  = length;

	ESP_LOGI(tag, "start connect");

	args->disconnecting = [](uint16_t handle, void *args) {
		ESP_LOGI(tag, "written, disconnecting");
		callback_args_t *arg = (callback_args_t *)args;
		arg->central->disconnect(handle, nullptr, nullptr);
		return 0;
	};

	args->connecting = [](uint16_t handle, void *args) {
		ESP_LOGI(tag, "reconnecting, start write");
		callback_args_t *arg = (callback_args_t *)args;
		arg->central->connect(arg->address, arg->writing, args);
		return 0;
	};

	args->writing = [](uint16_t handle, void *args) {
		ESP_LOGI(tag, "connected, start write");
		callback_args_t *arg = (callback_args_t *)args;
		arg->central->write(handle, arg->service, arg->characteristic, arg->command, arg->length, 10000,
						arg->disconnecting, arg->connecting, args);
		return 0;
	};

	args->connecting(0, args);

	return 0;
}
