/**
 * @file word_grouper.h
 * @brief Group recognizer sub-word tokens into whole words
 */

#pragma once

#include "audio/whisper_processor.h"  // for ffvoice::Word

#include <cstdint>
#include <string>
#include <vector>

namespace ffvoice {

/**
 * @brief A raw recognizer token with timing — the input to word grouping.
 */
struct WordToken {
    std::string text;   ///< Token text as emitted by the recognizer
    int64_t start_ms;   ///< Token start time in milliseconds
    int64_t end_ms;     ///< Token end time in milliseconds
    float probability;  ///< Token probability (0.0-1.0)
};

/**
 * @brief Group sub-word recognizer tokens into whole words.
 *
 * Whisper emits BPE sub-word tokens; a new word begins at a token whose text
 * starts with a space, and tokens without a leading space continue the current
 * word. A grouped Word spans from its first token's start to its last token's
 * end, its text is the concatenation of its tokens' text, and its probability
 * is the mean of its tokens' probabilities. Empty-text tokens are ignored.
 *
 * This is pure logic with no Whisper dependency, so it can be unit-tested
 * directly with synthetic token input.
 *
 * @param tokens Recognizer tokens in chronological order.
 * @return Grouped words in chronological order.
 */
std::vector<Word> GroupTokensIntoWords(const std::vector<WordToken>& tokens);

}  // namespace ffvoice
