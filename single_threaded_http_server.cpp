#include<iostream>
#include<sys/socket.h>
#include<unistd.h> //UNIX STD (For POSIX System Calls)
#include<arpa/inet.h> //sockaddr
#include<sstream>
#include<fstream>

bool send_all(int socket_fd, const char* data, size_t totalBytes) {
    size_t totalSent = 0;
    while(totalSent < totalBytes) {
        ssize_t sent = write(socket_fd, data + totalSent, totalBytes - totalSent);
            if(sent < 0) {
                if(errno == EINTR) continue;
                return false;
            }
            if(sent == 0) return false;
            totalSent += static_cast<size_t>(sent);
    }
    return true;
}

std::string getMimeType(const std::string& path) {
    if(path.rfind(".html") != std::string::npos || path.rfind(".htm") != std::string::npos) return "text/html";
    if(path.rfind(".css") != std::string::npos) return "text/css";
    if(path.rfind(".js") != std::string::npos) return "application/javascript";
    if(path.rfind(".png") != std::string::npos) return "image/png";
    if(path.rfind(".jpg") != std::string::npos || path.rfind(".jpeg") != std::string::npos) return "image/jpeg";
    if(path.rfind(".gif") != std::string::npos) return "image/gif";
    if(path.rfind(".ico") != std::string::npos) return "image/x-icon";
    return "text/plain";
}

void sendErrorResponse(int client_fd, int statusCode, const std::string& statusText, const std::string& message) {
    std::string body = "<html><body><h1>" + std::to_string(statusCode) + " " + statusText + "</h1><p>" + message + "</p></body></html>";
    std::string header = 
        "HTTP/1.1" + std::to_string(statusCode) + " " + statusText + "\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n";
    
    send_all(client_fd, header.data(), header.size());
    send_all(client_fd, body.data(), body.size());
}

int main() {
    int server_fd = socket( //fd(file descriptor) because everythin is file in UNIX/LINUX. Network socket is file. Therefore, operators like read, write, open, close.
        AF_INET, //IPv4 
        SOCK_STREAM, //TCP type transfer
        0); //0 for default protocol to be used. TCP for SOCK_STREAM.
    if(server_fd < 0) {
        perror("Socket not created.");
        return 1;
    }
    int enable = 1;
    if(setsockopt(server_fd, SOL_SOCKET,SO_REUSEADDR, &enable, sizeof(enable)) < 0) { //Incase program is closed and kernel is still waiting for final FIN or something and IP address is still in use.
        perror("Address already in use.");
        close(server_fd);
        return 1;
    }
    /**
     * This code configures an IPv4 network endpoint 
     * (IP address and port) so the kernel knows which 
     * network interface and port your server should attach to.
     */
    sockaddr_in address{}; //Internet socket address
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY tells the kernel to accept incoming connections directed at any of those addresses. (eg. loopback, wifi, vpn ip)
    address.sin_port = htons(8080); //htons = host to network short. Convert however host store 8080 to network standard. convert to big endian.


    /**Casting from derived class to base class. */
    if(bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        perror("Binding Error.");
        close(server_fd);
        return 1;
    }

    if(listen(server_fd, 5) < 0) { //5 is the backlog value ie max length of queue. After the queue is full, any client that tries to connect gets connection refused error.
        perror("Listening Error.");
        close(server_fd);
        return 1;
    }

    std::cout << "Listening on port 8080.\n";

    while(true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if(client_fd < 0) {
            perror("Accept Error.");
            continue;
        }

        std::cout << "Client Connected.\n";
        char buffer[2048];
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer)-1);
        if(bytes_read <= 0) {
            close(client_fd);
            continue;
        }
        buffer[bytes_read] = '\0';
        std::istringstream request_stream(buffer);
        std::string method, path, version;
        request_stream >> method >> path >> version;
        std::cout << "\n[Request] Method: " << method
                  << "| Path: " << path
                  << "| Version: " << version << "\n";
        if(method != "GET") {
            sendErrorResponse(client_fd, 501, "Not Implemented", "Only GET Method");
            close(client_fd);
            continue;
        }
        if(path.find("..") != std::string::npos) {
            sendErrorResponse(client_fd, 403, "Forbidden", "Access Denied");
            close(client_fd);
            continue;
        }

        if(path == "/") path = "/index.html";
        std::string filePath = "./public" + path;

        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if(!file.is_open()) {
            sendErrorResponse(client_fd, 404, "Not Found", "The requested file was not found.");
            close(client_fd);
            continue;
        }
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> fileData(static_cast<size_t>(fileSize));
        if(!file.read(fileData.data(), fileSize)) {
            sendErrorResponse(client_fd, 500, "Internal Server Error", "Failed to read file.");
            close(client_fd);
            continue;
        }

        std::string mimeType = getMimeType(path);
        std::string headers = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: " + mimeType + "\r\n"
            "Content-Length: " + std::to_string(fileSize) + "\r\n"
            "Connection: close\r\n\r\n";
        send_all(client_fd, headers.data(), headers.size());
        send_all(client_fd, fileData.data(), fileSize);

        close(client_fd);
        std::cout << "Client Discconnected.\n";
    }
    close(server_fd); //Will never be called. '_'
    return 0;
}

