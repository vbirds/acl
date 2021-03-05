#pragma once

#include "mqtt_message.hpp"

namespace acl {

class mqtt_suback : public mqtt_message {
public:
	mqtt_suback(unsigned payload_len = 0);
	~mqtt_suback(void);

	void set_pkt_id(unsigned short id);
	void add_topic_qos(mqtt_qos_t qos);

protected:
	// @override
	bool to_string(string& out);

	// @override
	int update(const char* data, int dlen);

	// @override
	bool is_finished(void) const {
		return finished_;
	}

public:
	int update_header_var(const char* data, int dlen);
	int update_topic_qos(const char* data, int dlen);

private:
	bool finished_;
	char buff_[2];
	unsigned dlen_;

	unsigned short pkt_id_;
	std::vector<mqtt_qos_t> qoses_;

	unsigned body_len_;
	unsigned nread_;

};

} // namespace acl