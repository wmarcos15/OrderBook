#pragma once
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace orderbook::detail {

// One bit per position. Finds the next 1 by looking at 64 positions at a time,
// and a summary of which words are not empty lets it skip the empty ones.
class Bitmap {
private:
    static constexpr std::size_t WORD_BITS = 64;

    std::vector<std::uint64_t> words_;
    // One bit per word of words_, on if and only if that word is not 0
    std::vector<std::uint64_t> summary_;

    // How many words hold that many bits
    static std::size_t wordsFor(std::size_t bits) {
        return (bits + WORD_BITS - 1) / WORD_BITS;
    }

    // Which word holds the position
    static std::size_t wordOf(std::size_t position) {
        return position / WORD_BITS;
    }

    // Which bit of its word holds the position
    static std::size_t bitOf(std::size_t position) {
        return position % WORD_BITS;
    }

    // A word with a single 1, at the position's bit
    static std::uint64_t maskOf(std::size_t position) {
        return std::uint64_t {1} << bitOf(position);
    }

    // The lowest 1 at `position` or above it in any vector of words, NONE if there is none
    static std::size_t nextIn(const std::vector<std::uint64_t>& words, std::size_t position) {
        std::size_t index = wordOf(position);
        if (index >= words.size()) return NONE;

        // Only the first word has bits below position to ignore, the later ones are whole
        std::uint64_t word = words[index] & (~std::uint64_t {0} << bitOf(position));

        while (word == 0) {
            ++index;
            if (index == words.size()) return NONE;
            word = words[index];
        }

        return index * WORD_BITS + static_cast<std::size_t>(std::countr_zero(word));
    }

    // The highest 1 at `position` or below it in any vector of words, NONE if there is none
    static std::size_t previousIn(const std::vector<std::uint64_t>& words, std::size_t position) {
        std::size_t index = wordOf(position);
        assert(index < words.size());

        // Only the first word has bits above position to ignore, the earlier ones are whole
        std::uint64_t word = words[index] & (~std::uint64_t {0} >> (WORD_BITS - 1 - bitOf(position)));

        while (word == 0) {
            // Checked before the decrement: index is unsigned and would wrap around
            if (index == 0) return NONE;
            --index;
            word = words[index];
        }

        return index * WORD_BITS + (WORD_BITS - 1 - static_cast<std::size_t>(std::countl_zero(word)));
    }

public:
    // Returned when there is no 1 to find
    static constexpr std::size_t NONE = std::numeric_limits<std::size_t>::max();

    // All the bits start at 0
    explicit Bitmap(std::size_t positions) : words_(wordsFor(positions)), summary_(wordsFor(words_.size())) {}

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

        // The word stays marked while it has other bits on
        if (words_[word] == 0)
            summary_[wordOf(word)] &= ~maskOf(word);
    }

    bool test(std::size_t position) const {
        assert(wordOf(position) < words_.size());
        return (words_[wordOf(position)] & maskOf(position)) != 0;
    }

    // The lowest 1 at `position` or above it, NONE if there is none
    std::size_t next(std::size_t position) const {
        std::size_t index = wordOf(position);
        if (index >= words_.size()) return NONE;

        // The answer is usually in the same word as position
        std::uint64_t word = words_[index] & (~std::uint64_t {0} << bitOf(position));

        if (word == 0) {
            // Jump to the next word that is not empty
            index = nextIn(summary_, index + 1);
            if (index == NONE) return NONE;
            word = words_[index];
            assert(word != 0);
        }

        return index * WORD_BITS + static_cast<std::size_t>(std::countr_zero(word));
    }

    // The highest 1 at `position` or below it, NONE if there is none
    std::size_t previous(std::size_t position) const {
        assert(wordOf(position) < words_.size());
        std::size_t index = wordOf(position);

        // The answer is usually in the same word as position
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
