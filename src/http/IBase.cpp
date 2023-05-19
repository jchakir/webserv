# include "../../include/IBase.hpp"
# include "../../include/defines.hpp"

IBase::IBase() 
    : keepAlive(true),
	isChunked(false),
	state(CREATING),
	contentLength(0),
	maxBodySize(ULLONG_MAX)
{}

IBase::~IBase() {}