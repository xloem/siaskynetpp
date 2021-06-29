#include <siaskynet_multiportal.hpp>

#include <chrono>
#include <thread>
#include <random>

namespace sia {

skynet_multiportal::skynet_multiportal(std::chrono::milliseconds timeout, bool do_not_set_up_portals)
{
	if (do_not_set_up_portals) { return; }

	for (auto portal : skynet::portals()) {
		ensure_portal(portal);
	}

	measure_portals(timeout);
}

bool skynet_multiportal::measure_portals(std::chrono::milliseconds timeout)
{
	// each portal is tried in parallel, and data is filled
	// in as results come in.
	// once there is both an upload and download portal known to work,
	// the constructor returns.

	std::unique_lock<std::mutex> lock(mutex);
	std::condition_variable transferred;
	std::shared_ptr<std::pair<bool,bool>> transferred_successfully(new std::pair<bool,bool>(false, false));

	std::string filename = "test";
	std::string data; data.reserve(1024); data.resize(1024);
	{
		std::random_device rd;
		std::uniform_int_distribution<char> dist(-128,127);
		for (char & d : data) {
			d = dist(rd);
		}
	}


	for (auto & portal_entry : portals) {
		auto & portal = portal_entry.second;
		
		std::thread([this, &portal, timeout, data, &transferred, transferred_successfully]() {
			auto transfer = begin_transfer(download, portal.portal);
	
			skynet downloader(portal.portal);
			try {
				auto result = downloader.download("sia://AAA2s82WUW1c73RYIcAb3PnBPHcFdHZ7XfleMkrDnueCXQ/test", {}, timeout);
				end_transfer(transfer, result.filename.size() + result.data.size());
				std::unique_lock<std::mutex> lock(mutex);
				transferred_successfully->first = true;
				transferred.notify_all();
			} catch (...) {
				end_transfer(transfer, 1);
			}
		}).detach();
		std::thread([this, &portal, timeout, data, &transferred, transferred_successfully, filename]() {
			auto transfer = begin_transfer(upload, portal.portal);
			skynet uploader(portal.portal);
			try {
				uploader.upload(filename, data, {}, timeout);
				end_transfer(transfer, filename.size() + data.size());
				std::unique_lock<std::mutex> lock(mutex);
				transferred_successfully->second = true;
				transferred.notify_all();
			} catch (...) {
				end_transfer(transfer, 1);
			}
		}).detach();
	}

	// false if timeout is hit without success, hopefully
	return transferred.wait_for(
		lock, timeout, [&] {
			return transferred_successfully->first && transferred_successfully->second;
		}
	);
}

void skynet_multiportal::ensure_portal(skynet::portal_options portal)
{
	std::lock_guard<std::mutex> lock(mutex);
	portals[portal.url].portal = portal;
}

skynet_multiportal::portal_metrics const & skynet_multiportal::metrics(std::string url)
{
	std::lock_guard<std::mutex> lock(mutex);
	return portals[url];
}

skynet_multiportal::transfer skynet_multiportal::begin_transfer(transfer_kind kind, skynet::portal_options portal)
{
	if (portal.url.size()) {
		ensure_portal(portal);
	}
	std::unique_lock<std::mutex> lock(mutex);
	if (portal.url.size()) {
		portals[portal.url].metrics[kind].mutex.lock();
		return {
			kind,
			portal,
			std::chrono::steady_clock::now()
		};
	}
	portal_metrics * best_portal = 0;
	portal_metrics::metric * best_metric = 0;
	double best_speed = 0;
	while (!best_portal) {
		for (auto & portal_entry : portals) {
			auto & portal = portal_entry.second;
			auto & metric = portal.metrics[kind];
			if (metric.mutex.try_lock()) {
				if (!best_speed || metric.speed > best_speed) {
					if (best_portal) {
						best_metric->mutex.unlock();
					}
					best_portal = &portal;
					best_metric = &metric;
					best_speed = metric.speed;
				} else {
					metric.mutex.unlock();
				}
			}
		}
		if (!best_portal) {
			transferred[kind].wait(lock);
		}
	}
	return {
		kind,
		best_portal->portal,
		std::chrono::steady_clock::now()
	};
}

void skynet_multiportal::end_transfer(skynet_multiportal::transfer transfer, unsigned long amount_successfully_transferred)
{
	std::lock_guard<std::mutex> lock(mutex);
	portal_metrics & portal = portals[transfer.portal.url];
	portal_metrics::metric & metric = portal.metrics[transfer.kind];
	
	metric.data += amount_successfully_transferred;
	metric.time += std::chrono::steady_clock::now() - transfer.start_time;
	metric.speed = metric.data / std::chrono::duration<double>(metric.time).count();
	metric.mutex.unlock();

	transferred[transfer.kind].notify_all();
}

} // namespace sia
