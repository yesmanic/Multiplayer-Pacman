#include "../interfaces/commands.hpp"

#include <arpa/inet.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>

void authType(User &user, list<User> &users, istringstream &iss);
void connectionType(User &user, list<User> &users, istringstream &iss);
void passwd(User &user, string oldPassword, string newPassword);
string getIp(User &user);

void transmit(User &user, string message) {
    if (user.protocol == "tcp")
        write(user.socket, message.data(), message.size());
    else if (user.protocol == "udp") {
        sendto(user.socket, message.data(), message.size(), 0,
               (struct sockaddr *)&user.address, sizeof(user.address));
    }
}

void handleRequest(char *buff, User &user, list<User> &users) {
    istringstream iss(buff);
    string type;

    iss >> type;

    if (type == "auth")
        authType(user, users, iss);
    else if (type == "connection") 
        connectionType(user, users, iss);
    else if (type == "security") {
        string method;
        iss >> method;
        
        if (method == "heartbeat-ok") {
            cout << "heartbeat ok was received" << endl;
            user.isAlive = true;
        }
        
        else {
            cout << "Invalid method" << endl;
        }

    } 
    
    else if (type == "in-game") {
        if (!user.isPlaying) return;

        string method;
        iss >> method;

        if (method == "gameover") {
            string message = "in-game gameover-ok";
            transmit(user, message);
        }
        
        else if (method == "endgame") {
            string message = "in-game endgame-ok";
            transmit(user, message);
            user.isPlaying = false;
        }
        
        else if (method == "points") {
            string points;
            iss >> points;

            // TODO: save points to user.txt
            ifstream usersFile("users.txt");
            string line;

            ofstream tempFile("temp.txt");
            
            while (getline(usersFile, line)) {
                istringstream iss(line);
                string userFile, passFile, pointsFile;

                iss >> userFile >> passFile >> pointsFile;

                if (userFile == user.username) {
                    tempFile << userFile << " " << passFile << " " << points << endl;
                } else {
                    tempFile << userFile << " " << passFile << " " << pointsFile << endl;
                }
            }

            usersFile.close();
            tempFile.close();

            remove("users.txt");
            rename("temp.txt", "users.txt");

            user.isPlaying = false;

            string message = "in-game points-ok";
            transmit(user, message);
        } 
        
        else {
            cout << "Invalid method" << endl;
        }

    } 
    
    else {
        cout << "Invalid type" << endl;
        string message = "Invalid type";
        transmit(user, message);
    }
}

void authType(User &user, list<User> &users, istringstream &iss) {
    string method;
    iss >> method;

    if (method == "signin") {
        iss >> user.username >> user.password;
        signin(user);

    } else if (method == "login") {
        iss >> user.username >> user.password;

        login(user, users);

    } else if (method == "quit") {
        if (user.isPlaying) {
            string message = "auth quit-nok";
            transmit(user, message);
            return;
        }

        string message = "auth quit-ok";
        transmit(user, message);

        auto it = users.begin();
        while (it != users.end()) {
            bool isIpEqual =
                it->address.sin_addr.s_addr == user.address.sin_addr.s_addr;
            bool isPortEqual = it->address.sin_port == user.address.sin_port;
            if (isIpEqual && isPortEqual) {
                it = users.erase(it);
                break;
            } else {
                it++;
            }
        }

        string level = "INFO";
        string event = "Client disconnected from server";
        string details = "IP: " + getIp(user);

        report(level, event, details);

    } else if (method == "passwd") {
        string oldPassword, newPassword;
        iss >> oldPassword >> newPassword;
        passwd(user, oldPassword, newPassword);
    } else {
        cout << "Invalid method" << endl;
    }
}

void connectionType(User &user, list<User> &users, istringstream &iss) {
    if (!user.isLogged) return;

    string method;
    iss >> method;

    if (method == "start") {
        if (user.isPlaying) {
            string message = "connection start-nok";
            transmit(user, message);
            return;
        }

        string message = "connection start-ok";
        user.isPlaying = true;
        user.isHost = true;

        transmit(user, message);

    } 
    
    else if (method == "gamestart") {
        string port;
        iss >> port;

        user.gamePort = stoi(port);
        string message = "connection gamestart-ok";
        transmit(user, message);

        string level = "INFO";
        string event = "User " + user.username + " started a game";
        string details = "IP: " + getIp(user);

        report(level, event, details);

    } 
    
    else if (method == "challenge") {
        string opponent;
        iss >> opponent;

        bool found = false;

        list<User>::iterator it;

        for (it = users.begin(); it != users.end(); it++) {
            if (it->username == opponent && it->isLogged && it->isPlaying &&
                it->isHost) {
                found = true;
                break;
            }
        }

        if (!found) {
            string message = "connection challenge-nok";
            transmit(user, message);
            return;
        }

        string ip = inet_ntoa(it->address.sin_addr);
        string port = to_string(it->gamePort);

        string message = "connection challenge-ok " + ip + " " + port;
        user.isPlaying = true;

        transmit(user, message);

        string level = "INFO";
        string event = "User " + user.username + " challenged " + opponent;
        string details = "IP: " + getIp(user);

        report(level, event, details);

    } 
    
    else if (method == "logout") {
        if (!user.isLogged || user.isPlaying) {
            string message = "connection logout-nok";
            transmit(user, message);
            return;
        }

        user.isLogged = false;
        user.isPlaying = false;
        user.isHost = false;

        string message = "connection logout-ok";
        transmit(user, message);
    
    } 
    
    else if (method == "list") {
        if (!user.isLogged) {
            string message = "connection list-nok";
            transmit(user, message);
            return;
        }

        string message = "connection list-ok ";
        string usersList = "";

        int loggedUsers = 0;
        for (User u : users) {
            if (u.isLogged) {
                loggedUsers++;
                usersList += " " + u.username;
                if (u.isPlaying) usersList += "(playing)";
            }
        }
        message += to_string(loggedUsers) + usersList;
        transmit(user, message);

    } 
    
    else if (method == "leaderboard") {
        
        ifstream usersFile("users.txt");
        string line;

        string message = "connection leaderboard-ok";
        
        priority_queue<pair<int, string>> pq;
        while (getline(usersFile, line)) {
            istringstream iss(line);
            string userFile, passFile, pointsFile;

            iss >> userFile >> passFile >> pointsFile;

            pq.push({stoi(pointsFile), userFile});
        }

        message += " " + to_string(pq.size());

        string usersList = "";
        while (!pq.empty()) {
            auto [points, username] = pq.top();
            usersList += " " + username + " " + to_string(points);
            pq.pop();
        }

        message += usersList;
        transmit(user, message);
    }
    
    else {
        cout << "Invalid method" << endl;
    }
}

