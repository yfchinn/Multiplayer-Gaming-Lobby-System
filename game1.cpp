/* playDaVinciCode */
/* 實作 達文西密碼/終極密碼 桌遊 */

/* 初始，每位玩家四張牌 */
/* 輪流猜牌 */
/* 如果猜對，對方翻開，如果猜錯，自己隨機翻開一張 */
/* 當牌全部翻開，則輸 */

#include <iostream>
#include <string>
#include <cstring>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

using namespace std;

struct Cube
{
    int number;
    char color; // 'B' for Black, 'W' for White
    bool hidden = true;
    bool operator<(const Cube &other) const
    {
        return (number < other.number) || (number == other.number && color == 'W' && other.color == 'B');
    }
};

vector<Cube> initializeDeck()
{
    vector<Cube> deck;
    // 添加0-9號的黑白牌
    for (int i = 0; i <= 9; ++i)
    {
        deck.push_back({i, 'B', true});
        deck.push_back({i, 'W', true});
    }
    // 隨機打亂牌組
    srand(time(0));
    random_shuffle(deck.begin(), deck.end());
    return deck;
}

vector<Cube> drawInitialCardsP1(vector<Cube> &deck)
{
    vector<Cube> hand;
    for (int i = 0; i < 4; ++i)
    {
        hand.push_back(deck.front());
        deck.erase(deck.begin());
    }
    sort(hand.begin(), hand.end());
    return hand;
}

vector<Cube> drawInitialCardsP2(vector<Cube> &deck)
{
    vector<Cube> hand;
    for (int i = 0; i < 4; ++i)
    {
        hand.push_back(deck.back());
        deck.pop_back();
    }
    sort(hand.begin(), hand.end());
    return hand;
}

void displayHand(const vector<Cube> &hand, bool ismycard)
{
    cout << "+----+ ";
    for (size_t i = 1; i < hand.size(); ++i)
    {
        cout << "+----+ ";
    }
    cout << endl;

    for (size_t i = 0; i < hand.size(); ++i)
    {
        if (hand[i].hidden && !ismycard)
            cout << "| ?? | ";
        else
            cout << "| " << hand[i].color << hand[i].number << " | ";
    }
    cout << endl;

    cout << "+----+ ";
    for (size_t i = 1; i < hand.size(); ++i)
    {
        cout << "+----+ ";
    }
    cout << endl;
}

void sendHandCard(int sock, const vector<Cube> &hand)
{
    string message;
    message.clear();
    for (const auto &card : hand)
    {
        message += string(1, card.color) + " " + to_string(card.number) + " " + (card.hidden ? "1" : "0") + " ";
    }
    message += "\n"; // 加上換行符作為結束標記
    // Print the message being sent for debugging
    send(sock, message.c_str(), message.size(), 0);
}

// 接收手牌
vector<Cube> receiveHand(int sock)
{
    char buffer[1024] = {0};
    memset(buffer, 0, sizeof(buffer));
    vector<Cube> hand;
    string data;
    int bytesReceived;
    // 迴圈接收直到獲取完整的資料
    while ((bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytesReceived] = '\0';
        data += buffer;
        if (data.back() == '\n') // 結束標記
            break;
    }

    istringstream iss(data);
    // Print the contents of the stream
    // cout << "Stream content: " << iss.str() << endl;
    string color, number, hiddenStatus;
    while (iss >> color >> number >> hiddenStatus)
    {
        try
        {
            bool hidden = (hiddenStatus == "1");
            int num = stoi(number);
            hand.push_back({num, color[0], hidden});
        }
        catch (const invalid_argument &e)
        {
            cerr << "Error: Invalid data received in `receiveHand`." << endl;
            break;
        }
    }
    return hand;
}

