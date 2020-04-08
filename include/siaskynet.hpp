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

		template <typename Data>
		upload_data(std::string filename, Data const & data)
		: filename(filename), data(std::begin(data), std::end(data))
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
			std::vector<std::pair<std::string, subfile>> subfiles;
		} metadata;

		std::vector<uint8_t> data;
	};

	skynet(portal_options const & options = default_options);
	portal_options options;

	response query(std::string const & skylink);
	response download(std::string const & skylink);

	template <typename Data>
	std::string upload(std::string const & filename, Data const & data)
	{
		return upload(upload_data{filename, data});
	}
	std::string upload(upload_data const & file);
	std::string upload(std::string const & filename, std::vector<upload_data> const & files);
};

}
