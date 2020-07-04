#pragma once

#include "siaskynet.hpp"

#include <condition_variable>
#include <map>
#include <mutex>

namespace sia {

class skynet_multiportal {
public:

	skynet_multiportal(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000), bool do_not_set_up_portals = false);

	enum transfer_kind {
		download = 0,
		upload = 1,
		transfer_kind_count = 2
	};

	struct transfer {
		transfer_kind kind;
		skynet::portal_options & portal;
		std::chrono::steady_clock::time_point start_time;
	};

	// call when starting a transfer to select a portal and track metrics
	transfer begin_transfer(transfer_kind kind);

	// call when transfer is done to record metrics and reuse portal
	void end_transfer(transfer, unsigned long amount_successfully_transferred);

	struct portal_metrics {
		skynet::portal_options portal;
		struct metric {
			double speed = 0;
			unsigned long long data = 0;
			std::chrono::steady_clock::duration time = std::chrono::steady_clock::duration(0);
			std::mutex mutex;
			std::chrono::steady_clock::time_point last_start_time;
		} metrics[transfer_kind_count];
	};

	// add a portal to the list, or update its options
	void ensure_portal(skynet::portal_options portal);

	// get metrics for a portal by url
	portal_metrics const & metrics(std::string url);

private:
	std::mutex mutex;
	std::condition_variable transferred[transfer_kind_count];

	// note:
	// 	siaskynet_multiportal.cpp uses element pointers while mutex is
	// 	not locked.  std::map sustains that iterators (and hence element
	// 	pointers) are valid across insertion. (std::unordered_map does
	// 	not, so do not use it here)
	std::map<std::string, portal_metrics> portals;
};

} // namespace sia
