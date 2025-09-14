// g++ lobbyServer.cpp -o s

#include <iostream>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <thread>
#include <fstream>
#include <sstream>

using namespace std;

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

const string playerDataFile = "player_data.txt";
const string gameInfoFile = "game_info.txt";

struct Player
{
    string username;
    string password;
    bool online = false;
    string status = "idle"; // idle or in room
};

struct GameRoom
{
    string creator;
    string gameType;
    string roomType; // public or private
    string roomName;
    string portNumber;
    string roomStatus = "waiting"; // waiting or in game
    unordered_set<string> players;
};

struct GameInfo
{
    string name;
    string author;
    string description;
    string filepath; // in server_game_folder
};

unordered_map<int, string> clients;           // Map from socket to username
unordered_map<string, Player> playerDatabase; // Map from username to Player data
vector<GameRoom> gameRooms;                   // List of rooms
unordered_map<string, GameInfo> gameDatabase;
string inviteMessage;

void registerPlayer(int clientSock, const string &username, const string &password)
{
    if (playerDatabase.find(username) == playerDatabase.end())
    {
        playerDatabase[username] = {username, password, true};
        clients[clientSock] = username;

        string message = "RESPONSE: Registration successful.";
        send(clientSock, message.c_str(), message.size(), 0);
    }
    else
    {
        string message = "RESPONSE: Username already exists.";
        send(clientSock, message.c_str(), message.size(), 0);
    }
}

void loginPlayer(int clientSock, const string &username, const string &password)
{
    if (playerDatabase.find(username) != playerDatabase.end() && playerDatabase[username].password == password)
    {
        playerDatabase[username].online = true;
        clients[clientSock] = username;

        string message = "RESPONSE: Login success.";
        send(clientSock, message.c_str(), message.size(), 0);

        // 廣播給其他所有player
        string broadcastMsg = "BROADCAST: " + username + " has logged in.";
        for (const auto &client : clients)
        {
            if (client.first != clientSock) // Skip the user who just logged in
            {
                send(client.first, broadcastMsg.c_str(), broadcastMsg.size(), 0);
            }
        }

        /* 改成不是這裡print
        // print online players
        message += "Online players:\n";
        bool anyOnline = false;
        for (const auto &entry : playerDatabase)
        {
            if (entry.second.online)
            {
                message += entry.second.username + " (" + entry.second.status + ")\n";
                anyOnline = true;
            }
        }
        if (!anyOnline)
        {
            message += "Currently, no players are online.";
        }

        // print game rooms
        message += "Game rooms:\n";
        bool anyPublicRoom = false;
        for (const auto &room : gameRooms)
        {
            if (room.roomType == "public" && room.roomStatus == "waiting")
            {
                message += "Creator: " + room.creator + ", Room Name: " + room.roomName + ", Game Type: " + room.gameType + ", Room Status: " + room.roomStatus + "\n";
                anyPublicRoom = true;
            }
        }
        if (!anyPublicRoom)
        {
            message += "No public rooms waiting for players.\n";
        }*/
    }
    else
    {
        string message = "RESPONSE: Login failed. Invalid username or password.";
        send(clientSock, message.c_str(), message.size(), 0);
    }
}

void logoutPlayer(int clientSock)
{
    if (clients.find(clientSock) != clients.end())
    {
        string username = clients[clientSock];
        playerDatabase[username].online = false;
        clients.erase(clientSock);

        string message = "RESPONSE: Logout successful.";
        send(clientSock, message.c_str(), message.size(), 0);

        // 廣播給其他所有player
        string broadcastMsg = "BROADCAST: " + username + " has logged out.";
        for (const auto &client : clients)
        {
            if (client.first != clientSock) // Skip the user who just logged in
            {
                send(client.first, broadcastMsg.c_str(), broadcastMsg.size(), 0);
            }
        }
    }
}

