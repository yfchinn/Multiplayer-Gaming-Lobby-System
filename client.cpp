// g++ client.cpp game1.cpp game2.cpp -o c

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <fcntl.h>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <filesystem>
#include <cstdlib>

using namespace std;
namespace fs = std::filesystem;

bool isLoggedIn = false;
string IpAddress = "140.113.235.151"; // modify it! server's ip address
string myIpAddress;
string client_game_folder;
string client_download_game_folder;

atomic<bool> isRunning(true);
mutex invitationMutex;

vector<string> invitations; // 儲存邀請
queue<string> broadcastQueue;
queue<string> invitationQueue;
queue<string> responseQueue;
mutex queueMutex;
condition_variable queueCondVar;

/* ----------------------------通用函式------------------------------ */

int createSocket()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return sock;
}

void setupServerAddress(struct sockaddr_in &server_addr, const string &server_ip, int port)
{
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }
}

bool connectToServer(int sock, struct sockaddr_in &server_addr)
{
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        return false;
    }
    // cout << "Connected to the lobby server.\n";
    return true;
}

void sendMessage(int sock, const string &message)
{
    send(sock, message.c_str(), message.length(), 0);
}

string receiveMessage(int sock)
{
    char buffer[1024] = {0};
    int bytesRead = read(sock, buffer, sizeof(buffer));
    if (bytesRead > 0)
    {
        return std::string(buffer, bytesRead); // 返回讀取的內容
    }
    else if (bytesRead == 0)
    {
        return ""; // 連線已關閉
    }
    else if (errno == EWOULDBLOCK || errno == EAGAIN)
    {
        return "NO_DATA"; // 非阻塞狀態無數據
    }
    else
    {
        perror("Error reading from socket");
        return "ERROR"; // 錯誤情況
    }
}

string waitForResponse()
{
    unique_lock<std::mutex> lock(queueMutex);
    queueCondVar.wait(lock, []
                      { return !responseQueue.empty(); });
    string response = responseQueue.front();
    responseQueue.pop();
    return response;
}

bool fileExists(const string &filename)
{
    ifstream file(filename);
    return file.is_open();
}

void getGameFile(int sock, const string &gameType)
{
    // 發送請求以下載指定遊戲文件
    string message = "REQUEST_GAME_FILE " + gameType + ".cpp";
    sendMessage(sock, message);

    // 等待回應
    string response = waitForResponse();
    if (response.substr(0, 10) != "FILE_SIZE ")
    {
        cerr << "Error: Invalid file size response. Received: " << response << endl;
        return;
    }

    // 解析文件大小
    size_t fileSize = stoi(response.substr(10));
    cout << "File size: " << fileSize << " bytes\n";

    // 向服務器確認準備接收文件
    sendMessage(sock, "READY_TO_RECEIVE\n");

    // 設定儲存檔案的路徑
    string filePath = client_download_game_folder + gameType + ".cpp";

    // 打開文件以進行保存
    ofstream outFile(filePath, ios::binary);
    if (!outFile.is_open())
    {
        cerr << "Error: Unable to open file " << filePath << " for writing.\n";
        return;
    }

    // 分塊接收並寫入文件
    size_t totalBytesReceived = 0;
    while (totalBytesReceived < fileSize)
    {
        string chunk = waitForResponse(); // 從 RESPONSE 隊列中接收數據
        if (chunk == "ERROR" || chunk.empty())
        {
            cerr << "Error receiving file data.\n";
            outFile.close();
            return;
        }
        // cout << chunk << endl; //@@

        outFile.write(chunk.c_str(), chunk.size());
        totalBytesReceived += chunk.size();
        cout << "Received " << totalBytesReceived << "/" << fileSize << " bytes...\n";
    }

    // 確認文件接收完成
    string endOfFileMessage = waitForResponse();
    if (endOfFileMessage == "END_OF_FILE")
    {
        cout << "Game file " << gameType << ".cpp received and saved to " << filePath << endl;
    }
    else
    {
        cerr << "Error: Missing END_OF_FILE marker.\n";
    }

    outFile.close();
}

