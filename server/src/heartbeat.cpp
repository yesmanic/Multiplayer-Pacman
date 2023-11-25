#include "../interfaces/heartbeat.hpp"

string receiveHeartbeat(User &user);
void transmitHeartbeat(User &user, string message);

void heartbeat(list<User> &users) {
    while (true) {
        auto it = users.begin();
        while (it != users.end()) {
            if (!it->isConnected) {
                it++;
                continue;
            }

            string message = "connection heartbeat";
            transmitHeartbeat(*it, message);
            cout << "ggboy" << endl;

            if (it->protocol == "tcp") {
                string response = receiveHeartbeat(*it);
                if (response == "connection heartbeat-ok") {
                    cout << "Client is alive" << endl;
                    it++;
                } else {
                    cout << "Client is dead" << endl;
                    it = users.erase(it);
                }
            } else {
                it++;
            }
        }
        this_thread::sleep_for(chrono::seconds(5));
    }
}

void tcphb(int tcpfd, list<User> &users) {
    struct sockaddr_in clientSocket;
    bzero(&clientSocket, sizeof(clientSocket));
    socklen_t clientSocketLen = sizeof(clientSocket);

    for (;;) {
        int clientfd = accept(tcpfd, (struct sockaddr *) &clientSocket, &clientSocketLen);
        cout << "TCP HeartBeat Connection Open" << endl;

        vector<char> buff(1000, 0);
        int n = read(clientfd, buff.data(), buff.size());

        istringstream iss(buff.data());
        string type, method, ip, port;
        iss >> type >> method >> ip >> port;

        struct in_addr addr;
        inet_pton(AF_INET, ip.data(), &addr);

        auto it = users.begin();
        for (;it != users.end(); it++) {
            bool isIpEqual = it->address.sin_addr.s_addr == addr.s_addr;
            bool isPortEqual = it->address.sin_port == htons(stoi(port));
            if (isIpEqual && isPortEqual) {
                it->heartbeat.address = clientSocket;
                it->heartbeat.clientfd = clientfd;
                it->isConnected = true;
                break;
            }
        }

        if (it != users.end()) {
            cout << "I've found!!" << endl;
            string message = "security heartbeat-ok";
            transmitHeartbeat(*it, message);
        } else {
            cout << "I've not found!!" << endl;
            string message = "security heartbeat-nok";
            transmitHeartbeat(*it, message);
        }
    }
}

void udphb(int udpfd, list<User> &users) {
    struct sockaddr_in clientSocket;
    bzero(&clientSocket, sizeof(clientSocket));
    socklen_t clientSocketLen = sizeof(clientSocket);

    vector<char> buff(1000, 0);

    while (true) {
        int n = recvfrom(udpfd, buff.data(), 1000, 0, (struct sockaddr *) &clientSocket, &clientSocketLen);
        cout << "UDP HeartBeat Connection Open" << endl;

        string response = string(buff.data());
        cout << response << endl;

        istringstream iss(buff.data());
        string type, method;
        iss >> type >> method;

        if (type == "security" && method == "heartbeat") {
            string ip, port;
            iss >> ip >> port;

            struct in_addr addr;
            inet_pton(AF_INET, ip.data(), &addr);

            auto it = users.begin();
            for (;it != users.end(); it++) {
                bool isIpEqual = it->address.sin_addr.s_addr == addr.s_addr;
                bool isPortEqual = it->address.sin_port == htons(stoi(port));
                if (isIpEqual && isPortEqual) {
                    it->heartbeat.address = clientSocket;
                    it->heartbeat.clientfd = udpfd;
                    it->isConnected = true;
                    break;
                }
            }

            if (it != users.end()) {
                cout << "I've found!!" << endl;
                string message = "security heartbeat-ok";
                transmitHeartbeat(*it, message);
            } else {
                cout << "I've not found!!" << endl;
                string message = "security heartbeat-nok";
                transmitHeartbeat(*it, message);
            }
        } else if (type == "connection" && method == "heartbeat-ok") {
            cout << "Client is alive" << endl;
        }

        memset(buff.data(), 0, 1000);
    }
}

string receiveHeartbeat(User &user) {
    vector<char> buff(1000, 0);
    int n = recvfrom(user.heartbeat.clientfd, buff.data(), buff.size(), 0, NULL, NULL);
    return string(buff.data());
}

void transmitHeartbeat(User &user, string message) {
    if (user.protocol == "tcp")
        send(user.heartbeat.clientfd, message.data(), message.size(), 0);
    else if (user.protocol == "udp")
        sendto(user.heartbeat.clientfd, message.data(), message.size(), 0, (struct sockaddr *)&user.heartbeat.address, sizeof(user.heartbeat.address));
}