void createGameRoom(int clientSock, const string &roomName, const string &gameType, const string &roomType, const string &portNumber)
{
    string creator = clients[clientSock];
    GameRoom newRoom = {creator, gameType, roomType, roomName, portNumber, "waiting"};

    gameRooms.push_back(newRoom);

    if (roomType == "public")
    {
        string message = "RESPONSE: Room created successfully. Waiting for other players to join.\n";
        send(clientSock, message.c_str(), message.size(), 0);

        // 廣播給其他所有player
        string broadcastMsg = "BROADCAST: A new public game room named \"" + roomName + "\" has been created.";
        for (const auto &client : clients)
        {
            if (client.first != clientSock) // Skip the user who just logged in
            {
                send(client.first, broadcastMsg.c_str(), broadcastMsg.size(), 0);
            }
        }
    }
    else if (roomType == "private")
    {
        string message = "RESPONSE: Room created successfully.\n Idle players: \n";
        for (const auto &entry : playerDatabase)
        {
            if (entry.second.online && entry.second.status == "idle" && entry.second.username != creator)
            {
                message += "Username: " + entry.second.username + "\n";
            }
        }
        message += "Would you like to invite an idle player? Please enter the username: ";
        send(clientSock, message.c_str(), message.size(), 0);
    }
}

void listRoom(int clientSock)
{
    string publicRooms = "RESPONSE: Public game rooms:\n";
    bool anyPublicRoom = false;

    for (const auto &room : gameRooms)
    {
        if (room.roomType == "public" && room.roomStatus == "waiting")
        {
            publicRooms += "Creator: " + room.creator + ", Room Name: " + room.roomName + ", Game Type: " + room.gameType + ", Room Status: " + room.roomStatus + "\n";
            anyPublicRoom = true;
        }
    }
    if (!anyPublicRoom)
    {
        publicRooms += "No public rooms waiting for players.\n";
    }
    /* publicRooms += inviteMessage; // 如果有邀請訊息 */
    send(clientSock, publicRooms.c_str(), publicRooms.size(), 0);
}

void listPlayer(int clientSock)
{
    string msg = "RESPONSE: Online players:\n";
    bool anyOnlinePlayers = false;
    for (const auto &entry : playerDatabase) // Iterates over playerDatabase
    {
        const Player &player = entry.second; // Access the Player struct
        if (player.online)                   // Check if the player is online
        {
            msg += "Username: " + player.username + ", Status: " + player.status + "\n";
            anyOnlinePlayers = true; // Mark that we found an online player
        }
    }
    if (!anyOnlinePlayers)
    {
        msg += "No online players.\n";
    }
    send(clientSock, msg.c_str(), msg.size(), 0);
}

void listGame(int clientSock)
{
    // 建遊戲列表
    string msg = "RESPONSE: All available games:\n";
    bool anyGamesFound = false;

    for (const auto &entry : gameDatabase)
    {
        const GameInfo &game = entry.second;
        msg += "Name: " + game.name + ", ";
        msg += "Developer: " + game.author + ", ";
        msg += "Description: " + game.description + "\n";
        anyGamesFound = true;
    }

    if (!anyGamesFound)
    {
        msg += "No games.\n";
    }

    // 將消息發給client
    send(clientSock, msg.c_str(), msg.size(), 0);
}

