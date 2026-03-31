// tests/test_card_verifier.cpp
//
// Unit tests for CardVerifier
//
// Covered cases:
//   1. Pure-digit card in allowlist  → verify() returns true
//   2. Card not in allowlist         → verify() returns false (card_id still extracted)
//   3. Track2 format  ;PAN=...?      → PAN is extracted correctly
//   4. Garbage-prefix/suffix input   → fallback extracts all digits
//   5. Empty / too-short card number → verify() returns false
//
// No hardware, no file system needed for cases 1-5 because we write a
// temporary allowlist file in SetUpTestSuite.

#include <gtest/gtest.h>
#include "verifier/verifier.hpp"

#include <cstdio>
#include <fstream>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Fixture: writes a small allowlist to a temp file once for the whole suite
// ─────────────────────────────────────────────────────────────────────────────
class CardVerifierTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Build a temporary allowlist file
        s_allowlist_path = "test_allowlist_tmp.txt";
        std::ofstream f(s_allowlist_path);
        // Comment and blank lines should be ignored by CardVerifier
        f << "# allowed cards\n";
        f << "\n";
        f << "10320049\n";
        f << "12345678\n";
        f.close();

        s_verifier = new CardVerifier(s_allowlist_path);
    }

    static void TearDownTestSuite() {
        delete s_verifier;
        s_verifier = nullptr;
        std::remove(s_allowlist_path.c_str());
    }

    // Convenience: call verify and return both the bool result and out_card_id
    static bool verify(const std::string& raw, std::string& out_id) {
        return s_verifier->verify(raw, out_id);
    }

    static CardVerifier*  s_verifier;
    static std::string    s_allowlist_path;
};

CardVerifier* CardVerifierTest::s_verifier      = nullptr;
std::string   CardVerifierTest::s_allowlist_path;


// ─────────────────────────────────────────────────────────────────────────────
// Test 1: Pure-digit card that IS in the allowlist → true
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(CardVerifierTest, PureDigitAllowedCardReturnsTrue) {
    std::string card_id;
    bool result = verify("10320049", card_id);

    EXPECT_TRUE(result);
    EXPECT_EQ(card_id, "10320049");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2: Card NOT in the allowlist → false, but card_id is still extracted
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(CardVerifierTest, CardNotInAllowlistReturnsFalse) {
    std::string card_id;
    bool result = verify("99999999", card_id);

    EXPECT_FALSE(result);
    // The card number should still be extracted even when denied
    EXPECT_EQ(card_id, "99999999");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: Track2 magnetic-stripe format  ;PAN=expiry?
//   Input  ";10320049=1234?"  → PAN = "10320049" → allowed → true
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(CardVerifierTest, Track2FormatExtractsPanCorrectly) {
    std::string card_id;
    bool result = verify(";10320049=1234?", card_id);

    EXPECT_TRUE(result);
    EXPECT_EQ(card_id, "10320049");
}

// Also check a Track2 card whose PAN is NOT in the allowlist
TEST_F(CardVerifierTest, Track2FormatNotAllowedReturnsFalse) {
    std::string card_id;
    bool result = verify(";99999999=9999?", card_id);

    EXPECT_FALSE(result);
    EXPECT_EQ(card_id, "99999999");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4: Input with non-digit prefix/suffix → fallback extracts all digits
//   Input  "abc10320049xyz"  → digits = "10320049" → allowed → true
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(CardVerifierTest, GarbagePrefixSuffixFallbackExtractsDigits) {
    std::string card_id;
    bool result = verify("abc10320049xyz", card_id);

    EXPECT_TRUE(result);
    EXPECT_EQ(card_id, "10320049");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5a: Empty string → verify() returns false
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(CardVerifierTest, EmptyStringReturnsFalse) {
    std::string card_id;
    bool result = verify("", card_id);

    EXPECT_FALSE(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5b: Card shorter than 4 digits → verify() returns false
//   (code: if (out_card_id.size() < 4 || ...) return false;)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(CardVerifierTest, TooShortCardReturnsFalse) {
    std::string card_id;
    bool result = verify("123", card_id);   // 3 digits – below the minimum of 4

    EXPECT_FALSE(result);
    EXPECT_EQ(card_id, "123");  // extracted, but rejected by length check
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5c: Card longer than 32 digits → verify() returns false
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(CardVerifierTest, TooLongCardReturnsFalse) {
    // 33 digits
    std::string long_card(33, '1');
    std::string card_id;
    bool result = verify(long_card, card_id);

    EXPECT_FALSE(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bonus: constructor ignores blank lines and '#' comments in allowlist
//   (implicitly tested by the fact that "10320049" is found despite the
//    comment and blank line at the top of the file, but we make it explicit)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(CardVerifierTest, AllowlistIgnoresCommentsAndBlankLines) {
    // "#allowed" should NOT itself be treated as a valid card ID
    std::string card_id;
    // If the comment line "# allowed cards" were accidentally inserted into
    // the set, a crafted raw input could match it.  Verify that does not happen.
    bool result = verify("# allowed cards", card_id);
    EXPECT_FALSE(result);
}
