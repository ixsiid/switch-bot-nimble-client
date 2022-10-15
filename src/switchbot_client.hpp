#include <nimble/ble.h>
#include <host/ble_uuid.h>

class SwitchBotClient {
private:
	static bool is_nimble_started;
	static void nimble_start();

	ble_addr_t address;
	

	int send(const uint8_t * command);
public:
	SwitchBotClient(const char * peer_address, const char * service_uuid, const char * characteristic_uuid);
	int press();
	int push();
	int pull();
	
	ble_uuid128_t service, characteristic;
	static const uint8_t command_press[3];
	static const uint8_t command_push[3];
	static const uint8_t command_pull[3];
};

inline int SwitchBotClient::press() { return send(command_press); }
inline int SwitchBotClient::push() { return send(command_push); }
inline int SwitchBotClient::pull() { return send(command_pull); }
