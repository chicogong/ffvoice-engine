/**
 * @file test_word_grouper.cpp
 * @brief Unit tests for GroupTokensIntoWords (sub-word token to word grouping)
 */

#include "utils/word_grouper.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace ffvoice;

class WordGrouperTest : public ::testing::Test {
protected:
    // Helper: build a WordToken concisely.
    static WordToken Tok(const std::string& text, int64_t start_ms, int64_t end_ms,
                         float probability = 1.0f) {
        WordToken t;
        t.text = text;
        t.start_ms = start_ms;
        t.end_ms = end_ms;
        t.probability = probability;
        return t;
    }
};

// ============================================================================
// Empty input
// ============================================================================

TEST_F(WordGrouperTest, EmptyTokenListProducesEmptyResult) {
    std::vector<WordToken> tokens;
    auto words = GroupTokensIntoWords(tokens);
    EXPECT_TRUE(words.empty());
}

TEST_F(WordGrouperTest, OnlyEmptyTextTokensProduceEmptyResult) {
    std::vector<WordToken> tokens = {
        Tok("", 0, 100, 0.5f),
        Tok("", 100, 200, 0.6f),
    };
    auto words = GroupTokensIntoWords(tokens);
    EXPECT_TRUE(words.empty());
}

// ============================================================================
// Single token
// ============================================================================

