#include <iostream>
#include <string>
#include <ios>

#include "../include/FeedHandler.h"

/*
Order: action,orderid,side,quantity,price (e.g., A,123,B,9,1000)
action = A (add), X (remove), M (modify)
orderid = unique positive integer to identify each order;
used to reference existing orders for remove/modify
side = B (buy), S (sell)
quantity = positive integer indicating maximum quantity to buy/sell
price = double indicating max price at which to buy/min price to sell

Trade: action,quantity,price (e.g., T,2,1025)
action = T (trade)
quantity = amount that traded s
price = price at which the trade happened
*/

int main(int argc, char **argv)
{
    FeedHandler feed;
    std::string line;
    const std::string filename(argv[1]);
    const std::string output(argv[2]);
    std::ios::sync_with_stdio(false);
    std::ifstream infile(filename.c_str(), std::ios::in);
    std::ofstream outfile(output.c_str(), std::ios::out);
    int counter = 0;
    while (std::getline(infile, line))
    {
        feed.processMessage(line);
        if (++counter % 10 == 0)
        {
            feed.printCurrentOrderBook(outfile);
        }
    }
    return 0;
}