void listYourGame(int clientSock)
{
    // 確認client的用戶名
    if (clients.find(clientSock) == clients.end())
    {
        string errorMsg = "ERROR: User not found.\n";
        send(clientSock, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    string username = clients[clientSock];

    // 建遊戲表
    string msg = "RESPONSE: Games authored by you:\n";
    bool anyGamesFound = false;

    for (const auto &entry : gameDatabase)
    {
        const GameInfo &game = entry.second;
        if (game.author == username) // 僅匹配作者是當前用戶的遊戲
        {
            msg += "Name: " + game.name + ", ";
            msg += "Developer: " + game.author + ", ";
            msg += "Description: " + game.description + "\n";
            anyGamesFound = true;
        }
    }

    if (!anyGamesFound)
    {
        msg += "No games authored by you.\n";
    }

    // 將消息發給client
    send(clientSock, msg.c_str(), msg.size(), 0);
}

void sendGameFile(int clientSock, const string &gameType)
{
    string filePath = "server_game_folder/" + gameType; // 假設伺服器的遊戲文件存放在該目錄

    // 打開遊戲文件
    ifstream inFile(filePath, ios::binary);
    if (!inFile.is_open())
    {
        cerr << "Error: Could not open file " << filePath << " for reading.\n";
        string errorMsg = "ERROR: File not found\n";
        send(clientSock, errorMsg.c_str(), errorMsg.length(), 0);
        return;
    }

    // 獲取文件大小
    inFile.seekg(0, ios::end);
    size_t fileSize = inFile.tellg();
    inFile.seekg(0, ios::beg);

    // 發送文件大小
    string fileSizeMessage = "RESPONSE: FILE_SIZE " + to_string(fileSize) + "\n";
    send(clientSock, fileSizeMessage.c_str(), fileSizeMessage.length(), 0);

    // 等待客戶端確認準備接收文件
    char buffer[1024];
    ssize_t bytesReceived = recv(clientSock, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0 || string(buffer, bytesReceived) != "READY_TO_RECEIVE\n")
    {
        cerr << "Error: Did not receive confirmation from client to send file.\n";
        return;
    }

    size_t totalBytesSent = 0;                    // 累計發送的字節數
    const string prefix = "RESPONSE: ";           // 定義前綴
    const size_t prefixSize = prefix.size();      // 前綴長度
    const size_t maxDataSize = 1024 - prefixSize; // 文件數據最大長度
    char sendBuffer[1024];                        // 總發送緩衝區大小固定為 1024 字節

    while (!inFile.eof())
    {
        // 讀取數據，最大讀取長度為 1024 減去前綴長度
        inFile.read(buffer, maxDataSize);
        streamsize bytesRead = inFile.gcount();

        if (bytesRead > 0)
        {
            size_t startByte = totalBytesSent;
            size_t endByte = totalBytesSent + bytesRead - 1;

            // 將 "RESPONSE: " 複製到發送緩衝區
            memcpy(sendBuffer, prefix.c_str(), prefixSize);

            // 將文件內容追加到發送緩衝區
            memcpy(sendBuffer + prefixSize, buffer, bytesRead);

            // 發送數據
            ssize_t bytesSent = send(clientSock, sendBuffer, prefixSize + bytesRead, 0);
            if (bytesSent < 0)
            {
                cerr << "Error: Failed to send data chunk from bytes " << startByte << " to " << endByte << ".\n";
                break;
            }

            // 更新累計發送的字節數
            totalBytesSent += bytesRead;

            cout << "Sent bytes " << startByte << " to " << endByte << " (including RESPONSE prefix).\n";
        }
    }

    // 發送 "END_OF_FILE" 標記傳輸結束
    string endOfFileMessage = "RESPONSE: END_OF_FILE";
    send(clientSock, endOfFileMessage.c_str(), endOfFileMessage.length(), 0);

    cout << "Game file " << gameType << ".cpp sent to client.\n";

    inFile.close();
}

void joinGameRoom(int clientSock, const string &roomName)
{
    bool roomFound = false;

    for (auto &room : gameRooms)
    {
        cout << room.roomName << " " << room.roomStatus << endl;
        cout << "roomname: " << roomName << endl;
        if (room.roomName == roomName && room.roomStatus == "waiting")
        {
            roomFound = true;

            room.players.insert(clients[clientSock]);
            playerDatabase[clients[clientSock]].status = "in game"; // 更新玩家狀態

            // request + recv
            int creatorSock = -1;
            for (const auto &client : clients)
            {
                if (client.second == room.creator)
                {
                    creatorSock = client.first;
                    break;
                }
            }
            if (creatorSock == -1)
            {
                string message = "RESPONSE: Error: Creator socket not found.";
                send(clientSock, message.c_str(), message.size(), 0);
                return; // Exit if creator socket is not found
            }
            string message = "RESPONSE: Someone joined room.\n";
            send(creatorSock, message.c_str(), message.size(), 0);

            string response;
            char buffer[256]; // Adjust size as needed
            int bytesReceived = recv(creatorSock, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived > 0)
            {
                buffer[bytesReceived] = '\0'; // Null-terminate the received message
                response = string(buffer);    // 有creator的ip and port
            }

            cout << response << endl; // ##
            response = "RESPONSE: " + response;

            // 傳送creator的ip and port給新玩家
            send(clientSock, response.c_str(), response.size(), 0);
            cout << response << endl; // ##

            // Wait for the response from the creator
            if (bytesReceived > 0)
            {
                buffer[bytesReceived] = '\0'; // Null-terminate the received message
                string response(buffer);

                if (response.substr(0, 7) == "NOTIFY ")
                {
                    string receivedRoomName = response.substr(7); // Extract the room name
                    if (receivedRoomName == roomName)
                    {
                        for (auto &it : gameRooms)
                        {
                            if (it.roomName == receivedRoomName)
                            {
                                it.roomStatus = "in game"; // Update room status
                            }
                        }
                    }
                }
            }
            break;
        }
    }

    if (!roomFound)
    {
        send(clientSock, "RESPONSE: 無法加入房間，請檢查房間信息。\n", 57, 0); // 若找不到房間，傳送錯誤訊息
    }
}

void invitePlayer(int p1Sock, const string &invitee)
{
    string inviter = clients[p1Sock];
    string roomname;

    // 檢查被邀請的玩家是否在線
    if (playerDatabase.find(invitee) != playerDatabase.end() && playerDatabase[invitee].online)
    {
        int p2Sock = -1;
        for (const auto &client : clients)
        {
            if (client.second == invitee)
            {
                p2Sock = client.first; // 找到被邀請者的 socket
                break;
            }
        }

        if (p2Sock != -1)
        {
            // 發送邀請訊息給被邀請者
            for (const auto &room : gameRooms)
            {
                if (room.creator == inviter)
                {
                    // 使用找到的房間名稱來建立邀請訊息
                    inviteMessage = "INVITATION: " + inviter + " wants to invite you to the private game room (" + room.roomName + "). Yes or No? ";
                    roomname = room.roomName;
                    send(p2Sock, inviteMessage.c_str(), inviteMessage.size(), 0);

                    // 廣撥給對方
                    string broadcastMsg = "BROADCAST: " + inviter + " wants to invite you to the private game room (" + room.roomName + "). Please go to invitation management to response.\n";
                    send(p2Sock, broadcastMsg.c_str(), broadcastMsg.size(), 0);
                }
            }

            // 創建新的 socket 並設定它來監聽新Port
            int replySock = socket(AF_INET, SOCK_STREAM, 0);
            if (replySock == -1)
            {
                cerr << "Failed to create reply socket.\n";
                return;
            }

            sockaddr_in replyAddr{};
            replyAddr.sin_family = AF_INET;
            replyAddr.sin_addr.s_addr = INADDR_ANY;
            replyAddr.sin_port = 0;

            if (bind(replySock, (struct sockaddr *)&replyAddr, sizeof(replyAddr)) == -1)
            {
                cerr << "Failed to bind reply socket.\n";
                close(replySock);
                return;
            }

            if (listen(replySock, 1) == -1)
            {
                cerr << "Failed to listen on reply socket.\n";
                close(replySock);
                return;
            }

            // 獲取分配的端口
            socklen_t len = sizeof(replyAddr);
            if (getsockname(replySock, (struct sockaddr *)&replyAddr, &len) == -1)
            {
                cerr << "Failed to get socket name.\n";
                close(replySock);
                return;
            }
            int assignedPort = ntohs(replyAddr.sin_port);

            // 告知被邀請者使用該端口連接
            string portMessage = "RESPONSE: " + to_string(assignedPort);
            send(p2Sock, portMessage.c_str(), portMessage.size(), 0);

            // 等待被邀請者連接
            sockaddr_in clientAddr{};
            socklen_t clientAddrLen = sizeof(clientAddr);
            int newConnSock = accept(replySock, (struct sockaddr *)&clientAddr, &clientAddrLen);

            if (newConnSock != -1)
            {
                // 接收被邀請者的回覆
                char buffer[256];
                int bytesReceived = recv(newConnSock, buffer, sizeof(buffer) - 1, 0);
                // cout << "有收到被邀請者的回覆連線\n";
                if (bytesReceived > 0)
                {
                    buffer[bytesReceived] = '\0'; // 將回覆訊息 null-terminate
                    string response(buffer);

                    // 檢查回覆是否為 "Yes"
                    if (response == "Yes" || response == "yes")
                    {
                        // 呼叫 joinGameRoom 函式，讓被邀請者加入遊戲房間
                        joinGameRoom(p2Sock, roomname);
                        send(p1Sock, "RESPONSE: Someone joined room.\n", 32, 0);
                        cout << "有人加入房間\n";
                    }
                    else
                    {
                        send(p1Sock, "RESPONSE: The player rejected.\n", 32, 0);
                        cout << "玩家拒絕邀請\n";
                    }
                }
                close(newConnSock);
            }
            else
            {
                send(p1Sock, "RESPONSE: 玩家未連接到新Port。\n", 26, 0);
            }

            close(replySock);
        }
        else
        {
            send(p1Sock, "RESPONSE: 無法找到被邀請者的 socket。\n", 35, 0);
        }
    }
    else
    {
        send(p1Sock, "RESPONSE: 玩家不在線或不存在。\n", 25, 0);
    }
}

void endGame(const string &game_name)
{
    // 在 gameRooms 中尋找指定的遊戲房間
    auto it = find_if(gameRooms.begin(), gameRooms.end(),
                      [&game_name](const GameRoom &room)
                      { return room.roomName == game_name; });

    if (it != gameRooms.end())
    {
        // 遍歷遊戲房間中的所有玩家，將他們的狀態設為 idle
        for (const string &playerName : it->players)
        {
            if (playerDatabase.find(playerName) != playerDatabase.end())
            {
                playerDatabase[playerName].status = "idle";
                playerDatabase[playerName].online = true; // 確保玩家還在線上
            }
        }

        // 可以選擇刪除遊戲房間（如果不再需要）
        gameRooms.erase(it);
    }
    /* else
    {
        cout << "找不到遊戲房間：" << game_name << endl;
    } */
}

void loadPlayerData(const string &filename)
{
    ifstream inFile(filename);
    if (!inFile.is_open())
    {
        // 文件不存在，創建一個空文件
        ofstream outFile(filename);
        if (!outFile.is_open())
        {
            cerr << "Error creating file " << filename << ".\n";
            return;
        }
        cout << "File " << filename << " not found. A new file has been created.\n";
        outFile.close();
        return;
    }

    string line;
    while (getline(inFile, line))
    {
        stringstream ss(line);
        Player player;
        ss >> player.username >> player.password;
        if (!ss.fail())
        {
            playerDatabase[player.username] = player;
        }
    }
    inFile.close();
    cout << "Player data loaded from " << filename << endl;
}

void loadGameInfo(const string &filename)
{
    ifstream inFile(filename);
    if (!inFile.is_open())
    {
        // 文件不存在，創建一個空文件
        ofstream outFile(filename);
        if (!outFile.is_open())
        {
            cerr << "Error creating file " << filename << ".\n";
            return;
        }
        cout << "File " << filename << " not found. A new file has been created.\n";
        outFile.close();
        return;
    }

    string line;
    while (getline(inFile, line))
    {
        stringstream ss(line);
        GameInfo game;
        getline(ss, game.name, '|');
        getline(ss, game.author, '|');
        getline(ss, game.description, '|');
        getline(ss, game.filepath, '|');
        if (!ss.fail() && !game.name.empty())
        {
            gameDatabase[game.name] = game;
        }
    }
    inFile.close();
    cout << "Game info loaded from " << filename << endl;
}

void savePlayerData(const string &filename)
{
    ofstream outFile(filename);
    if (!outFile.is_open())
    {
        cerr << "Could not open file " << filename << " for writing.\n";
        return;
    }

    for (const auto &entry : playerDatabase)
    {
        const Player &player = entry.second;
        outFile << player.username << " " << player.password << "\n";
    }
    outFile.close();
    cout << "Player data saved to " << filename << endl;
}

void saveGameInfo(const string &filename)
{
    ofstream outFile(filename);
    if (!outFile.is_open())
    {
        cerr << "Could not open file " << filename << " for writing.\n";
        return;
    }

    for (const auto &entry : gameDatabase)
    {
        const GameInfo &game = entry.second;
        outFile << game.name << "|"
                << game.author << "|"
                << game.description << "|"
                << game.filepath << "\n";
    }
    outFile.close();
    cout << "Game info saved to " << filename << endl;
}

void publishGame(int clientSock, const string &gameName, const string &description)
{
    // 1. 獲取當前用戶（作者）
    if (clients.find(clientSock) == clients.end())
    {
        string errorMsg = "Error: User not found.\n";
        send(clientSock, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }
    string author = clients[clientSock];

    // 2. 準備接收文件內容
    char buffer[1024];
    string filepath = "server_game_folder/" + gameName + ".cpp";
    ofstream outFile(filepath, ios::binary);
    if (!outFile.is_open())
    {
        string errorMsg = "Error: Could not open file " + filepath + " for writing.\n";
        send(clientSock, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    // 3. 接收文件數據直到接收到 "END_OF_FILE" 標記
    string endOfFileMarker = "END_OF_FILE\n";
    string fileData = "";
    size_t eofPos;

    while (true)
    {
        ssize_t bytesReceived = recv(clientSock, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0)
        {
            break;
        }

        fileData.append(buffer, bytesReceived);

        // 檢查是否接收到文件結束標記
        eofPos = fileData.find(endOfFileMarker);
        if (eofPos != string::npos)
        {
            // 找到 "END_OF_FILE"，將標記之前的內容寫入文件
            outFile.write(fileData.c_str(), eofPos);
            break;
        }
        else
        {
            // 尚未找到 "END_OF_FILE"，直接寫入接收到的數據
            outFile.write(fileData.c_str(), fileData.size());
            fileData.clear(); // 清空 fileData
        }
    }

    // 關閉文件
    outFile.close();

    cout << "Game file " << gameName << " received and saved at " << filepath << endl;

    // 4. 將遊戲資訊添加到資料庫
    GameInfo newGame;
    newGame.name = gameName;
    newGame.author = author;
    newGame.description = description;
    newGame.filepath = filepath;

    gameDatabase[gameName] = newGame;

    // 5. 保存遊戲資訊到文件
    saveGameInfo(gameInfoFile);
}

void handleClient(int clientSock)
{
    char buffer[BUFFER_SIZE];
    while (true)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSock, buffer, BUFFER_SIZE - 1, 0);
        if (bytesReceived <= 0)
        {
            // cout << "客戶端斷開連接。\n";
            logoutPlayer(clientSock);
            close(clientSock);
            break;
        }

        string command(buffer);
        if (command.substr(0, 8) == "REGISTER")
        {
            size_t pos = command.find(' ', 9);
            if (pos != string::npos)
            {
                string username = command.substr(9, pos - 9);
                string password = command.substr(pos + 1);
                registerPlayer(clientSock, username, password);
            }
            else
            {
                string message = "Invalid registration command.\n";
                send(clientSock, message.c_str(), message.size(), 0);
            }
            savePlayerData(playerDataFile);
        }
        else if (command.substr(0, 5) == "LOGIN")
        {
            size_t pos = command.find(' ', 6);
            if (pos != string::npos)
            {
                string username = command.substr(6, pos - 6);
                string password = command.substr(pos + 1);
                loginPlayer(clientSock, username, password);
            }
            else
            {
                string message = "Invalid login command.\n";
                send(clientSock, message.c_str(), message.size(), 0);
            }
        }
        else if (command == "LOGOUT")
        {
            logoutPlayer(clientSock);
            savePlayerData(playerDataFile);
        }
        else if (command == "LIST_ROOM")
        {
            listRoom(clientSock);
        }
        else if (command == "LIST_PLAYER")
        {
            listPlayer(clientSock);
        }
        else if (command == "LIST_GAME")
        {
            listGame(clientSock);
        }
        else if (command == "LIST_YOUR_GAME")
        {
            listYourGame(clientSock);
        }
        else if (command.substr(0, 12) == "PUBLISH_GAME")
        {
            // 已知string message = "PUBLISH_GAME " + gameName + " " + description + "\n";
            size_t pos1 = command.find(' ', 13); // 找到 gameName 的結束位置

            if (pos1 != string::npos)
            {
                string gameName = command.substr(13, pos1 - 13); // 提取 gameName

                // 用 'substr(pos1 + 1)' 提取剩餘的部分作為description (包含空格)
                string description = command.substr(pos1 + 1); // 提取 description

                // 發布遊戲
                publishGame(clientSock, gameName, description);
            }
            else
            {
                string message = "ERROR: Invalid PUBLISH_GAME command format.\n";
                cout << message;
            }
        }
        else if (command.substr(0, 17) == "REQUEST_GAME_FILE")
        {
            // 從command中提取遊戲類型
            size_t gameTypeStartPos = command.find(' ', 17); // 找到遊戲類型開始的位置
            // todo
            if (gameTypeStartPos != string::npos)
            {
                string gameType = command.substr(gameTypeStartPos + 1, command.length() - gameTypeStartPos - 1);

                // 用 sendGameFile，將遊戲文件發送給客戶端
                sendGameFile(clientSock, gameType);
            }
            else
            {
                // 如果找不到遊戲類型，則發送error msg
                string errorMsg = "ERROR: Invalid REQUEST_GAME_FILE command format.\n";
                cout << errorMsg;
            }
        }
        else if (command == "LIST_INVITATION")
        {
            /* listInvitation(clientSock); */
            continue;
        }
        else if (command.substr(0, 11) == "CREATE_ROOM")
        {
            size_t pos1 = command.find(' ', 12);       // 找到 roomName 的結束位置
            size_t pos2 = command.find(' ', pos1 + 1); // 找到 roomType 的結束位置
            size_t pos3 = command.find(' ', pos2 + 1); // 找到 gameType 的結束位置

            if (pos1 != string::npos && pos2 != string::npos && pos3 != string::npos)
            {
                string roomName = command.substr(12, pos1 - 12);             // 提取房間名稱
                string roomType = command.substr(pos1 + 1, pos2 - pos1 - 1); // 提取房間類型
                string gameType = command.substr(pos2 + 1, pos3 - pos2 - 1); // 提取遊戲類型
                string portNumber = command.substr(pos3 + 1);                // 提取端口號，直到結束

                createGameRoom(clientSock, roomName, gameType, roomType, portNumber); // 傳遞所有必要的參數
            }
            else
            {
                string message = "ERROR: Invalid create room command.\n";
                send(clientSock, message.c_str(), message.size(), 0);
            }
        }
        else if (command.substr(0, 9) == "JOIN_ROOM")
        {
            string roomName = command.substr(10, command.size() - 10); // 提取房間名稱
            joinGameRoom(clientSock, roomName);                        // 傳遞所有必要的參數 (誰想加，加哪間)
        }
        else if (command.substr(0, 6) == "INVITE")
        {
            // cout << "收到邀請\n";
            string invitee = command.substr(7);
            invitePlayer(clientSock, invitee);
        }
        else if (command.substr(0, 8) == "END_GAME")
        {
            // cout << "結束遊戲\n";
            string game_name = command.substr(9);
            endGame(game_name);
        }
        else
        {
            string message = "無效的命令: " + command + "\n";
            send(clientSock, message.c_str(), message.size(), 0);
        }
    }
}

void runServer(int port)
{
    loadPlayerData(playerDataFile);

    int serverSock, clientSock;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0)
    {
        cerr << "Error creating socket.\n";
        return;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        cerr << "Error binding socket.\n";
        close(serverSock);
        return;
    }

    listen(serverSock, MAX_CLIENTS);
    cout << "Server listening on port " << port << endl;

    while (true)
    {
        clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrLen);
        if (clientSock < 0)
        {
            cerr << "Error accepting connection.\n";
            continue;
        }
        cout << "Client connected.\n";
        thread(handleClient, clientSock).detach();
    }

    close(serverSock);
}

int main()
{
    int port;
    cout << "Please enter the lobby server port: ";
    cin >> port;
    runServer(port);
    // return 0;
}