// Main game loop
void playDaVinciCode(int sock, bool isInitiator)
{
    vector<Cube> deck = initializeDeck();
    vector<Cube> playerHand = isInitiator ? drawInitialCardsP1(deck) : drawInitialCardsP2(deck);
    vector<Cube> opponentHand;

    if (isInitiator)
    {
        sendHandCard(sock, playerHand);
        opponentHand = receiveHand(sock);
    }
    else
    {
        opponentHand = receiveHand(sock);
        sendHandCard(sock, playerHand);
    }

    cout << "Your hand: \n";
    displayHand(playerHand, true);
    cout << "Opponent's hand: \n";
    displayHand(opponentHand, false);

    bool gameOver = false;
    bool playerTurn = isInitiator;

    while (!gameOver)
    {
        if (playerTurn)
        {
            cout << "Your turn to guess." << endl;
            send(sock, "GUESS", 5, 0);

            cout << "Enter opponent card position (0-3): ";
            int guessPos;
            cin >> guessPos;
            send(sock, to_string(guessPos).c_str(), 2, 0);

            cout << "Enter opponent card number: ";
            int guessNum;
            cin >> guessNum;
            send(sock, to_string(guessNum).c_str(), 2, 0);

            char buffer[1024] = {0};
            recv(sock, buffer, sizeof(buffer), 0);

            if (string(buffer) == "CORRECT")
            {
                cout << "Correct guess! Opponent's card revealed." << endl;
                opponentHand[guessPos].hidden = false;
            }
            else
            {
                cout << "Incorrect guess! One of your cards is revealed." << endl;
                vector<int> hiddenIndices;
                for (int i = 0; i < playerHand.size(); ++i)
                {
                    if (playerHand[i].hidden)
                    {
                        hiddenIndices.push_back(i); // 保存隱藏卡片的索引
                    }
                }
                if (!hiddenIndices.empty())
                {
                    int randIdx = hiddenIndices[rand() % hiddenIndices.size()]; // 隨機選擇一個隱藏的卡片索引
                    playerHand[randIdx].hidden = false;
                }
            }
        }
        else
        {
            char buffer[1024] = {0};
            recv(sock, buffer, sizeof(buffer), 0);
            if (string(buffer) == "GUESS")
            {
                int pos, num;
                recv(sock, buffer, sizeof(buffer), 0);
                pos = stoi(buffer);
                recv(sock, buffer, sizeof(buffer), 0);
                num = stoi(buffer);

                if (num == playerHand[pos].number)
                {
                    send(sock, "CORRECT", 7, 0);
                    playerHand[pos].hidden = false;
                }
                else
                {
                    send(sock, "INCORRECT", 9, 0);
                    vector<int> hiddenIndices;
                    for (int i = 0; i < opponentHand.size(); ++i)
                    {
                        if (opponentHand[i].hidden)
                        {
                            hiddenIndices.push_back(i); // 保存隱藏卡片的索引
                        }
                    }
                    if (!hiddenIndices.empty())
                    {
                        int randIdx = hiddenIndices[rand() % hiddenIndices.size()]; // 隨機選擇一個隱藏的卡片索引
                        opponentHand[randIdx].hidden = false;
                    }
                }
            }
        }

        // Display updated hands after each guess
        cout << "\n=====================================" << endl;
        cout << "Your hand: \n";
        displayHand(playerHand, true);
        cout << "Opponent's hand: \n";
        displayHand(opponentHand, false);

        gameOver = all_of(playerHand.begin(), playerHand.end(), [](Cube card)
                          { return !card.hidden; }) ||
                   all_of(opponentHand.begin(), opponentHand.end(), [](Cube card)
                          { return !card.hidden; });
        playerTurn = !playerTurn;
    }

    if (all_of(playerHand.begin(), playerHand.end(), [](Cube card)
               { return !card.hidden; }))
        cout << "You lost! All your cards are revealed." << endl;
    else
        cout << "Congratulations! You won." << endl;
}

// initiator: ./executableName 18080 1 0.0.0.0
// not-initiator: ./executableName 18080 0 <initiatorIP>
int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        cerr << "Usage: " << argv[0] << " <port> <isInitiator> <initiatorIP>" << endl;
        return 1;
    }

    // Parse command-line arguments
    int port = stoi(argv[1]); // 使用指定的埠口
    bool isInitiator = stoi(argv[2]) == 1;
    string initiatorIP = argv[3]; // 接收發起者的 IP 地址

    cout << "@@" << port << " " << isInitiator << " " << initiatorIP << endl;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (isInitiator)
    {
        // 發起者：綁定本地地址並作為伺服器監聽
        inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr); // 綁定所有可用的網路接口

        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("Bind failed");
            close(sock);
            return 1;
        }

        listen(sock, 1);

        cout << "Waiting for connection on port " << port << "..." << endl;
        int clientSock = accept(sock, nullptr, nullptr);
        if (clientSock < 0)
        {
            perror("Accept failed");
            close(sock);
            return 1;
        }
        cout << "Connection established with receiver." << endl;
        playDaVinciCode(clientSock, true);
        close(clientSock);
    }
    else
    {
        // 讓接收方等待5秒，確保發起者已經準備好監聽
        sleep(5);

        // 接收方：連接到發起者的指定 IP 和埠口
        if (inet_pton(AF_INET, initiatorIP.c_str(), &addr.sin_addr) <= 0)
        {
            cerr << "Invalid IP address: " << initiatorIP << endl;
            close(sock);
            return 1;
        }

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("Connection failed");
            close(sock);
            return 1;
        }
        cout << "Connected to initiator at " << initiatorIP << " on port " << port << "." << endl;
        playDaVinciCode(sock, false);
    }

    close(sock);
    return 0;
}