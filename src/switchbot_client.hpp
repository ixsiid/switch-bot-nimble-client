#pragma once

#include <nimble/ble.h>
#include <host/ble_uuid.h>
#include "nimble_central.hpp"

class SwitchBotClient {
private:
	static bool is_nimble_started;
	static void nimble_start();

	ble_addr_t address;

	NimbleCentral * central;
	

	int send(const uint8_t * command, size_t length);
public:
	SwitchBotClient(const char * peer_address);
	int press();
	int push();
	int pull();
	
	static const ble_uuid128_t service, characteristic;
	static const uint8_t command_press[3];
	static const uint8_t command_push[3];
	static const uint8_t command_pull[3];
};

inline int SwitchBotClient::press() { return send(command_press, sizeof(command_press)); }
inline int SwitchBotClient::push() { return send(command_push, sizeof(command_push)); }
inline int SwitchBotClient::pull() { return send(command_pull, sizeof(command_pull)); }