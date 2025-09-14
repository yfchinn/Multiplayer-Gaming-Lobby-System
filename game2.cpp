/* Blackjack */

#include <iostream>
#include <algorithm>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>

using namespace std;

struct Card
{
    string suit;
    int value;
};

// 生成四副牌
vector<Card> createDeck()
{
    vector<Card> deck;
    string suits[] = {"♥️", "♦", "♣", "♠️"}; //{"h", "d", "c", "s"};
    for (int d = 0; d < 4; ++d)            // Add four decks
    {
        for (const string &suit : suits)
        {
            for (int i = 1; i <= 13; ++i)
            {
                deck.push_back({suit, i});
            }
        }
    }
    return deck;
}

// 洗牌
void shuffleDeck(vector<Card> &deck)
{
    srand(time(0));
    random_shuffle(deck.begin(), deck.end());
}

// 計算分數，A可以是1或11
int calculateScore(const vector<Card> &hand)
{
    int score = 0, aces = 0;
    for (const Card &card : hand)
    {
        if (card.value >= 10)
            score += 10;
        else if (card.value == 1)
        { // A
            score += 11;
            ++aces;
        }
        else
            score += card.value;
    }
    // 若超過21，將A的值從11改為1
    while (score > 21 && aces > 0)
    {
        score -= 10;
        --aces;
    }
    return score;
}

// 發牌
void dealCards(vector<Card> &deck, vector<Card> &hand, int count)
{
    for (int i = 0; i < count; ++i)
    {
        hand.push_back(deck.back());
        deck.pop_back();
    }
}

// 顯示玩家手牌
void displayHand(const vector<Card> &hand, bool hide, bool hideFirstCard)
{
    for (size_t i = 0; i < hand.size(); ++i)
    {
        if (hide)
        {
            if (i != 0 && hideFirstCard)
            {
                string displayValue;
                if (hand[i].value == 11)
                    displayValue = "J";
                else if (hand[i].value == 12)
                    displayValue = "Q";
                else if (hand[i].value == 13)
                    displayValue = "K";
                else
                    displayValue = to_string(hand[i].value);

                cout << hand[i].suit << " " << displayValue << " ";
            }
            else
            {
                cout << "[ ?? ] ";
            }
        }
        else
        {
            string displayValue;
            if (hand[i].value == 11)
                displayValue = "J";
            else if (hand[i].value == 12)
                displayValue = "Q";
            else if (hand[i].value == 13)
                displayValue = "K";
            else
                displayValue = to_string(hand[i].value);

            cout << hand[i].suit << " " << displayValue << " ";
        }
    }
    cout << endl;
}

// 發送訊息
void _sendMessage(int sock, const string &message)
{
    size_t totalSent = 0;
    while (totalSent < message.size())
    {
        int sent = send(sock, message.c_str() + totalSent, message.size() - totalSent, 0);
        if (sent == -1)
        {
            cerr << "Error in sending message.\n";
            return;
        }
        totalSent += sent;
    }
}

// 接收訊息
string _receiveMessage(int sock)
{
    char buffer[1024] = {0};
    int bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0)
    {
        return "";
    }
    buffer[bytesRead] = '\0';
    return string(buffer);
}

// 發送手牌資訊給對方
void sendHand(int sock, const vector<Card> &hand)
{
    string handMessage = "Hand\n";
    for (const Card &card : hand)
    {
        handMessage += card.suit + " " + to_string(card.value) + " ";
    }
    handMessage += "\0"; // Null-terminate the message
    _sendMessage(sock, handMessage);
}

// 接收對方的手牌更新
vector<Card> receiveOpponentHand(int sock)
{
    string message = _receiveMessage(sock);
    vector<Card> opponentHand;
    if (!message.empty())
    {
        stringstream ss(message);
        string suit;
        int value;
        string tmp;
        ss >> tmp;   // 用掉Hand
        ss.ignore(); // 用掉\n
        while (ss >> suit >> value)
        {
            opponentHand.push_back({suit, value});
        }
    }
    return opponentHand;
}

