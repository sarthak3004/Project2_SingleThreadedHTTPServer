#include<iostream>
#include<sys/socket.h>
#include<unistd.h> //UNIX STD (For POSIX System Calls)
#include<arpa/inet.h> //sockaddr
#include<sstream>

bool send_all(int socket_fd, std::string& data) {
    size_t totalSent = 0;
    const char* ptr = data.c_str();
    size_t totalBytes = data.size();
    while(totalSent < totalBytes) {
        ssize_t sent = write(socket_fd, ptr + totalSent, totalBytes - totalSent);
            if(sent < 0) {
                if(errno == EINTR) continue;
                return false;
            }
            if(sent == 0) return false;
            totalSent += static_cast<size_t>(sent);
    }
    return true;
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

        
        std::string response;
        if(method != "GET") {
            std::string body = "<h1>501 Not Implemented</h1><p>Only GET is supported.</p>";
            response = 
                "HTTP/1.1 501 Not Implemented\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + 
                body;
        } else {
            std::string body = 
                "<!DOCTYPE html><html><body>"
                "<h1>Hello from C++ HTTP Server!</h1>"
                "<p>Successfully parsed request line:</p>"
                "<ul>"
                "<li><b>Method:</b> " + method + "</li>"
                "<li><b>Path:</b> " + path + "</li>"
                "<li><b>Version:</b> " + version + "</li>"
                "</ul>"
                "</body></html>";

            response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + 
                body;
        }
        send_all(client_fd, response);
        close(client_fd);
        std::cout << "Client Discconnected.\n";
    }
    close(server_fd); //Will never be called. '_'
    return 0;
}

