/**
 * @file word_grouper.cpp
 * @brief Implementation of recognizer-token to word grouping
 */

#include "utils/word_grouper.h"

namespace ffvoice {

std::vector<Word> GroupTokensIntoWords(const std::vector<WordToken>& tokens) {
    std::vector<Word> words;

    bool have_word = false;
    Word current;
    double probability_sum = 0.0;
    int token_count = 0;

    // Finalize the word currently being built and append it to the result.
    auto flush = [&]() {
        if (!have_word) {
            return;
        }
        current.probability =
            token_count > 0 ? static_cast<float>(probability_sum / token_count) : 0.0f;
        words.push_back(current);
        have_word = false;
    };

    for (const auto& token : tokens) {
        if (token.text.empty()) {
            continue;  // ignore empty tokens
        }

        // Whisper emits BPE sub-word tokens; a token whose text begins with a
        // space marks the start of a new word.
        if (token.text.front() == ' ') {
            flush();
        }

        if (!have_word) {
            current = Word();
            current.start_ms = token.start_ms;
            have_word = true;
            probability_sum = 0.0;
            token_count = 0;
        }

        current.text += token.text;
        current.end_ms = token.end_ms;
        probability_sum += token.probability;
        ++token_count;
    }
    flush();

    return words;
}

}  // namespace ffvoice
