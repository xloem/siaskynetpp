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
	static portal_options default_options;

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
	};

	skynet(portal_options const & options = default_options);
	portal_options options;

	response query(std::string const & skylink);
	response download(std::string const & skylink);
	response download_file(std::string const & path, std::string const & skylink);

	template <typename Data>
	std::string upload(std::string const & filename, Data const & data, std::string const & contenttype = {})
	{
		return upload(upload_data{filename, data, contenttype});
	}
	std::string upload(upload_data const & file);
	std::string upload(std::string const & filename, std::vector<upload_data> const & files);
	std::string upload_file(std::string const & path, std::string filename = "");
	//TODO: void upload_directory(std::string const & path);
};

}
