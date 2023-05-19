# include <string>
# include <poll.h>
# include <iostream>
# include <vector>
# include <unistd.h>
# include <sys/socket.h>
# include <netdb.h>
# include <map>
# include <sstream>

# include "../../include/webserv.hpp"
# include "../../include/core.hpp"
# include "../../include/configuration.hpp"
# include "../../include/http.hpp"

# define BIND_REPETITION 5

# define GREEN "\033[32m"
# define RESET "\033[0m"

// * Constructor ***********************************************************************************************************************************************************************
bool Webserv::webservState = WEB_SERV_RUNNING;

Webserv::Webserv(const std::string& configFilePath) 
    : configuration(CORE::loadConfiguration(configFilePath, new Context(MAIN_CONTEXT), 0))
{
    # ifdef DEBUG
    std::cout << "-------------------------- Configuration --------------------------" << std::endl;
    CORE::display(this->configuration, 0);
    # endif
}
// *************************************************************************************************************************************************************************************

// * Static Tools **********************************************************************************************************************************************************************
static bool havSameName(int index, const std::vector<Context*>& contexts, const Context* context) {
    int p = 0;
    for (size_t i = 0; i < contexts.size(); ++i) {
        if (contexts[i]->getName() == SERVER_CONTEXT) {
            if (p == index) {
                if (context->getDirectives().find(NAME_DIRECTIVE) == context->getDirectives().end() && contexts[i]->getDirectives().find(NAME_DIRECTIVE) == contexts[i]->getDirectives().end()) {
                    return true;
                } else if (context->getDirectives().find(NAME_DIRECTIVE) != context->getDirectives().end() && contexts[i]->getDirectives().find(NAME_DIRECTIVE) != contexts[i]->getDirectives().end()) {
                    std::vector<std::string> contextName = context->getDirectives().find(NAME_DIRECTIVE)->second;
                    std::vector<std::string> serverName = contexts[i]->getDirectives().find(NAME_DIRECTIVE)->second;
                    if (contextName.size() == serverName.size()) {
                        for (size_t j = 0; j < contextName.size(); ++j) {
                            if (contextName[j] != serverName[j]) {
                                return false;
                            }
                        }
                        return true;
                    }
                }
                break;
            }
            ++p;
        }
    }
    return false;
}

static bool isInRange(const struct sockaddr* addr, const std::vector<struct addrinfo*>& data, const std::vector<Context*>& contexts, const Context* context) {
    bool res = false;

    for (size_t i = 0; i < data.size(); ++i) {
        if (addr->sa_family == data[i]->ai_addr->sa_family) {
            if (addr->sa_family == AF_INET) {
                const struct sockaddr_in* addr4 = reinterpret_cast<const struct sockaddr_in*>(addr);
                const struct sockaddr_in* serverAddr4 = reinterpret_cast<const struct sockaddr_in*>(data[i]->ai_addr);

                if ((addr4->sin_addr.s_addr == serverAddr4->sin_addr.s_addr && addr4->sin_port == serverAddr4->sin_port)) {
                    res = true;
                    if (havSameName(i, contexts, context)) {
                        throw std::runtime_error("Server names already used in another server block");
                    }
                }
            } else if (addr->sa_family == AF_INET6) {
                const struct sockaddr_in6* addr6 = reinterpret_cast<const struct sockaddr_in6*>(addr);
                const struct sockaddr_in6* serverAddr6 = reinterpret_cast<const struct sockaddr_in6*>(data[i]->ai_addr);

                if ((memcmp(&(addr6->sin6_addr), &(serverAddr6->sin6_addr), sizeof(addr6->sin6_addr)) == 0 && addr6->sin6_port == serverAddr6->sin6_port)) {
                    res = true;
                    if (havSameName(i, contexts, context)) {
                        throw std::runtime_error("Server names already used in another server block");
                    }
                }
            }
        }
    }
    return res;
}

