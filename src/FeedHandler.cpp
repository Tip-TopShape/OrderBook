#include "../include/FeedHandler.h"
#include <vector>
#include <iostream>

/*
Order: action,orderid,side,quantity,price (e.g., A,123,B,9,1000)
action = A (add), X (remove), M (modify)
orderid = unique positive integer to identify each order;
used to reference existing orders for remove/modify
side = B (bid), S (sell)
quantity = positive integer indicating maximum quantity to bid/sell
price = double indicating max price at which to bid/min price to sell

Trade: action,quantity,price (e.g., T,2,1025)
action = T (trade)a
\

\
quantity = amount that traded s
price = price at which the trade happened
*/

// helper: produce order pointer
std::unique_ptr<FeedHandler::Order> produceOrder(FeedHandler::parsedOrder order) {
    auto pOrder = std::make_unique<FeedHandler::Order>(
        FeedHandler::Order(
            stoi(order[1]),
            stoi(order[3]),
            stof(order[4])));

    return pOrder;
}

bool FeedHandler:: match(std::unique_ptr<Order> &nOrder, const char &type) {
    // find orders between [0, order._price]
    // take the lower bound
    // pick the first in queue
    // continue until order is satisfied
    // print order
    auto orderComplete = false;
    auto originalQty = nOrder->_qty;
    auto price = nOrder->_price;
    auto afterQty = price;
    switch (type) {
        case 'B': {
            if (Bids.empty())
                return false;
            auto it = Sells.begin(); // cheapest available
            // while price is less than or equal to new orders bid
            while (it != Sells.end() && it->first <= nOrder->_price) {
                // less -> reduce existing order q -> make a trade between nOrder and order[i][j]
                // equal -> just pop existing order -> same thing -> break
                // greater -> pop existing order -> continue (increment it)

                while (!it->second.empty()) {
                    auto& currOrder = (it->second.front());

                    // current order is sell order
                    // nOrder is a bid order
                    // the bid qty will go down
                    auto originalQtyCurrOrder =  currOrder->_qty;
                    currOrder->setQty( currOrder->_qty - nOrder->_qty );
                    nOrder->setQty( nOrder->_qty - currOrder->_qty );

                    // if order iterator is complete
                    if (currOrder->_qty == 0) {
                        // remove order
                        // what if sell order traded before?
                        logOrder("S", currOrder->_originalQty, currOrder->_price);
                        currOrder.reset();
                        it->second.pop_front(); // remove it fro
                        // TODO: log the trade
                    }

                    // if new order is complete
                    if (nOrder->_qty == 0) {
                        orderComplete = true;
                        nOrder.reset();
                        logOrder("B", nOrder->_originalQty, nOrder->_price );
                        // TODO: log the trade
                        break;
                    }

                }

                // if we no longer have orders at this price
                if (it->second.empty()) {
                    Bids.erase(it);
                }

                if (orderComplete) {
                    // logOrder(std::to_string(type), nOrder->_qty, nOrder->_price);
                    return true;
                }

                ++it;
            }
            break;
        }

        case 'S': {
            if (Bids.empty())
                return false;
            auto it = Bids.begin(); // cheapest available
            // while price is less than or equal to new orders bid
            while (it != Bids.end() && it->first <= nOrder->_price) {
                // less -> reduce existing order q -> make a trade between nOrder and order[i][j]
                // equal -> just pop existing order -> same thing -> break
                // greater -> pop existing order -> continue (increment it)

                while (!it->second.empty()) {
                    auto& currOrder = (it->second.front());
                    // update current
                    currOrder->setQty( currOrder->_qty - nOrder->_qty );
                    nOrder->setQty( nOrder->_qty - currOrder->_qty );

                    // if order iterator is complete
                    if (currOrder->_qty == 0) {
                        // remove order
                        logOrder("B", currOrder->_originalQty, currOrder->_price);
                        currOrder.reset();
                        it->second.pop_front();
                        // TODO: log the trade
                    }

                    // if new order is complete
                    if (nOrder->_qty == 0) {
                        orderComplete = true;
                        nOrder.reset();
                        logOrder("S", nOrder->_originalQty, nOrder->_price );
                        // TODO: log the trade
                        break;
                    }

                }
                // if we no longer have orders at this price
                if (it->second.empty()) {
                    Sells.erase(it);
                }
                if (orderComplete) {
                    // logOrder(std::to_string(type), nOrder->_qty, nOrder->_qty);
                    return true;
                }


                ++it;
            }
            break;
        }

    }

return orderComplete;
}

