# include "../../include/client.hpp"
# include "../../include/http.hpp"

static void __read_content_length_body_(Client* client, IBase *base)
{
	size_t size = std::min(base->contentLength, client->getBuffer().size());

	base->body.append(client->getBuffer(), 0, size);
	client->getBuffer().erase(0, size);
	base->contentLength -= size;

	if (base->contentLength == 0) {
		base->state = READY;
	}
}

static bool __read_chunked_body_(Client* client, IBase *base) {
	std::string&	Buffer = client->getBuffer();
	const char		*buffer = Buffer.c_str();

	if (base->contentLength == 0) {
		while (std::isdigit(*buffer)) {
			++buffer;
		}
		if (*buffer == '\0') {
			return false;
		}
		if (*buffer != '\r' || *(buffer + 1) != '\n')
			throw 400;
		Buffer[buffer - Buffer.c_str()] = '\0';
		buffer += 2; // for bypass '\r\n'
		base->contentLength = HTTP::parseContentLength(Buffer.c_str());
		if (base->contentLength == 0 && *buffer == '\r' && *(buffer + 1) == '\n') {
			base->state = READY;
			buffer += 2; // for bypass '\r\n'
			Buffer.erase(0,buffer - Buffer.c_str());
			return false;
		}

		// this for max body size checker
		if (base->maxBodySize < base->contentLength)
			throw 413;
		base->maxBodySize -= base->contentLength;

		base->contentLength += 2; // this is for '\r\n'
		Buffer.erase(0,buffer - Buffer.c_str());
	} else {
		base->body.append(Buffer, 0, base->contentLength - 2); // -2 for '\r\n'
		if (Buffer.size() > base->contentLength) {
			Buffer.erase(0, base->contentLength);
			base->contentLength = 0;
		} else {
			base->contentLength -= Buffer.size();
			Buffer.clear();
		}
	}
	return Buffer.size() != 0;
}

void HTTP::readBodyFromBuffer(Client *client) {
	IBase *base;
	
	if (client->isCgi())
		base = reinterpret_cast<IBase*>(client->getResponse());
	else
		base = reinterpret_cast<IBase*>(client->getRequest());
	
	if (base->isChunked) {
		while (__read_chunked_body_(client, base));
	} else {
		__read_content_length_body_(client, base);
	}
}
