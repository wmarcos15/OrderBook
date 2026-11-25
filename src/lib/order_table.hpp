#pragma once
#include "order_queue.hpp"
#include "types.hpp"

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace orderbook::detail {

// Finds an order by its ID without hashing: the ID says the page and the slot.
// The table owns the pages but not the orders, which live in the pool.
template<std::size_t PageSize = 4096>
class OrderTable {
    static_assert(std::has_single_bit(PageSize), "PageSize must be a power of two");

private:
    using Page = std::unique_ptr<OrderNode*[]>;

    struct Entry {
        Page page; // nullptr if no page
        std::uint32_t live {0};
    };

    std::vector<Entry> directory_;
    std::vector<Page> freePages_;

    static std::size_t indexOf(OrderID id) {
        return id / PageSize;
    }

    static std::size_t slotOf(OrderID id) {
        return id % PageSize;
    }

public:
    OrderTable() = default;
    OrderTable(const OrderTable&) = delete;
    OrderTable& operator=(const OrderTable&) = delete;
    OrderTable(OrderTable&&) = default;
    OrderTable& operator=(OrderTable&&) = default;

    // Returns null for any ID that is not in the table
    OrderNode* find(OrderID id) const {
        if (indexOf(id) >= directory_.size()) return nullptr;
        const Entry& entry = directory_[indexOf(id)];
        const Page& page = entry.page;
        if (page == nullptr) return nullptr;
        return page[slotOf(id)];
    }

    void insert(OrderID id, OrderNode* node) {
        assert(node != nullptr);
        assert(node->order.getID() == id);

        if (indexOf(id) >= directory_.size())
            directory_.resize(indexOf(id) + 1);

        Entry& entry = directory_[indexOf(id)];

        if (entry.page == nullptr) {
            if (!freePages_.empty()) {
                entry.page = std::move(freePages_.back());
                freePages_.pop_back();
            } else {
                // Room for every page in freePages_, so that erase never has to allocate
                freePages_.reserve(directory_.size());
                entry.page = std::make_unique<OrderNode*[]>(PageSize);
            }
        }

        assert(entry.page[slotOf(id)] == nullptr);
        entry.page[slotOf(id)] = node;
        ++entry.live;
    }

    void erase(OrderID id) {
        assert(find(id) != nullptr);

        Entry& entry = directory_[indexOf(id)];
        entry.page[slotOf(id)] = nullptr;
        --entry.live;

        if (entry.live == 0)
            freePages_.push_back(std::move(entry.page));
    }

    // Pages in use plus spare ones.
    std::size_t pageCount() const {
        std::size_t size = freePages_.size();
        for (const auto& entry : directory_) {
            if (entry.page != nullptr) ++size;
        }
        return size;
    }
};

} // namespace orderbook::detail