void FeedHandler::addOrder(std::unique_ptr<Order> &order, char &type) {
    // no imediate match, add to queue

    if (type == 'B') {
        if (Bids.find(order->_qty) == Bids.end()) {
            pOrders q;
            q.emplace_back(std::move(order));
            Bids[order->_qty] = std::move(q);
        } else Bids[order->_qty].push_back(std::move(order));
    }
    else if (type == 'S') {
        if (Sells.find(order->_qty) == Sells.end()) {
            pOrders q;
            q.emplace_back(std::move(order));
            Sells[order->_qty] = std::move(q);
        }
        else Sells[order->_qty].push_back(std::move(order));
    }
}

void FeedHandler::processMessage(const std::string &line) {
    if (line.empty())
        return;

    // process line
    parsedOrder order = parseMessage(line);

    char action = order[0][0]; // add, bid, remove

    if (action != 'A' && action != 'B' && action != 'M' && action != 'X') {
        return;
    }
    const auto type = order[2][0]; // bid or sell
    auto pOrder = produceOrder(order);
    auto orderPrice = pOrder->_price;
    auto qty = pOrder->_qty;

    // issue : should only match if sell or bid
    // if modify -> it's already in the queue so don't try to rematch
    // if remove -> immediately destroy order

    if (type == 'A') {
        // begin matching
        bool isMatched = match(pOrder, type);
        if (!isMatched) {
            // TODO: what if it not fulfilled?
            handleAction(action, pOrder, type);
        }
    } else {
        handleAction(action, pOrder, type);
    }

    logOrder(std::to_string(type), qty, orderPrice);
}

void FeedHandler::handleAction(char &action, std::unique_ptr<Order> &pOrder, const char &type) {
    auto orderPrice = pOrder->_price;
    switch (action) {
        case 'A':
            // new order (sell or bid)
            // attempt to match
            // otherwise save to be matched

            // std::cout << "Adding to order...\n";
            if (type == 'B') {
                if (Bids.find(orderPrice) == Bids.end()) {
                    pOrders q;  // no queue for price -> create one and add
                    q.emplace_back(std::move(pOrder));
                    Bids[orderPrice] = std::move(q); // move it into map since we don't need it
                } else {
                    Bids[orderPrice].emplace_back(std::move(pOrder));
                }
            }
            else if (type == 'S') {
                if (Sells.find(orderPrice) == Sells.end()) {
                    pOrders q;  // no queue for price -> create one and add
                    q.emplace_back(std::move(pOrder));
                    Sells[orderPrice] = std::move(q); // move it into map since we don't need it
                } else {
                    Sells[orderPrice].emplace_back(std::move(pOrder));
                }
            }

            break;
        case 'R':
            // std::cout << "Removing order...\n";
            // iterate over bids
            for (auto it = Bids[pOrder->_price].begin(); it != Bids[pOrder->_price].end(); ++it) {
                if ((*it)->_orderId == pOrder->_orderId) {
                    // destroy order
                    (*it).reset();
                    Bids[pOrder->_price].erase(it);
                    break;
                }
            }
            break;

            // O(|bids|+|Sells|)
            // do prefetch?

        case 'M':
            // std::cout << "Modifying order...\n";
            break;

        default:
            break;
            // std::cout << "Unknown request...\n";
    }
}