void Webserv::errorHandler(int statusCode, Client* client) {
	if (client->getState() == TO_CLIENT && client->getResponse()) {
		Webserv::removeClient(client);
		return;
	}

	if (client->getClientToCgi() != NULL) {
		Webserv::removeClient(client->getClientToCgi());
	}

	if (client->isCgi()) {
		client = client->getCgiToClient();
		Webserv::removeClient(client->getClientToCgi());
		if (statusCode < 500) {
			statusCode = 502;
        }
	}
	client->setResponse(new Response(statusCode, KEEP_ALIVE));
	std::stringstream statusCodeString;
	statusCodeString << statusCode;

	try {
		if (client->getRequest()) {
			int fd;
			struct stat pathInfo;
			
			const Context *server = HTTP::getMatchedServer(client, configuration);
			const std::string& path = server->getDirective(statusCodeString.str()).at(0);
			
			const Context *location = HTTP::getMatchLocationContext(server->getChildren(), path);
			std::string fileName = location->getDirective(ROOT_DIRECTIVE).at(0);
			fileName += "/" + path.substr(location->getArgs().at(0).size());

			std::stringstream sizeStr;
			if (stat(fileName.c_str(), &pathInfo) != -1 && (fd = open(fileName.c_str(), O_RDONLY)) != -1) {
				client->getResponse()->download_file_fd = fd;
				sizeStr << pathInfo.st_size;
				client->getResponse()->addHeader("Content-Length", sizeStr.str());
				client->getResponse()->buffer.append(CRLF);
			} else {
				client->getResponse()->addBody(HTTP::getDefaultErrorPage(statusCode));
			}
		} else {
			client->getResponse()->addBody(HTTP::getDefaultErrorPage(statusCode));
		}
	}
	catch (const std::exception&) {
		client->getResponse()->addBody(HTTP::getDefaultErrorPage(statusCode));
	}
	if (client->getState() == FROM_CLIENT) {
		client->switchState();
	}
}

static void redirectTo(const std::pair<int, std::string>& redirect, Client* client) {
    client->setResponse(new Response(redirect.first, CLOSE_CONNECTION));
    client->getResponse()->addHeader("Location", redirect.second);
    client->getResponse()->buffer += "\r\n";
	if (client->getState() == FROM_CLIENT) {
		client->switchState();
	}
}

void Webserv::checkClientsTimeout()
{
	for (int i = clients.size() - 1; i >= 0; --i) {
		if (HTTP::getCurrentTimeOnMilliSecond() - TIMEOUT > clients[i]->getLastEvent()) {
			if (clients[i]->isCgi() || clients[i]->getClientToCgi())
				errorHandler(504, clients[i]);
			else
				errorHandler(408, clients[i]);
		}
	}
}
// *************************************************************************************************************************************************************************************

// * Methods ***************************************************************************************************************************************************************************
void Webserv::startServers() {
    const std::vector<Context*>& contexts = this->configuration->getChildren();
    std::vector<struct addrinfo*> serversAddress;

    for (std::vector<Context*>::const_iterator context = contexts.begin(); context != contexts.end(); ++context) {
        if ((*context)->getName() == HTTP_CONTEXT) {
            const std::vector<Context*>& contexts = (*context)->getChildren();
			for (std::vector<Context*>::const_iterator context = contexts.begin(); context != contexts.end(); ++context) {
                if ((*context)->getName() == SERVER_CONTEXT) {
                    struct addrinfo hints, *res = NULL;
                    int sockfd;

                    memset(&hints, 0, sizeof(hints));
                    hints.ai_family = AF_UNSPEC;
                    hints.ai_socktype = SOCK_STREAM;

                    const std::string& port = (*context)->getDirective(PORT_DIRECTIVE).at(0);
                    const std::string& host = (*context)->getDirective(HOST_DIRECTIVE).at(0);
					
                    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || res == NULL) {
                        throw std::runtime_error("getaddrinfo() : " + std::string(gai_strerror(errno)));
                    }

                    if (res->ai_family == AF_INET6) {
                        struct addrinfo* tmp = res;
                        while (tmp->ai_next != NULL && tmp->ai_next->ai_family != AF_INET) {
                            tmp = tmp->ai_next;
                        }
                        if (tmp->ai_next != NULL) {
                            struct addrinfo* tmp2 = tmp->ai_next;
                            tmp->ai_next = tmp->ai_next->ai_next;
                            tmp2->ai_next = res;
                            res = tmp2;
                        }
                    }

                    if (isInRange(res->ai_addr, serversAddress, contexts, (*context))) {
                        freeaddrinfo(res->ai_next);
						serversAddress.push_back(res);
						continue;
                    }
					serversAddress.push_back(res);
	
					if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
                        freeaddrinfo(res->ai_next);
                        throw std::runtime_error("socket() : " + std::string(strerror(errno)));
                    }

                    int yes = 1;
                    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
                        close(sockfd);
                        freeaddrinfo(res->ai_next);
                        throw std::runtime_error("setsockopt() : " + std::string(strerror(errno)));
                    }

                    size_t  repetition = BIND_REPETITION;
                    while (true) {
                        if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
                            if (errno == EADDRINUSE && repetition --> 0) {
                                std::cerr << "Address (" << host + ":" + port << ") already in use, retrying in 1 second..." << std::endl;
                                sleep(1);
                            } else {
                                close(sockfd);
                                freeaddrinfo(res->ai_next);
                                throw std::runtime_error("bind() : " + std::string(strerror(errno)));
                            }
                        } else {
                            break;
                        }
                    }

                    if (listen(sockfd, SOMAXCONN) == -1) {
                        close(sockfd);
                        freeaddrinfo(res->ai_next);
                        throw std::runtime_error("listen() : " + std::string(strerror(errno)));
                    }
					
                    this->serversSocketFd.push_back(sockfd);

                    # ifdef DEBUG
                    std::cout << "-------------------------- new Server --------------------------" << std::endl;
                    CORE::display(res);
                    # endif

                    freeaddrinfo(res->ai_next);
                }
            }
        }
    }

    for (size_t i = 0; i < serversAddress.size(); ++i) {
        serversAddress[i]->ai_next = NULL;
        freeaddrinfo(serversAddress[i]);
    }
}