void compileAndRunGame(const string &gameName, const string &executableName, int clientSocket, bool passToGame)
{
    // Define file paths
    string filePath1 = client_game_folder + gameName + ".cpp";
    string filePath2 = client_download_game_folder + gameName + ".cpp";

    string gameFile;

    // Check if the file exists in either location
    if (filesystem::exists(filePath1))
    {
        gameFile = filePath1;
    }
    else if (filesystem::exists(filePath2))
    {
        gameFile = filePath2;
    }
    else
    {
        cerr << "Game file " << gameName << ".cpp not found in either folder." << endl;
        return;
    }

    // Compile the game file
    string compileCommand = "g++ " + gameFile + " -o " + executableName;
    int compileResult = system(compileCommand.c_str());
    if (compileResult != 0)
    {
        cerr << "Compilation failed for " << gameFile << endl;
        return;
    }

    // Execute the compiled file
    string runCommand = "./" + executableName + " " + to_string(11126) + " " + (passToGame ? "1" : "0");
    if (passToGame)
    {
        runCommand += " 0.0.0.0";
    }
    else
    {
        struct sockaddr_in peerAddr;
        socklen_t addrLen = sizeof(peerAddr);

        if (getpeername(clientSocket, (struct sockaddr *)&peerAddr, &addrLen) == 0)
        {
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &peerAddr.sin_addr, ipStr, sizeof(ipStr));
            runCommand += " " + std::string(ipStr);
        }
        else
        {
            perror("getpeername failed");
        }
    }
    int runResult = system(runCommand.c_str());
    if (runResult != 0)
    {
        cerr << "Execution failed for " << executableName << endl;
    }

    close(clientSocket); // Close the socket after usage
}

/* --------------------------具體功能------------------------------ */

void registerUser(int sock, const string &username, const string &password)
{
    string message = "REGISTER " + username + " " + password;
    sendMessage(sock, message);

    string response = waitForResponse();
    cout << response << "\n";

    if (response == "Username already exists.")
    {
        cout << "Please try again.\n";
    }
}

void loginUser(int sock, const string &username, const string &password)
{
    string message = "LOGIN " + username + " " + password;
    sendMessage(sock, message);

    string response = waitForResponse();
    cout << response << endl;

    if (response.find("Login success.") != string::npos)
    {
        isLoggedIn = true;
    }

    client_game_folder = username + "_game_folder/";
    client_download_game_folder = username + "_download_game_folder/";
}

void logoutUser(int sock)
{
    string message = "LOGOUT";
    sendMessage(sock, message);

    string response = waitForResponse();
    cout << response << endl;

    isLoggedIn = false;
}

void endGame(int serverSocket, const string &roomName)
{
    // 發送遊戲結束的請求給 server
    string message = "END_GAME " + roomName;
    send(serverSocket, message.c_str(), message.size(), 0);
}

void startGame(int serverSocket, int clientSocket, const string &roomName, const string &gameType, bool passToGame)
{
    if (gameType == "game1")
    {
        // playDaVinciCode(clientSocket, passToGame);
        compileAndRunGame(gameType, "game1_executable", clientSocket, passToGame);
    }
    else if (gameType == "game2")
    {
        // playBlackjack(clientSocket, passToGame);
        compileAndRunGame(gameType, "game2_executable", clientSocket, passToGame);
    }
    else
    {
        cout << "Unknown game name: " << gameType << "\n";
        return;
    }

    // 遊戲結束後呼叫 endGame
    endGame(serverSocket, roomName);
}