void FeedHandler::printCurrentOrderBook(std::ostream &os) const  {
    auto bidPrice = Bids.begin();
    auto sellPrice = Sells.begin();
    // VariadicTable<int, int, int32_t> vtBids({"order Id", "quantity", "price"});
    // VariadicTable<int, int, int32_t> vtSells({"order Id", "quantity", "price"});
    os << "|order Id"; // 8 pad
    os << "  |";
    os << "quantity"; // 8 pad
    os << "  |";
    os <<  "price"; // 5 pad
    os << "     |";
    os <<  "order Id"; // 8 pad
    os << "  |";
    os <<  "quantity"; // 8 pad
    os << "  |";
    os <<  "price     |\n"; // 5 pad

    std::vector<int> pad = {8, 8, 5, 8, 8, 5};

    // continue while there's bids or sells
    while (sellPrice != Sells.end() || bidPrice != Bids.end())
    {
        std::vector<int> vec;
        // os << "\n";

        bool flagA = ( bidPrice != Bids.end() );
        bool flagB = ( sellPrice != Sells.end() );

        std::deque<std::unique_ptr<Order>>::const_iterator bidOrder;
        std::deque<std::unique_ptr<Order>>::const_iterator sellOrder;
        // auto sellOrder = sellPrice->second.begin();

        if (!bidPrice->second.empty()) {
            bidOrder = bidPrice->second.begin();
        } else {
            bidOrder = bidPrice->second.end();
        }

        if (!sellPrice->second.empty()) {
            sellOrder = sellPrice->second.begin();
        } else {
            sellOrder = sellPrice->second.end();
        }

        // same as while statement
        if (flagA || flagB) {
            // TODO: can't grab the .end() of an empty deque

            while (flagA || flagB) {
                // os << (*it)->_orderId << " ";
                // os << (*it)->_qty << " ";
                // os << (*it)->_price << " \n";
                bool const flagC = ( !bidPrice->second.empty() );
                bool const flagD = ( !sellPrice->second.empty() );

                if (flagA && flagC) {
                    vec.emplace_back((*bidOrder)->_orderId);
                    vec.emplace_back((*bidOrder)->_qty);
                    vec.emplace_back((*bidOrder)->_price);
                    if (bidOrder != bidPrice->second.end())
                        ++bidOrder;
                }
                else {
                    vec.emplace_back(0);
                    vec.emplace_back(0);
                    vec.emplace_back(0);
                }
    
                if (flagB && flagD) {
                    vec.emplace_back((*sellOrder)->_orderId);
                    vec.emplace_back((*sellOrder)->_qty);
                    vec.emplace_back((*sellOrder)->_price);
                    if (sellOrder != sellPrice->second.end())
                        ++sellOrder;
                }
                else {
                    vec.emplace_back(0);
                    vec.emplace_back(0);
                    vec.emplace_back(0);
                }
                int width = 10;
                for (auto i = 0; i < 6; i++) {
                    os << " " << vec[i];
                    int r = width - std::to_string(vec[i]).size();
                    int p = std::max(0, r);
                    os << std::string(p, ' ');
                }
                os << "\n";

                flagA = ( bidPrice != Bids.end() );
                flagB = ( sellPrice != Sells.end() );
            }
        }
    }
}

std::string stripWhitespace(std::string str) {
    str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char c) { return std::isspace(c); }),
              str.end());
    return str;
}

FeedHandler::parsedOrder FeedHandler::parseMessage(const std::string &line) {
    size_t start = 0;
    FeedHandler::parsedOrder order;

    // std::cout << "length: " << line.length() << "\n";

    while (start < line.length()) {
        std::string token = line.substr(start, line.find(',', start) - start);
        start = start + token.length() + 1;
        order.push_back(stripWhitespace(token));
    }

    return order;
}