void passwd(User &user, string oldPassword, string newPassword) {
    ifstream usersFile("users.txt");
    string line;

    bool found = false;
    while (getline(usersFile, line)) {
        istringstream iss(line);
        string userFile, passFile;

        iss >> userFile >> passFile;

        if (userFile == user.username && passFile == oldPassword) {
            cout << "User found" << endl;
            found = true;
            break;
        }
    }

    if (!found) {
        cout << "User not found" << endl;
        string message = "auth passwd-nok";
        transmit(user, message);
        return;
    }

    ifstream usersFile2("users.txt");
    ofstream tempFile("temp.txt");

    while (getline(usersFile2, line)) {
        istringstream iss(line);
        string userFile, passFile;

        iss >> userFile >> passFile;

        if (userFile == user.username) {
            tempFile << userFile << " " << newPassword << endl;
        } else {
            tempFile << userFile << " " << passFile << endl;
        }
    }

    usersFile.close();
    usersFile2.close();
    tempFile.close();

    remove("users.txt");
    rename("temp.txt", "users.txt");

    string message = "auth passwd-ok";
    transmit(user, message);
}

void signin(User &user) {
    ifstream usersFile("users.txt");
    string line;

    bool found = false;
    while (getline(usersFile, line)) {
        istringstream iss(line);
        string userFile, passFile;

        iss >> userFile >> passFile;

        if (userFile == user.username) {
            cout << "User already exists" << endl;
            found = true;
            break;
        }
    }

    if (found) {
        string message = "auth signin-nok";
        transmit(user, message);
        return;
    }

    ofstream serverFile;
    serverFile.open("users.txt", ofstream::binary | ofstream::app);
    serverFile << user.username << " " << user.password << " 0" << endl;
    serverFile.close();

    string message = "auth signin-ok";
    transmit(user, message);

    string level = "INFO";
    string event = "User " + user.username + " sign in";
    string details = "IP: " + getIp(user);

    report(level, event, details);
}

void login(User &user, list<User> &users) {
    ifstream usersFile("users.txt");
    string line;

    bool found = false;
    while (getline(usersFile, line)) {
        istringstream iss(line);
        string userFile, passFile;

        iss >> userFile >> passFile;

        if (userFile == user.username && passFile == user.password) {
            cout << "User found" << endl;
            found = true;
            break;
        }
    }

    if (!found) {
        cout << "User not found" << endl;
        string message = "auth login-nok";
        transmit(user, message);
        
        string level = "ERROR";
        string event = "User " + user.username + " failed to log in";
        string details = "IP: " + getIp(user);

        report(level, event, details);
    } else {
        bool userAlreadyLogged = false;

        for (User u : users) {
            if (u.username == user.username && u.isLogged) {
                cout << "User already logged" << endl;
                userAlreadyLogged = true;
                break;
            }
        }

        if (userAlreadyLogged) {
            string message = "auth login-nok";
            transmit(user, message);
            return;
        }

        user.isLogged = true;
        string message = "auth login-ok";

        transmit(user, message);

        string level = "INFO";
        string event = "User " + user.username + " logged in";
        string details = "IP: " + getIp(user);

        report(level, event, details);
    }
}

string getIp(User &user) {
    string address = string(inet_ntoa(user.address.sin_addr)) + ":" + to_string(ntohs(user.address.sin_port));
    return address;
}

// Log events in a file
void report(string level, string event, string details) {
    ofstream logFile;
    logFile.open("log.txt", ofstream::binary | ofstream::app);
    
    // Catch current time
    time_t now = time(0);
    tm *ltm = localtime(&now);

    // Format time dd-mm-yyyy hh:mm:ss
    string day = to_string(ltm->tm_mday).size() == 1 ? "0" + to_string(ltm->tm_mday) : to_string(ltm->tm_mday);
    string month = to_string(1 + ltm->tm_mon).size() == 1 ? "0" + to_string(1 + ltm->tm_mon) : to_string(1 + ltm->tm_mon);
    string year = to_string(1900 + ltm->tm_year);
    string hour = to_string(ltm->tm_hour).size() == 1 ? "0" + to_string(ltm->tm_hour) : to_string(ltm->tm_hour);
    string min = to_string(ltm->tm_min).size() == 1 ? "0" + to_string(ltm->tm_min) : to_string(ltm->tm_min);
    string sec = to_string(ltm->tm_sec).size() == 1 ? "0" + to_string(ltm->tm_sec) : to_string(ltm->tm_sec);

    string time = day + "-" + month + "-" + year + " " + hour + ":" + min + ":" + sec;

    // Write in file
    logFile << "[" << time << "] [" << level << "] " << event << " - " << details << endl;
    logFile.close();
}