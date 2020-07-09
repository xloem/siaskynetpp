#pragma once

#include "siaskynet.hpp"

#include <condition_variable>
#include <map>
#include <mutex>

namespace sia {

class skynet_multiportal {
public:

	skynet_multiportal(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000), bool do_not_set_up_portals = false);

	/*
	skynet::response query(std::string const & skylink, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	skynet::response download(std::string const & skylink, std::initializer_list<std::pair<size_t, size_t>> ranges = {}, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	skynet::response download_file(std::string const & path, std::string const & skylink, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

	template <typename Data>
	std::string upload(std::string const & filename, Data const & data, std::string const & contenttype = {}, unsigned long parallelism = 1, std::chrono::milliseconds timeout = std::chrono::milliseconds(0))
	{
		return upload(skynet::upload_data{filename, data, contenttype}, parallelism, timeout);
	}
	std::string upload(skynet::upload_data const & file, unsigned long parallelism = 1, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	std::string upload(std::string const & filename, std::vector<upload_data> const & files, unsigned long parallelism = 1, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	std::string upload_file(std::string const & path, std::string filename = "", unsigned long parallelism = 1, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	*/
	// only problem with multiportal-download is the user doesn't know what portal is being used before completion
	// it would be nice to get an in-progress-transfer return value
	// it seems this would go in a separate class, though

	enum transfer_kind {
		download = 0,
		upload = 1,
		transfer_kind_count = 2
	};

	struct transfer {
		transfer_kind kind;
		skynet::portal_options portal;
		std::chrono::steady_clock::time_point start_time;
	};

	// call when starting a transfer to select a portal and track metrics
	transfer begin_transfer(transfer_kind kind, skynet::portal_options portal = {});

	// call when transfer is done to record metrics and reuse portal
	void end_transfer(transfer, unsigned long amount_successfully_transferred);

	struct portal_metrics {
		skynet::portal_options portal;
		struct metric {
			double speed = 0;
			unsigned long long data = 0;
			std::chrono::steady_clock::duration time = std::chrono::steady_clock::duration(0);
			std::mutex mutex;
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