int toConnect(int sock, const string &portNumber, const string &gameType, const string &roomName)
{
    // 要傳給玩家二的訊息(藉由lobby)
    string Message = "Joined room successfully. " + myIpAddress + " " + portNumber + " " + gameType;
    sendMessage(sock, Message);
    // cout << "@" << Message << endl; //

    // 收到來自玩家二的socket
    // 傳給他 開始吧
    struct sockaddr_in gameServerAddr;
    socklen_t addrlen = sizeof(gameServerAddr);
    setupServerAddress(gameServerAddr, myIpAddress, stoi(portNumber));
    int gameSock = createSocket();
    if (bind(gameSock, (struct sockaddr *)&gameServerAddr, sizeof(gameServerAddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(gameSock, 1) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    cout << "Server waiting for connection..." << endl;
    int playerSock = accept(gameSock, (struct sockaddr *)&gameServerAddr, &addrlen);
    if (playerSock < 0)
    {
        perror("Error accepting player");
        exit(EXIT_FAILURE);
    }
    cout << "A player has joined your room.\n";

    close(gameSock);
    return playerSock;
}

void createRoom(int sock, const string &roomName, const string &roomType, const string &gameType, const string &portNumber)
{
    string message = "CREATE_ROOM " + roomName + " " + roomType + " " + gameType + " " + portNumber;
    sendMessage(sock, message);
    cout << waitForResponse();

    // 处理游戏文件
    string gameFilePath = client_game_folder + gameType + ".cpp";
    if (!fileExists(gameFilePath)) // 如果本地沒有該遊戲文件
    {
        cout << "Requesting the game file from server...\n";
        getGameFile(sock, gameType); // 從server請求遊戲文件
    }

    if (roomType == "public") // 等待其他玩家加入的循環
    {
        while (true)
        {
            string response = waitForResponse();
            if (response == "Someone joined room.\n")
            {
                cout << "A player has joined your room.\n";

                // 建立P2P連線
                int playerSock = toConnect(sock, portNumber, gameType, roomName);

                // 開始遊戲
                startGame(sock, playerSock, roomName, gameType, true);

                // close(gameSock);
                close(playerSock);
                break;
            }
        }
    }
    else if (roomType == "private") // 發送邀請
    {
        // cin username
        string invitedUsername;
        getline(cin, invitedUsername);

        // 發送邀請訊息到lobby server
        string inviteMessage = "INVITE " + invitedUsername;
        sendMessage(sock, inviteMessage);

        while (true)
        {
            string response = waitForResponse();

            if (response == "Someone joined room.\n")
            {
                cout << "Someone joined room.\n";

                // 建立P2P連線
                int playerSock = toConnect(sock, portNumber, gameType, roomName);
                // cout << "建立連線了\n";

                // 開始遊戲
                startGame(sock, playerSock, roomName, gameType, true);

                // close(gameSock);
                close(playerSock);
                break;
            }
            else if (response.find("The player rejected.") != string::npos)
            {
                cout << "The player rejected the invitation.\n";
                break; // 如果被拒絕，回到主選單
            }
            else
            {
                // 不該進到這裡
                cout << "else: " << response << "---" << endl; // 收到甚麼?
            }
        }
    }
}

void acceptInvite(int sock0, const string &response)
{
    // 從伺服器接收動態端口的回應
    string portStr = waitForResponse();
    int assignedPort = stoi(portStr);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        cerr << "Failed to create socket.\n";
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(assignedPort);
    if (inet_pton(AF_INET, IpAddress.c_str(), &serverAddr.sin_addr) <= 0)
    {
        cerr << "Invalid address or address not supported.\n";
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        cerr << "Connection to the server failed.\n";
        close(sock);
        return;
    }
    sendMessage(sock, response);
}

void joinRoom(int sock, const string &roomName, const bool isPrivate)
{
    if (!isPrivate)
    {
        string message = "JOIN_ROOM " + roomName;
        sendMessage(sock, message);
    }

    // 等待伺服器回應
    string response = waitForResponse();
    if (response.find("Joined room successfully.") != string::npos)
    {
        // cout << "@@" << response << endl; //
        // 解析伺服器傳回的房間資訊 從response裡找到
        // 取得起始位置
        size_t startPos = response.find("Joined room successfully.") + string("Joined room successfully. ").length();
        // 擷取 IP、Port 和 GameType
        size_t ipEnd = response.find(' ', startPos);
        string gameServerIp = response.substr(startPos, ipEnd - startPos);
        size_t portEnd = response.find(' ', ipEnd + 1);
        string gameServerPort = response.substr(ipEnd + 1, portEnd - ipEnd - 1);
        string gameType = response.substr(portEnd + 1);

        // 顯示訊息
        cout << "Game server IP: " << gameServerIp << "\n";
        cout << "Game server port: " << gameServerPort << "\n";
        cout << "Welcome to " << gameType << " \n";

        // 連到該ip and port
        // 跟他說自己的socket(讓他連)
        // 收到他的回覆
        struct sockaddr_in gameServerAddr;
        setupServerAddress(gameServerAddr, gameServerIp, std::stoi(gameServerPort));
        int gameSock = createSocket();
        if (connectToServer(gameSock, gameServerAddr))
        {
            cout << "Successfully connected to the game server. Starting the game...\n";

            // 進遊戲前 如果client_game_folder沒有該遊戲 要下載
            string gameFilePath = client_game_folder + gameType + ".cpp";
            if (!fileExists(gameFilePath)) // 如果本地沒有該遊戲文件
            {
                cout << "Requesting the game file from server...\n";
                getGameFile(sock, gameType); // 從server請求遊戲文件
            }

            startGame(sock, gameSock, roomName, gameType, false);
        }
        close(gameSock);
    }
    else
    {
        cout << response << "\n"; // 顯示錯誤訊息
    }
}

void listRoom(int sock)
{
    string message = "LIST_ROOM";
    sendMessage(sock, message);
    string response = waitForResponse();
    cout << response;
}

void listInvitation(int sock)
{
    string message = "LIST_INVITATION";
    sendMessage(sock, message);

    while (!invitationQueue.empty())
    {
        string response = invitationQueue.front();
        cout << response;

        size_t invitePos = response.find("wants to invite you");
        if (invitePos != string::npos)
        {
            // 從邀請訊息中提取 roomName
            size_t startPos = response.find("game room (") + 11; // "game room (" 的位置後一位開始
            size_t endPos = response.find(")", startPos);
            string roomName = response.substr(startPos, endPos - startPos); // 擷取 roomName

            string accept;
            getline(cin, accept);
            if (accept == "yes" || accept == "Yes")
            {
                acceptInvite(sock, accept);
                joinRoom(sock, roomName, 1);

                invitationQueue.pop();
                break;
            }
            else
            {
                cout << "Invitation declined.\n";
                acceptInvite(sock, accept);
                invitationQueue.pop();
            }
        }
    }
}

void listPlayer(int sock)
{
    string message = "LIST_PLAYER";
    sendMessage(sock, message);
    string response = waitForResponse();
    cout << response;
}

void listGame(int sock)
{
    string message = "LIST_GAME";
    sendMessage(sock, message);
    string response = waitForResponse();
    cout << response;
}

void listYourGame(int sock)
{
    string message = "LIST_YOUR_GAME";
    sendMessage(sock, message);
    string response = waitForResponse();
    cout << response;
}

void publishGame(int sock, const string &gameName, const string &description, const string &filePath)
{
    string message = "PUBLISH_GAME " + gameName + " " + description + "\n";
    sendMessage(sock, message);

    // 2. 打開文件
    ifstream inFile(filePath, ios::binary);
    if (!inFile.is_open())
    {
        cerr << "Error: Could not open file " << filePath << " for reading.\n";
        return;
    }

    // 3. 發送文件大小
    inFile.seekg(0, ios::end);
    size_t fileSize = inFile.tellg();
    inFile.seekg(0, ios::beg);

    // 4. 分塊發送文件內容
    char buffer[1024];
    while (!inFile.eof())
    {
        inFile.read(buffer, sizeof(buffer)); // 讀取數據
        streamsize bytesRead = inFile.gcount();
        if (bytesRead > 0)
        {
            send(sock, buffer, bytesRead, 0); // 發送數據
        }
    }
    inFile.close();

    // 5. 通知server文件傳輸完成
    string endOfFileMessage = "END_OF_FILE\n";
    sendMessage(sock, endOfFileMessage);

    cout << "Game published successfully!\n";
}

/* ----------------------------基礎設定------------------------------ */

bool isPortAvailable(int port)
{
    if (port < 10000)
        return false;

    if (port > 65535)
        return false;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        std::cerr << "Failed to create socket.\n";
        return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bool available = (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);

    close(sock); // 釋放 socket 資源
    return available;
}

void setNonBlocking(int sock)
{
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1)
    {
        cerr << "Failed to get socket flags.\n";
        exit(EXIT_FAILURE);
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        cerr << "Failed to set socket to non-blocking mode.\n";
        exit(EXIT_FAILURE);
    }
}

vector<pair<string, string>> splitMessagesByTags(const string &rawMessage)
{
    vector<pair<string, string>> result;
    vector<string> tags = {"BROADCAST: ", "INVITATION: ", "RESPONSE: "};

    size_t pos = 0;
    while (pos < rawMessage.size())
    {
        size_t nextPos = string::npos;
        string currentTag;

        // 找到下一個標籤
        for (const string &tag : tags)
        {
            size_t tagPos = rawMessage.find(tag, pos);
            if (tagPos != string::npos && (nextPos == string::npos || tagPos < nextPos))
            {
                nextPos = tagPos;
                currentTag = tag;
            }
        }

        if (!currentTag.empty())
        {
            // 如果找到標籤，提取該標籤之後的內容
            size_t contentStart = nextPos + currentTag.size();
            size_t nextTagPos = string::npos;

            // 尋找下一個標籤來劃定範圍
            for (const string &tag : tags)
            {
                size_t tagPos = rawMessage.find(tag, contentStart);
                if (tagPos != string::npos && (nextTagPos == string::npos || tagPos < nextTagPos))
                {
                    nextTagPos = tagPos;
                }
            }

            string content = rawMessage.substr(contentStart, nextTagPos - contentStart);
            result.emplace_back(currentTag.substr(0, currentTag.size() - 2), content); // 去掉 ": " 部分
            pos = nextTagPos;                                                          // 移動到下一個標籤的位置
        }
        else
        {
            // 沒有更多標籤，結束處理
            break;
        }
    }

    return result;
}

void serverListener(int sock)
{
    while (isRunning)
    {
        string message = receiveMessage(sock);
        if (message == "NO_DATA") // 無數據，稍作等待
        {
            this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        else if (message.empty()) // 連線關閉
        {
            cerr << "\n[Server] Connection closed by server.\n";
            isRunning = false;
            break;
        }
        else if (message == "ERROR") // 發生錯誤
        {
            cerr << "\n[Server] Error receiving message.\n";
            isRunning = false;
            break;
        }

        // 根據標籤分發訊息
        {
            lock_guard<mutex> lock(queueMutex);
            vector<pair<string, string>> messages = splitMessagesByTags(message);
            for (const auto &[tag, content] : messages)
            {
                if (tag == "BROADCAST")
                {
                    cout << "\nBROADCAST: " << content << endl;
                }
                else if (tag == "INVITATION")
                {
                    invitationQueue.push(content);
                }
                else if (tag == "RESPONSE")
                {
                    responseQueue.push(content);
                }
                else
                {
                    cerr << "[Server] Unknown message type: " << tag << " " << content << endl;
                }
            }
        }
        queueCondVar.notify_all(); // 通知主執行緒或其他處理邏輯
    }
}

void handleInvitations()
{
    lock_guard<mutex> lock(invitationMutex);
    if (invitations.empty())
    {
        cout << "No new invitations.\n";
    }
    else
    {
        cout << "You have the following invitations:\n";
        for (size_t i = 0; i < invitations.size(); ++i)
        {
            cout << i + 1 << ". " << invitations[i] << endl;
        }
        cout << "Enter the number of the invitation to accept (or 0 to ignore): ";
        int choice;
        cin >> choice;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if (choice > 0 && choice <= invitations.size())
        {
            cout << "You accepted the invitation: " << invitations[choice - 1] << endl;
            invitations.erase(invitations.begin() + choice - 1);
        }
        else if (choice == 0)
        {
            cout << "Ignored all invitations.\n";
        }
        else
        {
            cout << "Invalid choice.\n";
        }
    }
}

void closeConnection(int sock)
{
    close(sock);
    cout << "Disconnected from the lobby server.\n";
}

int main()
{
    cout << "Please enter your ip address: ";
    getline(cin, myIpAddress);

    int port;
    cout << "Please enter the lobby server port: ";
    cin >> port;

    int sock = createSocket();
    struct sockaddr_in server_addr;
    setupServerAddress(server_addr, IpAddress, port);

    if (!connectToServer(sock, server_addr))
    {
        cerr << "Failed to connect to server.\n";
        return EXIT_FAILURE;
    }

    setNonBlocking(sock);
    thread serverThread(serverListener, sock);

    string choice;
    string username, password;
    string roomName, roomType, gameType, portNumber;

    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    while (1)
    {
        cout << "\n=== Lobby Client Menu ===\n";
        if (!isLoggedIn)
        {
            cout << "1. Register\n";
            cout << "2. Login\n";
            cout << "3. Exit\n";
        }
        else
        {
            cout << "1. Create Room\n";
            cout << "2. List rooms\n";
            cout << "3. List games\n";
            cout << "4. List online players\n";
            cout << "5. Join Room\n";
            cout << "6. Invitation management\n";
            cout << "7. Game management\n";
            cout << "8. Logout\n";
        }
        cout << "Enter an option: ";

        getline(cin, choice);

        if (!isLoggedIn)
        {
            if (choice == "1") // Register
            {
                cout << "Enter your username: ";
                getline(cin, username);
                cout << "Enter your password: ";
                getline(cin, password);
                registerUser(sock, username, password);
            }
            else if (choice == "2") // Login
            {
                cout << "Enter username: ";
                getline(cin, username);
                cout << "Enter password: ";
                getline(cin, password);
                loginUser(sock, username, password);
            }
            else if (choice == "3") // Exit
            {
                closeConnection(sock);
                isRunning = false;
                return 0;
            }
            else
            {
                cout << "Invalid choice. Please try again.\n";
            }
        }
        else if (isLoggedIn)
        {
            if (choice == "1") // Create room
            {
                cout << "Enter room name: ";
                getline(cin, roomName);

                cout << "Enter game type (game1/game2): ";
                getline(cin, gameType);
                while (gameType != "game1" && gameType != "game2")
                {
                    cout << "Invalid game type. Please enter game1 or game2.\n";
                    cout << "Enter game type (game1/game2): ";
                    getline(cin, gameType);
                }

                cout << "Enter room type (public/private): ";
                getline(cin, roomType);
                while (roomType != "public" && roomType != "private")
                {
                    cout << "Invalid room type. Please enter public or private.\n";
                    cout << "Enter room type (public/private): ";
                    getline(cin, roomType);
                }

                cout << "Enter the port number to bind (10000~65535): ";
                getline(cin, portNumber);
                bool av = isPortAvailable(stoi(portNumber));
                while (!av)
                {
                    cout << "Port " << stoi(portNumber) << " is not available. Please enter another port.\n";
                    cout << "Enter the port number to bind (10000~65535): ";
                    getline(cin, portNumber);
                    av = isPortAvailable(stoi(portNumber));
                }
                createRoom(sock, roomName, roomType, gameType, portNumber);
            }
            else if (choice == "2") // List rooms
            {
                listRoom(sock);
            }
            else if (choice == "3") // List games
            {
                listGame(sock);
            }
            else if (choice == "4") // List online players
            {
                listPlayer(sock);
            }
            else if (choice == "5") // Join room
            {
                cout << "Enter the room name to join: ";
                string roomNameStr;
                getline(cin, roomNameStr);
                joinRoom(sock, roomNameStr, 0);
            }
            else if (choice == "6") // Invitation management
            {
                listInvitation(sock);
            }
            else if (choice == "7") // Game management
            {
                while (1)
                {
                    cout << "\nGame Management: " << endl;
                    cout << "(1) List your games\n(2) Publish your game\n(3) Back to lobby\n";
                    cout << "Enter your choice: ";
                    getline(cin, choice);
                    if (choice == "1") // List your games
                    {
                        listYourGame(sock);
                    }
                    else if (choice == "2") // publish games
                    {
                        string gameName, description, filePath;
                        while (1)
                        {
                            cout << "Please enter the game file name (ignore .cpp): ";
                            getline(cin, gameName);

                            filePath = client_game_folder + gameName + ".cpp";

                            // 檢查文件是否存在
                            if (fs::exists(filePath))
                                break;
                            else
                                cout << "File not found. Please try again.\n";
                        }
                        cout << "Please enter the game description: ";
                        getline(cin, description);
                        publishGame(sock, gameName, description, filePath);
                    }
                    else if (choice == "3") // back to lobby
                    {
                        break;
                    }
                    else
                    {
                        cout << "Invalid choice. Please try again.\n";
                    }
                }
            }
            else if (choice == "8") // Logout
            {
                logoutUser(sock);
            }
            else
            {
                cout << "Invalid choice. Please try again.\n";
            }
        }
    }

    if (serverThread.joinable())
    {
        serverThread.join();
    }

    closeConnection(sock);
    return 0;
}
