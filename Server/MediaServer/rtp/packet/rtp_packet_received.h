/*
 *  Copyright 2020 The mediaserver Project Authors. All rights reserved.
 *
 *  Created on: 2020/07/16
 *      Author: max
 *		Email: Kingsleyyau@gmail.com
 *
 *  Borrow from WebRTC Project
 */

#ifndef RTP_PACKET_RTP_PACKET_RECEIVED_H_
#define RTP_PACKET_RTP_PACKET_RECEIVED_H_

#include <stdint.h>

#include <vector>

#include <rtp/api/array_view.h>
#include <rtp/api/rtp_headers.h>
#include <rtp/packet/rtp_packet.h>
#include <rtp/base/ntp_time.h>

namespace qpidnetwork {
// Class to hold rtp packet with metadata for receiver side.
class RtpPacketReceived: public RtpPacket {
public:
	RtpPacketReceived();
	explicit RtpPacketReceived(const ExtensionManager* extensions);
	RtpPacketReceived(const RtpPacketReceived& packet);
	RtpPacketReceived(RtpPacketReceived&& packet);

	RtpPacketReceived& operator=(const RtpPacketReceived& packet);
	RtpPacketReceived& operator=(RtpPacketReceived&& packet);

	~RtpPacketReceived();

	// TODO(danilchap): Remove this function when all code update to use RtpPacket
	// directly. Function is there just for easier backward compatibilty.
	void GetHeader(RTPHeader* header) const;

	// Time in local time base as close as it can to packet arrived on the
	// network.
	int64_t arrival_time_ms() const {
		return arrival_time_ms_;
	}
	void set_arrival_time_ms(int64_t time) {
		arrival_time_ms_ = time;
	}

	// Estimated from Timestamp() using rtcp Sender Reports.
	NtpTime capture_ntp_time() const {
		return capture_time_;
	}
	void set_capture_ntp_time(NtpTime time) {
		capture_time_ = time;
	}

	// Flag if packet was recovered via RTX or FEC.
	bool recovered() const {
		return recovered_;
	}
	void set_recovered(bool value) {
		recovered_ = value;
	}

	int payload_type_frequency() const {
		return payload_type_frequency_;
	}
	void set_payload_type_frequency(int value) {
		payload_type_frequency_ = value;
	}

	// Additional data bound to the RTP packet for use in application code,
	// outside of WebRTC.
	qpidnetwork::ArrayView<const uint8_t> application_data() const {
		return application_data_;
	}
	void set_application_data(qpidnetwork::ArrayView<const uint8_t> data) {
		application_data_.assign(data.begin(), data.end());
	}

private:
	NtpTime capture_time_;
	int64_t arrival_time_ms_ = 0;
	int payload_type_frequency_ = 0;
	bool recovered_ = false;
	std::vector<uint8_t> application_data_;
};

}  // namespace qpidnetwork
#endif  // RTP_PACKET_RTP_PACKET_RECEIVED_H_
