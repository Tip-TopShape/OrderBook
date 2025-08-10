#include "../include/FeedHandler.h"
#include <ostream>
#include <iostream>

using namespace std;

void addOrdersTest()
{
    FeedHandler feed;
    auto s1 = "A,1,B,999,996.0";
    auto s2 = "A,2,S,183,1176.0";
    feed.processMessage(s1);
    feed.processMessage(s2);
    feed.printCurrentOrderBook(std::cout);

    //  1          816        996        0          0          0
    //  1          999        996        2          183        1176
}

int main()
{
    addOrdersTest();
    return 0;
}
