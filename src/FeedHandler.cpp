#include "../include/FeedHandler.h"
#include <algorithm>

void FeedHandler::processOrder(uint64_t &id, databento::Side &side,
                               uint32_t instrument_id,
                               databento::Action &action, int64_t &price,
                               uint32_t &qty, databento::UnixNanos &ts_recv,
                               databento::TimeDeltaNanos &ts_event) {
  auto newOrder = Order(ts_recv, ts_event, side, qty, price);
  newOrder.id = id;
  bool priceChange = false;

  ++orderCount_;
  bool sampleLatency = orderCount_ % LATENCY_SAMPLE_RATE == 0;
  uint64_t t0 = 0;
  if (sampleLatency) {
    t0 = now_ns();
  }

  Metrics::ActionType actionType;
  bool validAction = true;
  switch (action) {
  case databento::Action::Add:
    this->addOrder(newOrder, side, price, instrument_id);
    ++metrics_.adds;
    actionType = Metrics::ActionType::Add;
    break;
  case databento::Action::Modify:
    priceChange = this->modifyOrder(id, price, qty, instrument_id);
    ++metrics_.modifies;
    if (priceChange)
      this->addOrder(newOrder, side, price, instrument_id);
    actionType = Metrics::ActionType::Modify;
    break;
  case databento::Action::Cancel:
    this->cancelOrder(id, instrument_id);
    ++metrics_.cancels;
    actionType = Metrics::ActionType::Cancel;
    break;
  case databento::Action::Trade:
    this->trade(id, side, price, qty, instrument_id);
    ++metrics_.trades;
    actionType = Metrics::ActionType::Trade;
    break;
  case databento::Action::Fill:
    this->trade(id, side, price, qty, instrument_id);
    ++metrics_.fills;
    actionType = Metrics::ActionType::Fill;
    break;
  default:
    validAction = false;
    break;
  }
  if (sampleLatency && validAction) {
    uint64_t t1 = now_ns();
    metrics_.recordLatency(actionType, t1 - t0);
  }
  metrics_.recordOrder();
}

bool FeedHandler::modifyOrder(uint64_t order_id, int64_t new_price,
                              uint32_t new_qty, uint32_t instrument_id) {
  if (!id_to_index.contains(order_id))
    return false;
  auto idx = id_to_index[order_id];
  auto &order = order_pool[idx];
  SymbolBook *book = findBook(instrument_id);
  if (!book)
    return false;
  auto &arr = (order.side == databento::Side::Bid) ? book->bids : book->asks;

  if (new_price == order.price) {
    order.qty = new_qty;
    return false;
  } else {
    uint32_t lvl_idx = arr.find(order.price);
    if (lvl_idx == UINT32_MAX)
      return false;
    priceLevel &level = price_pool[lvl_idx];
    popOrder(level, idx);
    if (level.isEmpty()) {
      arr.deactivate(order.price);
      price_pool.free(lvl_idx);
      arr.decrementSize();
    }
    return true;
  }
}

void FeedHandler::trade(uint64_t id, databento::Side &side, int64_t &price,
                        uint32_t qty, uint32_t instrument_id) {
  if (!id_to_index.contains(id))
    return;
  auto idx = id_to_index[id];
  auto &order = order_pool[idx];
  SymbolBook *book = findBook(instrument_id);
  if (!book)
    return;
  auto &arr = (order.side == databento::Side::Bid) ? book->bids : book->asks;

  uint32_t lvl_idx = arr.find(order.price);
  if (lvl_idx == UINT32_MAX)
    return;
  priceLevel &level = price_pool[lvl_idx];

  if (order.qty <= qty) {
    order.qty = 0;
  } else {
    order.qty -= qty;
  }

  if (order.qty == 0) {
    popOrder(level, idx);
    if (level.isEmpty()) {
      arr.deactivate(order.price);
      price_pool.free(lvl_idx);
      arr.decrementSize();
    }
  }
}

void FeedHandler::addOrder(Order &entry, databento::Side &side, int64_t &price,
                           uint32_t instrument_id) {
  SymbolBook *book = findBook(instrument_id);
  if (!book)
    return;
  auto &arr = (side == databento::Side::Bid) ? book->bids : book->asks;
  if (!arr.ticks)
    return;

  uint32_t lvl_idx = arr.find(price);
  if (lvl_idx == UINT32_MAX) {
    lvl_idx = price_pool.allocate();
    if (lvl_idx == UINT32_MAX)
      return;
    price_pool[lvl_idx] = priceLevel{};
    arr.activate(price, lvl_idx);
    arr.incrementSize();
  }
  this->push(price_pool[lvl_idx], entry);
}

void FeedHandler::cancelOrder(const uint64_t &id, uint32_t instrument_id) {
  if (!id_to_index.contains(id))
    return;
  uint32_t idx = id_to_index[id];
  auto &order = order_pool[idx];

  SymbolBook *book = findBook(instrument_id);
  if (!book)
    return;
  auto &arr = (order.side == databento::Side::Bid) ? book->bids : book->asks;

  uint32_t lvl_idx = arr.find(order.price);
  if (lvl_idx == UINT32_MAX)
    return;
  priceLevel &level = price_pool[lvl_idx];

  popOrder(level, idx);
  id_to_index.erase(id);

  if (level.isEmpty()) {
    arr.deactivate(order.price);
    price_pool.free(lvl_idx);
    arr.decrementSize();
  }
}

size_t FeedHandler::calculateMemoryUsed() const {
  size_t used = sizeof(books) + (MAX_SIZE * sizeof(Order));
  for (const auto &book : books) {
    used += sizeof(SymbolBook);
    used += book.bids.capacity * sizeof(TickArray<int64_t>::Slot);
    used += book.asks.capacity * sizeof(TickArray<int64_t>::Slot);
  }
  return used;
}
