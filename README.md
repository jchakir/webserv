# Webserv

  A high-performance, scalable, and feature-rich HTTP server in C++ 98.
  
  This project is done with teammate [Mustapha-Annouaoui](https://github.com/Mustapha-Annouaoui) a great programmer.
  
  Please read en.subject.pdf for more information about guidelines.

# Features:

    Non-blocking I/O for high performance and scalability
    Support for multiple HTTP methods, including GET, POST, and DELETE
    Accurate HTTP status codes and error pages
    Ability to serve static and dynamic content
    Support for cookies and session management
    GET, POST and DELETE methods Support
    Multiple CGI Support (php, python, ...)
    Multiple servers block Support
    Multiple locations block Support
    Multiple index files Support
    Multiple error pages Support
    Autoindex Support
    URL encoding Support
    Client body size limit Support
    timeout Support
    Redirect Support
    include files in config file Support
    Log files Support
    Chunked Transfer Encoding Support
    Keep-Alive Support
    config file comments Support
    mime types include in config file Support
    and more...

# Usage:

### To use the server, simply clone the repository:
```
git clone https://github.com/your-username/http-server.git
cd http-server
```
### To generate config file template, the output file located in config folder
```
make generate
```
### To run in debug mode:
```
make run
```
### Run in production mode:
```
make release
make run
```

# Configuration:

### The configuration file can contain the following keys:

    port                   : The port that the server should listen on.
    server_name            : A list of server names that the server should respond to.
    error_page             : A map of HTTP status codes to error pages.
    client_body_size_limit : The maximum size of a client request body in bytes.
    location               : A list of routes that the server should serve.

### Each route(location) can contain the following keys:

    methods           : A list of HTTP methods that the route should support.
    redirect          : A redirection URL, if any.
    root              : The directory or file from which the requested file should be served.
    directory_listing : Whether or not directory listing is enabled for the route.
    default_file      : The default file to serve if the requested file is a directory.
    cgi_script        : The CGI script to execute if the requested file has the specified extension.
    upload_dir        : The directory where uploaded files should be saved.
    cgi_path          : A list of CGI paths that should be supported for the route.

# Additional notes:

  The server is designed to be scalable and efficient. It uses non-blocking I/O and only 1 poll() (or equivalent) for all the I/O operations between the client and the server.
  The server is compatible with the web browser of the user's choice. It has been tested with Chrome, Firefox, Edge, and Safari.
  The server can be used to serve a fully static website, allow clients to upload files, and execute CGI scripts. It is also capable of handling cookies and session management.

# Conclusion

Webserv is a high-performance, scalable, and feature-rich HTTP server in C++ 98. It is ideal for a wide range of applications, from simple     static websites to complex dynamic web applications.
