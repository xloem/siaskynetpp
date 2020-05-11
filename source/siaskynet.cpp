#include <siaskynet.hpp>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <fstream>

namespace sia {


static void read_file(std::string const & path, std::vector<uint8_t> & buffer);
static void write_file(std::string const & path, std::vector<uint8_t> const & buffer);
static std::string uploadToField(std::vector<skynet::upload_data> const & files, std::string const & filename, std::string const & url, std::string const & field);
static std::string trimSiaPrefix(std::string const & skylink);
static std::string trimTrailingSlash(std::string const & url);
static skynet::response::subfile parseCprResponse(cpr::Response & response);
static std::string extractContentDispositionFilename(std::string const & content_disposition);
static skynet::response::subfile parse_subfile(size_t & offset, nlohmann::json const & value);

skynet::portal_options skynet::default_options = {
	url: "https://siasky.net",
	uploadPath: "/skynet/skyfile",
	fileFieldname: "file",
	directoryFileFieldname: "files[]",
};

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
		std::string result = url;
		result.resize(result.size() - 1);
		return result;
	} else {
		return url;
	}
}

std::string skynet::upload_file(std::string const & path, std::string filename)
{
	if (!filename.size()) {
		filename = path;
	}

	upload_data data(filename, std::vector<uint8_t>());
	read_file(path, data.data);

	return upload(data);
}

/*
void skynet::upload_directory(std::string const & path, std::string const & filename)
{
	if (!filename.size()) {
		filename = path;
	}
}
*/

std::string skynet::upload(upload_data const & file)
{
	auto url = cpr::Url{trimTrailingSlash(options.url) + trimTrailingSlash(options.uploadPath)};

	return uploadToField({file}, file.filename, url, options.fileFieldname);
}

std::string skynet::upload(std::string const & filename, std::vector<skynet::upload_data> const & files)
{
	auto url = cpr::Url{trimTrailingSlash(options.url) + "/" + trimTrailingSlash(options.uploadPath)};

	return uploadToField(files, filename, url, options.directoryFileFieldname);
}

std::string uploadToField(std::vector<skynet::upload_data> const & files, std::string const & filename, std::string const & url, std::string const & field)
{
	auto parameters = cpr::Parameters{{"filename", filename}};

	cpr::Multipart uploads{};

	for (auto & file : files) {
		uploads.parts.emplace_back(field, cpr::Buffer{file.data.begin(), file.data.end(), file.filename}, file.contenttype);
	}

	auto response = cpr::Post(url, parameters, uploads);
	if (response.error) {
		throw std::runtime_error(response.error.message);
	} else if (response.status_code != 200) {
		throw std::runtime_error(response.text);
	}
	
	auto json = nlohmann::json::parse(response.text);

	std::string skylink = "sia://" + json["skylink"].get<std::string>();

	return skylink;
}

skynet::response skynet::query(std::string const & skylink)
{
	std::string url = trimTrailingSlash(options.url) + "/" + trimSiaPrefix(skylink);

	auto response = cpr::Head(url);
	if (response.error) {
		throw std::runtime_error(response.error.message);
	} else if (response.status_code != 200) {
		throw std::runtime_error(response.text);
	}

	skynet::response result;
	result.skylink = skylink;
	result.portal = options;
	result.filename = extractContentDispositionFilename(response.header["content-disposition"]);
	result.metadata = parseCprResponse(response);

	return result;
}

skynet::response skynet::download_file(std::string const & path, std::string const & skylink)
{ 
	response result = download(skylink);
	
	write_file(path, result.data);

	return result;
}

skynet::response skynet::download(std::string const & skylink)
{
	std::string url = trimTrailingSlash(options.url) + "/" + trimSiaPrefix(skylink);

	auto response = cpr::Get(url, cpr::Parameters{{"format","concat"}});
	if (response.error) {
		throw std::runtime_error(response.error.message);
	} else if (response.status_code != 200) {
		throw std::runtime_error(response.text);
	}

	skynet::response result;
	result.skylink = skylink;
	result.portal = options;
	result.filename = extractContentDispositionFilename(response.header["content-disposition"]);
	result.metadata = parseCprResponse(response);
	result.data = std::vector<uint8_t>(response.text.begin(), response.text.end());

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
	offset += metadata.len;

	if (!value.contains("subfiles")) { return metadata; }

	for (auto & subfile : value["subfiles"].items()) {
		metadata.subfiles.emplace_back(subfile.key(), parse_subfile(suboffset, subfile.value()));
	}

	return metadata;
}

skynet::response::subfile parseCprResponse(cpr::Response & cpr)
{
	auto & raw_json = cpr.header["skynet-file-metadata"];
	auto parsed_json = nlohmann::json::parse(raw_json);
	parsed_json["len"] = std::stoul(cpr.header["content-length"]);
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
