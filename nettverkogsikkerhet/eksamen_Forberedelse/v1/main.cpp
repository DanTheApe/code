#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <string>
#include <vector>
#include <thread>

std::string serverTCP(){
    std::string msgres;
    // std::vector<std::string> msgr;
    std::string msgs = {"I got the data"};
    int s = socket(AF_INET, SOCK_STREAM,0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    addr.sin_addr.s_addr = INADDR_ANY;
    // inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    bind(s,(sockaddr*)&addr,sizeof(addr));
    listen(s,5);
    int klient = accept(s,NULL,NULL);
    char buf[1024];
    while(true) {
        ssize_t n = recv(klient, buf, sizeof(buf), 0);
        if (n <= 0) break;
        msgres.append(buf, n);
    }
    send(klient,msgs.data(),msgs.size(),0);

    close(klient);
    close(s);

    // for (std::string f : msgr) {msgres += f;}
    return msgres;
}

std::string klientTCP() {
    std::string msgs = "I am here";
    std::string msgres;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    connect(s, (sockaddr*)&addr, sizeof(addr));
    send(s, msgs.data(), msgs.size(), 0);
    shutdown(s, SHUT_WR);              // signal til server: ferdig å sende

    char buf[1024];
    while (true) {
        ssize_t n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        msgres.append(buf, n);
    }

    close(s);
    return msgres;
}

std::string serverUDP() {
    std::string msgs = "I got the data";

    int s = socket(AF_INET, SOCK_DGRAM, 0);     // SOCK_DGRAM = UDP
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8082);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(s, (sockaddr*)&addr, sizeof(addr));
    // ingen listen/accept — UDP er forbindelsesløst

    char buf[1024];
    sockaddr_in klient{};
    socklen_t klen = sizeof(klient);
    ssize_t n = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&klient, &klen);

    std::string msgres(buf, n > 0 ? n : 0);
    sendto(s, msgs.data(), msgs.size(), 0, (sockaddr*)&klient, klen);

    close(s);
    return msgres;
}

std::string klientUDP() {
    std::string msgs = "I am here";

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8082);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    sendto(s, msgs.data(), msgs.size(), 0, (sockaddr*)&addr, sizeof(addr));

    char buf[1024];
    sockaddr_in from{};
    socklen_t flen = sizeof(from);
    ssize_t n = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &flen);

    close(s);
    return std::string(buf, n > 0 ? n : 0);
}

int main() {
    std::string msg = "I am here";
    
    // TCP server
    int s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(s,(sockaddr*)&addr,sizeof(addr));
    listen(s,5);
    int klient = accept(s,NULL,NULL);
    recv(klient,msg.data(),msg.size(),0);
    send(klient,msg.data(),msg.size(),0);
    close(klient);
    close(s);

    // TCP klient
    int s2 = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr2{};
    addr2.sin_family = AF_INET;
    addr2.sin_port = htons(8081);
    inet_pton(AF_INET,"10.0.0.1",&addr.sin_addr);
    connect(s2,(sockaddr*)&addr2,sizeof(addr2));
    send(s2,msg.data(),msg.size(),0);
    recv(s2,msg.data(), msg.size(),0);
    close(s2);

}
