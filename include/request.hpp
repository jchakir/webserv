# ifndef REQUEST_HPP
# define REQUEST_HPP

# include <string>
# include <map>
# include <unistd.h>

#include "defines.hpp"
#include "context.hpp"
#include "IBase.hpp"

struct Request : public IBase {
    public :
		const Context*  location;
		int             upload_file_fd;
        std::string     upload_file_name;
        std::string     method;
        std::string     path;
		std::string     fullPath;
        std::string     query;
        std::string     extension;
    public :
        Request();
        ~Request();
};

# endif