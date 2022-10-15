#include "switchbot_client.hpp"

// SwitchBot Bot:
//        Service UUID: "cba20d00-224d-11e6-9fb8-0002a5d5c51b"
// Characteristic UUID: "cba20002-224d-11e6-9fb8-0002a5d5c51b"

const ble_uuid128_t SwitchBotClient::service = {
    .u	 = {.type = BLE_UUID_TYPE_128},
    .value = {0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0xb8, 0x9f, 0xe6, 0x11, 0x4d, 0x22, 0x00, 0x0d, 0xa2, 0xcb},
};

const ble_uuid128_t SwitchBotClient::characteristic = {
    .u	 = {.type = BLE_UUID_TYPE_128},
    .value = {0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0xb8, 0x9f, 0xe6, 0x11, 0x4d, 0x22, 0x02, 0x00, 0xa2, 0xcb},
};

const uint8_t SwitchBotClient::command_press[3] = {0x57, 0x01, 0x00};
const uint8_t SwitchBotClient::command_push[3]  = {0x57, 0x01, 0x01};
const uint8_t SwitchBotClient::command_pull[3]  = {0x57, 0x01, 0x02};

SwitchBotClient::SwitchBotClient(const char *peer_address) {
	address.type = BLE_ADDR_RANDOM;
	sscanf(peer_address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		  &address.val[5], &address.val[4], &address.val[3],
		  &address.val[2], &address.val[1], &address.val[0]);

	central = new NimbleCentral();
	int rc  = central->start("SB client");
	assert(rc == 0);
}

int SwitchBotClient::send(const uint8_t *command, size_t length) {
	NimbleCallback on_dummy = [](uint16_t) {
		return 0;
	};
	/*

	NimbleCallback on_written = [&](uint16_t handle) {
		central->disconnect(handle, nullptr);
		return 0;
	};

	central->connect(&address, [&](uint16_t handle) {
		central->write(handle, &service, &characteristic, command, length, 10000, [&](uint16_t handle) {
			central->disconnect(handle, nullptr);
			return 0;
		});
		return 0;
	});
	*/

	return 0;
}
