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

	struct upload {
		std::string filename;
		std::vector<uint8_t> data;

		template <typename Data>
		upload(std::string filename, Data const & data)
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
	response read(std::string const & skylink);

	template <typename Data>
	std::string write(Data const & data, std::string const & filename)
	{
		return write(std::vector<uint8_t>(std::begin(data), std::end(data)), filename);
	}
	std::string write(std::vector<uint8_t> const & data, std::string const & filename);
	std::string write(std::vector<upload> const & files, std::string const & filename);
};

}
