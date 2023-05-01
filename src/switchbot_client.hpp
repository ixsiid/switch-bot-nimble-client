#pragma once

#include <nimble/ble.h>
#include <host/ble_uuid.h>
#include <simple_nimble_central.hpp>

class SwitchBotClient {
private:	
	static const ble_uuid128_t service, characteristic;
	static const uint8_t command_press[3];
	static const uint8_t command_push[3];
	static const uint8_t command_pull[3];

	ble_addr_t address;

	SimpleNimbleCentral * central;
	

	int send(const uint8_t * command, size_t length);
	bool send_async(const uint8_t *command, size_t length);
public:
	SwitchBotClient(const char * peer_address);
	int press();
	int push();
	int pull();

	bool press_async();
	bool push_async();
	bool pull_async();
};

inline int SwitchBotClient::press() { return send(command_press, sizeof(command_press)); }
inline int SwitchBotClient::push() { return send(command_push, sizeof(command_push)); }
inline int SwitchBotClient::pull() { return send(command_pull, sizeof(command_pull)); }

inline bool SwitchBotClient::press_async() { return send_async(command_press, sizeof(command_press)); }
inline bool SwitchBotClient::push_async() { return send_async(command_push, sizeof(command_push)); }
inline bool SwitchBotClient::pull_async() { return send_async(command_pull, sizeof(command_pull)); }
