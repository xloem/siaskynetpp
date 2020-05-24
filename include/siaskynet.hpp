#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace sia {

class skynet {
public:
	struct portal_options {
		std::string url;
		std::string uploadPath;
		std::string fileFieldname;
		std::string directoryFileFieldname;
	};
	static portal_options default_options();
	static std::vector<portal_options> portals();

	struct upload_data {
		std::string filename;
		std::vector<uint8_t> data;
		std::string contenttype;

		template <typename Data>
		upload_data(std::string filename, Data const & data, std::string contenttype = {})
		: filename(filename), data(std::begin(data), std::end(data)), contenttype(contenttype)
		{ }
	};

	struct response {
		std::string skylink;
		portal_options portal;

		std::string filename;

		struct subfile {
			std::string contenttype;
			std::string filename;
			size_t len;
			size_t offset;
			std::vector<std::pair<std::string, subfile>> subfiles;
		} metadata;

		std::vector<uint8_t> data;
		std::vector<std::pair<size_t,size_t>> dataranges;
	};

	skynet();
	skynet(portal_options const & options);
	portal_options options;

	response query(std::string const & skylink, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	response download(std::string const & skylink, std::initializer_list<std::pair<size_t, size_t>> ranges = {}, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	response download_file(std::string const & skylink, std::string path = "", std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	response download_directory(std::string const & skylink, std::string path = "", std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

	template <typename Data>
	std::string upload(std::string const & filename, Data const & data, std::string const & contenttype = {}, std::chrono::milliseconds timeout = std::chrono::milliseconds(0))
	{
		return upload(upload_data{filename, data, contenttype}, timeout);
	}
	std::string upload(upload_data const & file, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	std::string upload(std::string const & filename, std::vector<upload_data> const & files, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	std::string upload_file(std::string const & path, std::string filename = "", std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
	std::string upload_directory(std::string const & path, std::string filename = "", std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
};

}