void Webserv::removeClient(Client* client) {
	if (client->getClientToCgi()) {
		clients.erase(std::find(clients.begin(), clients.end(), client->getClientToCgi()));
	}
    clients.erase(std::find(clients.begin(), clients.end(), client));
    delete client;
}

void Webserv::run() {
    try {
        Webserv::startServers();
        CORE::listenToSignals();
    } catch (const std::exception& e) {
        throw std::runtime_error("initialization failed : " + std::string(e.what()));
    }

    std::cerr << GREEN;
    std::cerr << "**********************************" << std::endl;
    std::cerr << "* Webserv: webserv is running... *"<< std::endl;
    std::cerr << "**********************************" << std::endl;
    std::cerr << RESET;

    int					pollResult;
	std::vector<pollfd>	fds;

    while (Webserv::webservState == WEB_SERV_RUNNING) {
		checkClientsTimeout();

		CORE::fillFds(this->serversSocketFd, this->clients, fds);

        if ((pollResult = poll(fds.data(), fds.size(), 1000)) == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("poll() : " + std::string(strerror(errno)));
        }
        for (size_t i = 0; i < fds.size() && pollResult > 0; ++i) {
            try {
				if (fds[i].revents & POLLHUP) {
					Client* client = HTTP::getClientWithFd(fds[i].fd, this->clients);
					if (client && client->isCgi() == 0) {
						Webserv::removeClient(client);
						--pollResult;
						continue;
					}
				}
				if (fds[i].revents & POLLIN) {
					--pollResult;
					if (i < this->serversSocketFd.size()) {
						HTTP::acceptConnection(fds[i].fd, this->clients);
					} else {
						Client*	client = HTTP::getClientWithFd(fds[i].fd, this->clients);
						if (client) {
							Client*	cgi = HTTP::requestHandler(client, this->configuration);
							if (cgi) {
								if (cgi->getState() == TO_CGI) {
									this->clients.push_back(cgi);
								} else {
									Webserv::removeClient(cgi);
								}
							}
						}
					}
				} else if (fds[i].revents & POLLOUT) {
					--pollResult;
					Client *client = HTTP::getClientWithFd(fds[i].fd, this->clients);
					if (client && HTTP::responseHandler(client) == 0) {
                        Webserv::removeClient(client);
                    }
                }
            } catch (const int statusCode) {
                errorHandler(statusCode, HTTP::getClientWithFd(fds[i].fd, this->clients));
            } catch (const std::pair<long, std::string>& redirect) {
                redirectTo(redirect, HTTP::getClientWithFd(fds[i].fd, this->clients));
            } catch (const std::exception& e) {
                errorHandler(500, HTTP::getClientWithFd(fds[i].fd, this->clients));
            } catch (bool) {
				removeClient(HTTP::getClientWithFd(fds[i].fd, this->clients));
			}
        }
    }
}
// *************************************************************************************************************************************************************************************

// * Destructor ************************************************************************************************************************************************************************
Webserv::~Webserv() {
    for (size_t i = 0; i < this->serversSocketFd.size(); ++i) {
        close(this->serversSocketFd[i]);
    }
	for (size_t i = 0; i < this->clients.size(); ++i)
		delete this->clients[i] ;

    delete this->configuration;
}
// *************************************************************************************************************************************************************************************