TEST_F(WordGrouperTest, SingleTokenProducesOneWord) {
    std::vector<WordToken> tokens = {
        Tok(" Hello", 100, 450, 0.875f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(1u, words.size());
    EXPECT_EQ(" Hello", words[0].text);
    EXPECT_EQ(100, words[0].start_ms);
    EXPECT_EQ(450, words[0].end_ms);
    EXPECT_FLOAT_EQ(0.875f, words[0].probability);
}

TEST_F(WordGrouperTest, SingleTokenWithoutLeadingSpaceStillProducesValidWord) {
    // The very first token has no leading space; it must still start a word.
    std::vector<WordToken> tokens = {
        Tok("Hello", 10, 320, 0.42f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(1u, words.size());
    EXPECT_EQ("Hello", words[0].text);
    EXPECT_EQ(10, words[0].start_ms);
    EXPECT_EQ(320, words[0].end_ms);
    EXPECT_FLOAT_EQ(0.42f, words[0].probability);
}

// ============================================================================
// Continuation tokens (no leading spaces) merge into one word
// ============================================================================

TEST_F(WordGrouperTest, ContinuationTokensMergeIntoSingleWord) {
    // No token has a leading space, so all of them form ONE word.
    std::vector<WordToken> tokens = {
        Tok("un", 0, 100, 1.0f),
        Tok("believ", 100, 250, 1.0f),
        Tok("able", 250, 400, 1.0f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(1u, words.size());
    EXPECT_EQ("unbelievable", words[0].text);
    EXPECT_EQ(0, words[0].start_ms);    // first token's start
    EXPECT_EQ(400, words[0].end_ms);    // last token's end
    EXPECT_FLOAT_EQ(1.0f, words[0].probability);
}

// ============================================================================
// Leading-space tokens split into separate words
// ============================================================================

TEST_F(WordGrouperTest, LeadingSpaceTokensProduceSeparateWords) {
    std::vector<WordToken> tokens = {
        Tok(" Hello", 0, 300, 0.9f),
        Tok(" world", 300, 600, 0.8f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(2u, words.size());

    EXPECT_EQ(" Hello", words[0].text);
    EXPECT_EQ(0, words[0].start_ms);
    EXPECT_EQ(300, words[0].end_ms);
    EXPECT_FLOAT_EQ(0.9f, words[0].probability);

    EXPECT_EQ(" world", words[1].text);
    EXPECT_EQ(300, words[1].start_ms);
    EXPECT_EQ(600, words[1].end_ms);
    EXPECT_FLOAT_EQ(0.8f, words[1].probability);
}

TEST_F(WordGrouperTest, EveryTokenLeadingSpaceProducesOneWordEach) {
    std::vector<WordToken> tokens = {
        Tok(" a", 0, 50, 1.0f),
        Tok(" b", 50, 100, 1.0f),
        Tok(" c", 100, 150, 1.0f),
        Tok(" d", 150, 200, 1.0f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(4u, words.size());
    EXPECT_EQ(" a", words[0].text);
    EXPECT_EQ(" b", words[1].text);
    EXPECT_EQ(" c", words[2].text);
    EXPECT_EQ(" d", words[3].text);
}

// ============================================================================
// Realistic mix of leading-space starts and continuation tokens
// ============================================================================

TEST_F(WordGrouperTest, RealisticMixGroupsWordsAndSpans) {
    // " The" | " quick" + "er" | " fox"
    std::vector<WordToken> tokens = {
        Tok(" The", 0, 200, 0.95f),
        Tok(" quick", 200, 500, 0.90f),
        Tok("er", 500, 650, 0.70f),  // continuation of "quick"
        Tok(" fox", 650, 900, 0.85f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(3u, words.size());

    // Word 0: " The"
    EXPECT_EQ(" The", words[0].text);
    EXPECT_EQ(0, words[0].start_ms);
    EXPECT_EQ(200, words[0].end_ms);
    EXPECT_FLOAT_EQ(0.95f, words[0].probability);

    // Word 1: " quick" + "er" -> " quicker", spanning 200..650
    EXPECT_EQ(" quicker", words[1].text);
    EXPECT_EQ(200, words[1].start_ms);  // start of " quick"
    EXPECT_EQ(650, words[1].end_ms);    // end of "er"
    EXPECT_FLOAT_EQ((0.90f + 0.70f) / 2.0f, words[1].probability);

    // Word 2: " fox"
    EXPECT_EQ(" fox", words[2].text);
    EXPECT_EQ(650, words[2].start_ms);
    EXPECT_EQ(900, words[2].end_ms);
    EXPECT_FLOAT_EQ(0.85f, words[2].probability);
}

TEST_F(WordGrouperTest, FirstTokenNoSpaceThenLeadingSpaceTokens) {
    // First token lacks a leading space (continues a "current" word that does
    // not yet exist) and must still produce a valid first word.
    std::vector<WordToken> tokens = {
        Tok("Hel", 0, 150, 1.0f),
        Tok("lo", 150, 300, 1.0f),
        Tok(" there", 300, 600, 1.0f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(2u, words.size());
    EXPECT_EQ("Hello", words[0].text);
    EXPECT_EQ(0, words[0].start_ms);
    EXPECT_EQ(300, words[0].end_ms);

    EXPECT_EQ(" there", words[1].text);
    EXPECT_EQ(300, words[1].start_ms);
    EXPECT_EQ(600, words[1].end_ms);
}

// ============================================================================
// Probability is the arithmetic mean of a word's token probabilities
// ============================================================================

TEST_F(WordGrouperTest, WordProbabilityIsMeanOfTokenProbabilities) {
    // Single word from three continuation tokens with differing probabilities.
    std::vector<WordToken> tokens = {
        Tok("a", 0, 100, 0.2f),
        Tok("b", 100, 200, 0.5f),
        Tok("c", 200, 300, 0.8f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(1u, words.size());
    EXPECT_FLOAT_EQ((0.2f + 0.5f + 0.8f) / 3.0f, words[0].probability);  // 0.5
}

TEST_F(WordGrouperTest, ProbabilityMeanComputedPerWordIndependently) {
    // Two words, each averaging its own tokens only.
    std::vector<WordToken> tokens = {
        Tok(" foo", 0, 100, 0.4f),
        Tok("bar", 100, 200, 0.6f),   // word 0: mean(0.4, 0.6) = 0.5
        Tok(" baz", 200, 300, 0.10f),
        Tok("qux", 300, 400, 0.30f),
        Tok("ish", 400, 500, 0.50f),  // word 1: mean(0.1, 0.3, 0.5) = 0.3
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(2u, words.size());
    EXPECT_EQ(" foobar", words[0].text);
    EXPECT_NEAR(0.5f, words[0].probability, 1e-6f);
    EXPECT_EQ(" bazquxish", words[1].text);
    EXPECT_NEAR(0.3f, words[1].probability, 1e-6f);
}

// ============================================================================
// Empty-text tokens in the middle are skipped without breaking grouping
// ============================================================================

TEST_F(WordGrouperTest, EmptyTokenInMiddleIsSkipped) {
    // The empty token between "un" and "able" must not split the word, must not
    // affect timing, and must not be counted in the probability mean.
    std::vector<WordToken> tokens = {
        Tok("un", 0, 100, 0.6f),
        Tok("", 100, 100, 0.0f),  // skipped entirely
        Tok("able", 100, 250, 0.8f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(1u, words.size());
    EXPECT_EQ("unable", words[0].text);
    EXPECT_EQ(0, words[0].start_ms);
    EXPECT_EQ(250, words[0].end_ms);
    // Mean of the two NON-empty tokens only: (0.6 + 0.8) / 2 = 0.7
    EXPECT_FLOAT_EQ((0.6f + 0.8f) / 2.0f, words[0].probability);
}

TEST_F(WordGrouperTest, EmptyTokenBetweenWordsDoesNotCreateExtraWord) {
    std::vector<WordToken> tokens = {
        Tok(" Hello", 0, 300, 1.0f),
        Tok("", 300, 300, 0.0f),  // skipped, no flush triggered
        Tok(" world", 300, 600, 1.0f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(2u, words.size());
    EXPECT_EQ(" Hello", words[0].text);
    EXPECT_EQ(" world", words[1].text);
}

TEST_F(WordGrouperTest, LeadingAndTrailingEmptyTokensIgnored) {
    std::vector<WordToken> tokens = {
        Tok("", 0, 0, 0.0f),       // leading empty
        Tok(" word", 100, 400, 0.9f),
        Tok("", 400, 400, 0.0f),   // trailing empty
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(1u, words.size());
    EXPECT_EQ(" word", words[0].text);
    EXPECT_EQ(100, words[0].start_ms);
    EXPECT_EQ(400, words[0].end_ms);
    EXPECT_FLOAT_EQ(0.9f, words[0].probability);
}

// ============================================================================
// Output ordering
// ============================================================================

TEST_F(WordGrouperTest, WordsPreserveChronologicalOrder) {
    std::vector<WordToken> tokens = {
        Tok(" one", 0, 100, 1.0f),
        Tok(" two", 100, 200, 1.0f),
        Tok(" three", 200, 300, 1.0f),
    };
    auto words = GroupTokensIntoWords(tokens);

    ASSERT_EQ(3u, words.size());
    EXPECT_LT(words[0].start_ms, words[1].start_ms);
    EXPECT_LT(words[1].start_ms, words[2].start_ms);
    EXPECT_EQ(" one", words[0].text);
    EXPECT_EQ(" two", words[1].text);
    EXPECT_EQ(" three", words[2].text);
}
