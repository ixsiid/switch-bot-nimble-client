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

SwitchBotClient::SwitchBotClient(const char *peer_address) : central(SimpleNimbleCentral::get_instance()) {
	address.type = BLE_ADDR_RANDOM;
	sscanf(peer_address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		  &address.val[5], &address.val[4], &address.val[3],
		  &address.val[2], &address.val[1], &address.val[0]);
}

int SwitchBotClient::send(const uint8_t *command, size_t length) {
	return -1;
}

bool SwitchBotClient::send_async(const uint8_t *command, size_t length) {
	if (!central->connect(&address)) return false;
	if (!central->find_service((const ble_uuid_t *)&service)) return false;
	if (!central->find_characteristic((const ble_uuid_t *)&characteristic)) return false;
	if (!central->write(command, length)) return false;
	return true;
}