// Main game loop
void playBlackjack(int clientSock, bool isDealer)
{
    srand(time(0));

    vector<Card> deck = createDeck();
    shuffleDeck(deck);

    vector<Card> dealerHand, playerHand;
    dealCards(deck, dealerHand, 2);
    dealCards(deck, playerHand, 2);

    // 展示初始局面
    if (isDealer)
    {
        cout << "Your hand: ";
        displayHand(dealerHand, false, true);
        cout << "Player's hand: ";
        displayHand(playerHand, true, false);
    }
    else
    {
        cout << "Dealer's hand: ";
        displayHand(dealerHand, true, true); // 若為庄家，則隱藏第一張牌
        cout << "Your hand: ";
        displayHand(playerHand, false, false);
    }

    // 如果玩家是庄家
    if (isDealer)
    {
        int dealerScore = calculateScore(dealerHand);
        while (dealerScore < 17)
        {
            dealCards(deck, dealerHand, 1);
            dealerScore = calculateScore(dealerHand);
            cout << "Your hand: ";
            displayHand(dealerHand, false, true);
        }

        /* dealerScore = calculateScore(dealerHand);
        if (dealerScore < 21)
        {
            cout << "Your current score: " << dealerScore << ". Hit or Stand? ";
            string dealerMove;
            cin >> dealerMove;
            if (dealerMove == "Hit")
            {
                dealCards(deck, dealerHand, 1);
                cout << "Your hand: ";
                displayHand(dealerHand, false, true);
                if (calculateScore(dealerHand) > 21)
                {
                    cout << "You busted!\n";
                }
            }
            else if (dealerMove == "Stand")
            {
                cout << "You chose to stand.\n";
            }
            else
            {
                cout << "Invalid move! Please type 'Hit' or 'Stand'.\n";
            }
        } */
    }
    else
    {
        while (true)
        {
            int playerScore = calculateScore(playerHand);
            cout << "Your current score: " << playerScore << ". Hit or Stand? ";
            string playerMove;
            cin >> playerMove;

            if (playerMove == "Hit")
            {
                dealCards(deck, playerHand, 1);
                cout << "Your hand: ";
                displayHand(playerHand, false, false);

                if (calculateScore(playerHand) > 21)
                {
                    cout << "You busted!\n";
                    break;
                }
            }
            else if (playerMove == "Stand")
            {
                cout << "You chose to stand.\n";
                break;
            }
            else
            {
                cout << "Invalid move! Please type 'Hit' or 'Stand'.\n";
            }
        }
    }

    // 發出turn ended.    // 要收到turn ended.\n 才繼續往下做
    _sendMessage(clientSock, "Turn ended.\n"); // 通知對方回合結束
    // 等待對方的 turn ended 訊息
    string response = _receiveMessage(clientSock);
    if (response.find("Turn ended") != 0)
    {
        cout << "Error: received: " << response << endl;
        return;
    }

    // cout << "--------" << response << "------\n";

    // 發送自己的牌組給對方
    sendHand(clientSock, isDealer ? dealerHand : playerHand);
    vector<Card> opponentHand;
    opponentHand.clear();
    if (response.find("Hand") == 12)
    {
        // cout << "can find Hand\n"; //
        if (!response.empty())
        {
            stringstream ss(response);

            // 跳過 "Turn ended." 和 "Hand" 部分
            string temp;
            ss >> temp; // 讀取 "Turn"
            // cout<<"1. "<<temp<<endl;
            ss >> temp; // 讀取 "ended."
            // cout<<"2. "<<temp<<endl;
            ss.ignore(); // 忽略換行符 "\n"
            ss >> temp;  // 讀取 "Hand"
            // cout<<"3. "<<temp<<endl;
            ss.ignore(); // 忽略換行符 "\n"

            string suit;
            int value;
            while (ss >> suit >> value)
            {
                // cout << suit << " " << value << endl;
                opponentHand.push_back({suit, value});
            }
        }
    }
    else
    {
        opponentHand = receiveOpponentHand(clientSock);
    }
    playerHand = isDealer ? opponentHand : playerHand;
    dealerHand = isDealer ? dealerHand : opponentHand;

    // 判定結果
    int playerScore = calculateScore(playerHand);
    int dealerScore = calculateScore(dealerHand);

    cout << "Final Dealer's hand: ";
    displayHand(dealerHand, false, true);
    cout << "Final Player's hand: ";
    displayHand(playerHand, false, false);
    if (playerScore > 21)
    {
        if (isDealer)
            cout << "You win!\n";
        else
            cout << "You lose!\n";
    }
    else if (dealerScore > 21)
    {
        if (isDealer)
            cout << "You lose!\n";
        else
            cout << "You win!\n";
    }
    else if (playerScore > dealerScore)
    {
        if (isDealer)
            cout << "You lose!\n";
        else
            cout << "You win!\n";
    }
    else if (playerScore < dealerScore)
    {
        if (isDealer)
            cout << "You win!\n";
        else
            cout << "You lose!\n";
    }
    else
    {
        cout << "It's a tie!\n";
    }
}

// initiator: ./executableName 18080 1 0.0.0.0
// not-initiator: ./executableName 18080 0 <initiatorIP>
int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        cerr << "Usage: " << argv[0] << " <port> <isDealer> <initiatorIP>" << endl;
        return 1;
    }

    // Parse command-line arguments
    int port = stoi(argv[1]); // 使用指定的埠口
    bool isDealer = stoi(argv[2]) == 1;
    string initiatorIP = argv[3]; // 接收發起者的 IP 地址

    cout << "@@" << port << " " << isDealer << " " << initiatorIP << endl;

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

    if (isDealer)
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
        playBlackjack(clientSock, true);
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
        playBlackjack(sock, false);
    }

    close(sock);
    return 0;
}