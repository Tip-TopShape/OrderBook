#include <fstream>
#include <iostream>
#include <algorithm>
#include <memory>
#include <map>
#include <queue>

/*
Order: action,orderid,side,quantity,price (e.g., A,123,B,9,1000)
action = A (add), X (remove), M (modify)
orderid = unique positive integer to identify each order;`
used to reference existing orders for remove/modify
side = B (bid), S (sell)
quantity = positive integer indicating maximum quantity to bid/sell
price = double indicating max price at which to bid/min price to sell

Trade: action,quantity,price (e.g., T,2,1025)
action = T (trade)
quantity = amount that traded s
price = price at which the trade happened
*/

enum class Action : int
{
    A = 'A', // add
    R = 'R', // remove
    M = 'M'  // modify
};

class FeedHandler
{
public:
    struct Order
    {

        int _orderId;
        int _qty;
        int32_t _price;
        int32_t _originalQty;

        Order(const int oId, const int qty, const int32_t price)
            : _orderId(oId), _qty(qty), _price(price), _originalQty(qty) {};


        void setQty(const int qty) { _qty = std::max(0, qty); }
    };

    using parsedOrder = std::vector<std::string>;
    using pOrders = std::deque<std::unique_ptr<Order>>;


    void processMessage(const std::string &line);
    void printCurrentOrderBook(std::ostream &os) const;
    bool match(std::unique_ptr<Order> &order, const char &type);
    void addOrder(std::unique_ptr<Order> &order, char &type);
    void handleAction(char &action, std::unique_ptr<Order> &pOrder, const char &type);
    void incrementMsgNumber () {
        msgNumber++;
        if (msgNumber == 10) {
            printCurrentOrderBook(std::cout);
            msgNumber = 0;
        }
    }
    static void logOrder(const std::string &type, const int qty, const int32_t price) {
        const std::string filename("trades.txt");
        std::ofstream outfile(filename.c_str(), std::ios::out);
        outfile << "Order: " << type << " " + std::to_string(qty) << "@" << std::to_string(price) << "\n";
    }


    parsedOrder parseMessage(const std::string &line);

    // insert price key in descending order
    struct BidComp {
        bool operator()(const int32_t &A, const int32_t &B) const {
            return A > B;
        };
    };

    // insert price key in ascending order
    struct SellComp {
        bool operator()(const int32_t &A, const int32_t &B) const {
            return A < B;
        };
    };

private:
    std::map<int32_t, pOrders, BidComp> Bids;
    std::map<int32_t, pOrders, SellComp> Sells;

    int msgNumber = 0;
};

