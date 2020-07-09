#include <siaskynet.hpp>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_set>

namespace sia {


static void read_file(std::string const & path, std::vector<uint8_t> & buffer);
static void write_file(std::string const & path, std::vector<uint8_t> const & buffer);
static std::string uploadToField(std::vector<skynet::upload_data> const & files, std::string const & filename, std::string const & url, std::string const & field, std::chrono::milliseconds timeout);
static std::string trimSiaPrefix(std::string const & skylink);
static std::string trimTrailingSlash(std::string const & url);
static skynet::response::subfile parseCprResponse(cpr::Response & response);
static std::string extractContentDispositionFilename(std::string const & content_disposition);
static skynet::response::subfile parse_subfile(size_t & offset, nlohmann::json const & value);

skynet::portal_options skynet::default_options()
{
	return portals().front();
};

std::vector<skynet::portal_options> skynet::portals()
{
	std::unordered_set<std::string> urls;
	std::vector<skynet::portal_options> result;
	auto response = cpr::Get(cpr::Url{"https://siastats.info/dbs/skynet_current.json"});
	std::string text;
	if (response.error || response.status_code != 200) {
		// retrieved 2020-05-14
		text = R"([{"name":"SiaSky.net","files":[392823,7449,382702,5609],"size":[2.55,0.11,2.3,0.09],"link":"https://siasky.net","chartColor":"#666","version":"1.4.8-master","gitrevision":"a54efe103"},{"name":"SiaCDN.com","files":[659977],"size":[2.65],"link":"https://www.siacdn.com","chartColor":"#666","version":"1.4.8-master","gitrevision":"d47625aac"},{"name":"SkynetHub.io","files":[5408,0],"size":[0.04,0],"link":"https://skynethub.io","chartColor":"#666","version":"1.4.8-master","gitrevision":"ca21c97fc"},{"name":"SiaLoop.net","files":[40979],"size":[0.18],"link":"https://sialoop.net","chartColor":"#666","version":"1.4.7","gitrevision":"000eccb45"},{"name":"SkyDrain.net","files":[42242,914],"size":[0.23,0.04],"link":"https://skydrain.net","chartColor":"#666","version":"1.4.8","gitrevision":"1eb685ba8"},{"name":"Tutemwesi.com","files":[73918],"size":[0.32],"link":"https://skynet.tutemwesi.com","chartColor":"#666","version":"1.4.6-master","gitrevision":"c2a4d83"},{"name":"Luxor.tech","files":[21754],"size":[0.11],"link":"https://skynet.luxor.tech","chartColor":"#666","version":"1.4.5-master","gitrevision":"e1b995f"},{"name":"LightspeedHosting.com","files":[0],"size":[0],"link":"https://vault.lightspeedhosting.com","chartColor":"#666","version":"","gitrevision":""},{"name":"UTXO.no","files":[0],"size":[0],"link":"https://skynet.utxo.no","chartColor":"#666","version":"","gitrevision":""},{"name":"SkyPortal.xyz","files":[22858,0],"size":[0.14,0],"link":"https://skyportal.xyz","chartColor":"#666","version":"1.4.8","gitrevision":"1eb685ba8"}])";
	} else {
		text = response.text;
	}
	for (auto portal : nlohmann::json::parse(text)) {
		result.emplace_back(portal_options{
			url: portal["link"],
			uploadPath: "/skynet/skyfile",
			fileFieldname: "file",
			directoryFileFieldname: "files[]"
		});
		urls.insert(result.back().url);
	}
	if (!urls.count("https://siasky.dev")) {
		result.emplace_back(portal_options{
			url: "https://siasky.dev",
			uploadPath: "/skynet/skyfile",
			fileFieldname: "file",
			directoryFileFieldname: "files[]"
		});
		urls.insert(result.back().url);
	}
	return result;
}

skynet::skynet()
: options(default_options())
{ }

skynet::skynet(skynet::portal_options const & options)
: options(options)
{ }

std::string trimSiaPrefix(std::string const & skylink)
{
	if (0 == skylink.compare(0, 6, "sia://")) {
		return skylink.substr(6);
	} else {
		return skylink;
	}
}

std::string trimTrailingSlash(std::string const & url)
{
	if (url[url.size() - 1] == '/') {
		return {url.data(), url.size() - 1};
	} else {
		return url;
	}
}

std::string trimLeadingSlash(std::string const & url)
{
	if (url[0] == '/') {
		return {url.data() + 1, url.size() - 1};
	} else {
		return url;
	}
}

std::string skynet::upload_file(std::string const & path, std::string filename, std::chrono::milliseconds timeout)
{
	if (!filename.size()) {
		filename = path;
	}

	upload_data data(filename, std::vector<uint8_t>());
	read_file(path, data.data);

	return upload(data, timeout);
}

/*
void skynet::upload_directory(std::string const & path, std::string const & filename)
{
	if (!filename.size()) {
		filename = path;
	}
}
*/

std::string skynet::upload(upload_data const & file, std::chrono::milliseconds timeout)
{
	auto url = cpr::Url{trimTrailingSlash(options.url) + "/" + trimLeadingSlash(trimTrailingSlash(options.uploadPath))};

	return uploadToField({file}, file.filename, url, options.fileFieldname, timeout);
}

std::string skynet::upload(std::string const & filename, std::vector<skynet::upload_data> const & files, std::chrono::milliseconds timeout)
{
	auto url = cpr::Url{trimTrailingSlash(options.url) + "/" + trimLeadingSlash(trimTrailingSlash(options.uploadPath))};

	return uploadToField(files, filename, url, options.directoryFileFieldname, timeout);
}

std::string uploadToField(std::vector<skynet::upload_data> const & files, std::string const & filename, std::string const & url, std::string const & field, std::chrono::milliseconds timeout)
{
	auto parameters = cpr::Parameters{{"filename", filename}};

	cpr::Multipart uploads{};

	for (auto & file : files) {
		uploads.parts.emplace_back(field, cpr::Buffer{file.data.begin(), file.data.end(), file.filename}, file.contenttype);
	}

	auto response = cpr::Post(url, parameters, uploads, cpr::Timeout{timeout});
	if (response.error) {
		throw std::runtime_error(response.error.message);
	} else if (response.status_code != 200) {
		throw std::runtime_error(response.text);
	}
	
	auto json = nlohmann::json::parse(response.text);

	std::string skylink = "sia://" + json["skylink"].get<std::string>();

	return skylink;
}

skynet::response skynet::query(std::string const & skylink, std::chrono::milliseconds timeout)
{
	std::string url = trimTrailingSlash(options.url) + "/" + trimSiaPrefix(skylink);

	auto response = cpr::Head(url, cpr::Parameters{{"format","concat"}}, cpr::Timeout{timeout});
	if (response.error) {
		throw std::runtime_error(response.error.message);
	} else if (response.status_code != 200) {
		throw std::runtime_error("HEAD request failed with status code " + std::to_string(response.status_code));
	}

	skynet::response result;
	result.skylink = skylink;
	result.portal = options;
	result.filename = extractContentDispositionFilename(response.header["content-disposition"]);
	result.metadata = parseCprResponse(response);

	return result;
}

skynet::response skynet::download_file(std::string const & path, std::string const & skylink, std::chrono::milliseconds timeout)
{ 
	response result = download(skylink, {}, timeout);
	
	write_file(path, result.data);

	return result;
}

skynet::response skynet::download(std::string const & skylink, std::initializer_list<std::pair<size_t, size_t>> ranges, std::chrono::milliseconds timeout)
{
	skynet::response result;
	std::string url = trimTrailingSlash(options.url) + "/" + trimSiaPrefix(skylink);

	cpr::Header headers;
	if (ranges.size()) {
		std::string header_content;
		bool first = true;
		for (auto range: ranges) {
			result.dataranges.emplace_back(range);
			if (first) {
				header_content += "bytes=";
			} else {
				header_content += ", ";
			}
			header_content += std::to_string(range.first) + "-" + std::to_string(range.first + range.second - 1);
			first = false;
		}
		headers = cpr::Header{{"Range", header_content}};
	}

	auto response = cpr::Get(url, cpr::Parameters{{"format","concat"}}, headers, cpr::Timeout{timeout});
	if (response.error) {
		throw std::runtime_error(response.error.message);
	} else if (response.status_code != 200) {
		if (!ranges.size() || response.status_code != 206) {
			throw std::runtime_error(response.text);
		}
	} else if (ranges.size()) {
		throw std::runtime_error("Server does not support partial ranges.");
	}

	result.skylink = skylink;
	result.portal = options;
	result.filename = extractContentDispositionFilename(response.header["content-disposition"]);
	result.metadata = parseCprResponse(response);
	result.data = std::vector<uint8_t>(response.text.begin(), response.text.end());
	if (!ranges.size()) {
		result.dataranges.emplace_back(0, result.metadata.len);
	}

	return result;
}

skynet::response::subfile parse_subfile(size_t & offset, nlohmann::json const & value)
{
	size_t suboffset = offset;

	skynet::response::subfile metadata;
	metadata.contenttype = value["contenttype"].get<std::string>();
	metadata.len = value["len"].get<size_t>();
	metadata.filename = value["filename"].get<std::string>();

	metadata.offset = offset;
	offset += metadata.len; // this adds to suboffset when recursively called

	if (!value.contains("subfiles")) {
		return metadata;
	}

	for (auto & subfile : value["subfiles"].items()) {
		metadata.subfiles.emplace_back(subfile.key(), parse_subfile(suboffset, subfile.value()));
	}

	if (suboffset > offset) {
		// this is intended to fix those times when content-length is not provided and the outermost metadata.len is 0
		metadata.len += suboffset - offset;
		offset = suboffset;
	}

	return metadata;
}

skynet::response::subfile parseCprResponse(cpr::Response & cpr)
{
	auto & raw_json = cpr.header["skynet-file-metadata"];
	auto parsed_json = nlohmann::json::parse(raw_json);
	auto len = cpr.header["content-length"];
	parsed_json["len"] = len.size() ? std::stoul(cpr.header["content-length"]) : 0;
	parsed_json["contenttype"] = cpr.header["content-type"];

	size_t offset = 0;
	return parse_subfile(offset, parsed_json);
}

std::string extractContentDispositionFilename(std::string const & content_disposition)
{
	auto start = content_disposition.find("filename=");
	if (start == std::string::npos) { return {}; }

	start += 9;
	auto first_char = content_disposition[start];
	auto last_char = ';';
	if (first_char == '\'' || first_char == '"') {
		++ start;
		last_char = first_char;
	}
	
	auto end = content_disposition.find(last_char, start);
	if (end == std::string::npos) {
		end = content_disposition.size();
	}

	return { content_disposition.begin() + start, content_disposition.begin() + end };
}

static void read_file(std::string const & path, std::vector<uint8_t> & buffer)
{
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

	if (!file.is_open()) { throw std::runtime_error("Failed to open " + path); }

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	buffer.resize(size);
	file.read((char*)buffer.data(), size * sizeof(uint8_t) / sizeof(char));

	if (file.fail()) { throw std::runtime_error("Failed to read contents of " + path); }
}

static void write_file(std::string const & path, std::vector<uint8_t> const & buffer)
{
	std::ofstream file(path.c_str(), std::ios::out | std::ios::binary);

	if (!file.is_open()) { throw std::runtime_error("Failed to open " + path); }

	file.write((char*)buffer.data(), buffer.size() * sizeof(uint8_t) / sizeof(char));

	if (file.fail()) { throw std::runtime_error("Failed to write contents of " + path); }
}

}
