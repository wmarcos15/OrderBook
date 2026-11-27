#pragma once
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace orderbook::detail {

// One bit per position, with a summary to skip empty words
class Bitmap {
private:
    static constexpr std::size_t WORD_BITS = 64;

    std::size_t positions_;
    std::vector<std::uint64_t> words_;
    // One bit per word of words_, on if and only if that word is not 0
    std::vector<std::uint64_t> summary_;

    static std::size_t wordsFor(std::size_t bits) {
        return (bits + WORD_BITS - 1) / WORD_BITS;
    }

    static std::size_t wordOf(std::size_t position) {
        return position / WORD_BITS;
    }

    static std::size_t bitOf(std::size_t position) {
        return position % WORD_BITS;
    }

    static std::uint64_t maskOf(std::size_t position) {
        return std::uint64_t {1} << bitOf(position);
    }

    static std::size_t nextIn(const std::vector<std::uint64_t>& words, std::size_t position) {
        std::size_t index = wordOf(position);
        if (index >= words.size()) return NONE;

        std::uint64_t word = words[index] & (~std::uint64_t {0} << bitOf(position));

        while (word == 0) {
            ++index;
            if (index == words.size()) return NONE;
            word = words[index];
        }

        return index * WORD_BITS + static_cast<std::size_t>(std::countr_zero(word));
    }

    static std::size_t previousIn(const std::vector<std::uint64_t>& words, std::size_t position) {
        std::size_t index = wordOf(position);
        assert(index < words.size());

        std::uint64_t word = words[index] & (~std::uint64_t {0} >> (WORD_BITS - 1 - bitOf(position)));

        while (word == 0) {
            // Checked before the decrement: index is unsigned and would wrap around
            if (index == 0) return NONE;
            --index;
            word = words[index];
        }

        return index * WORD_BITS + (WORD_BITS - 1 - static_cast<std::size_t>(std::countl_zero(word)));
    }

    void rebuildSummary() noexcept {
        std::fill(summary_.begin(), summary_.end(), 0);
        for (std::size_t word {0}; word < words_.size(); ++word) {
            if (words_[word] != 0) summary_[wordOf(word)] |= maskOf(word);
        }
    }

public:
    static constexpr std::size_t NONE = std::numeric_limits<std::size_t>::max();

    explicit Bitmap(std::size_t positions)
        : positions_ {positions}, words_(wordsFor(positions)), summary_(wordsFor(words_.size())) {}

    void set(std::size_t position) {
        assert(wordOf(position) < words_.size());
        std::size_t word = wordOf(position);
        words_[word] |= maskOf(position);
        summary_[wordOf(word)] |= maskOf(word);
    }

    void clear(std::size_t position) {
        assert(wordOf(position) < words_.size());
        std::size_t word = wordOf(position);
        words_[word] &= ~maskOf(position);

        if (words_[word] == 0)
            summary_[wordOf(word)] &= ~maskOf(word);
    }

    void dropFront(std::size_t n) noexcept {
        if (n == 0) return;
        // A moved-from bitmap keeps its size but has no words
        if (n >= positions_ || words_.empty()) {
            std::fill(words_.begin(), words_.end(), 0);
            std::fill(summary_.begin(), summary_.end(), 0);
            return;
        }

        std::size_t wordShift = n / WORD_BITS;
        std::size_t bitShift = n % WORD_BITS;
        std::size_t count = words_.size();

        // Forwards: every word is read before it is written
        for (std::size_t word {0}; word < count; ++word) {
            std::size_t source = word + wordShift;
            std::uint64_t low = (source < count) ? words_[source] : 0;
            std::uint64_t high = (source + 1 < count) ? words_[source + 1] : 0;
            // A shift by 64 would be undefined
            words_[word] = (bitShift == 0) ? low : (low >> bitShift) | (high << (WORD_BITS - bitShift));
        }

        rebuildSummary();
    }

    void dropBack(std::size_t n) noexcept {
        if (n == 0) return;
        // A moved-from bitmap keeps its size but has no words
        if (n >= positions_ || words_.empty()) {
            std::fill(words_.begin(), words_.end(), 0);
            std::fill(summary_.begin(), summary_.end(), 0);
            return;
        }

        std::size_t wordShift = n / WORD_BITS;
        std::size_t bitShift = n % WORD_BITS;

        // Backwards: every word is read before it is written
        for (std::size_t word = words_.size(); word-- > 0;) {
            if (word < wordShift) {
                words_[word] = 0;
                continue;
            }
            std::size_t source = word - wordShift;
            std::uint64_t high = words_[source];
            std::uint64_t low = (source > 0) ? words_[source - 1] : 0;
            words_[word] = (bitShift == 0) ? high : (high << bitShift) | (low >> (WORD_BITS - bitShift));
        }

        // Bits pushed past the last position would be found by next
        if (bitOf(positions_) != 0)
            words_.back() &= maskOf(positions_) - 1;

        rebuildSummary();
    }

    std::size_t lowest() const noexcept {
        return next(0);
    }

    std::size_t highest() const noexcept {
        if (words_.empty()) return NONE;
        return previous(words_.size() * WORD_BITS - 1);
    }

    bool test(std::size_t position) const {
        assert(wordOf(position) < words_.size());
        return (words_[wordOf(position)] & maskOf(position)) != 0;
    }

    std::size_t next(std::size_t position) const {
        std::size_t index = wordOf(position);
        if (index >= words_.size()) return NONE;

        std::uint64_t word = words_[index] & (~std::uint64_t {0} << bitOf(position));

        if (word == 0) {
            index = nextIn(summary_, index + 1);
            if (index == NONE) return NONE;
            word = words_[index];
            assert(word != 0);
        }

        return index * WORD_BITS + static_cast<std::size_t>(std::countr_zero(word));
    }

    std::size_t previous(std::size_t position) const {
        assert(wordOf(position) < words_.size());
        std::size_t index = wordOf(position);

        std::uint64_t word = words_[index] & (~std::uint64_t {0} >> (WORD_BITS - 1 - bitOf(position)));

        if (word == 0) {
            // Checked before the subtraction: index is unsigned and would wrap around
            if (index == 0) return NONE;
            index = previousIn(summary_, index - 1);
            if (index == NONE) return NONE;
            word = words_[index];
            assert(word != 0);
        }

        return index * WORD_BITS + (WORD_BITS - 1 - static_cast<std::size_t>(std::countl_zero(word)));
    }
};

} // namespace orderbook::detail
