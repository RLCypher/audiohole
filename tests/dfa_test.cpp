#include "presentation/messages.h"
#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <set>

using namespace audiohole::presentation;
TEST(DfaDecimal, NormalizesHalfUpUsingDecimalDigits) {
    for (const auto& [input, normalized] :
         std::vector<std::pair<std::string, std::string>>{{"1.337", "1.34"},
                                                          {"1.005", "1.01"},
                                                          {"2.675", "2.68"},
                                                          {"5.035", "5.04"},
                                                          {"0.005", "0.01"},
                                                          {"0.0049999999999999999", "0.00"},
                                                          {"10.99999", "11.00"},
                                                          {"0001.337", "1.34"},
                                                          {"  +1.337  ", "1.34"},
                                                          {".125", "0.13"},
                                                          {"3.", "3.00"},
                                                          {"1.337e0", "1.34"},
                                                          {"110000e-4", "11.00"},
                                                          {"0.000001337e6", "1.34"}}) {
        const auto result = parse_dfa(input);
        ASSERT_EQ(result.status, DfaInputStatus::Valid) << input;
        EXPECT_EQ(dfa_text(result.hundredths), normalized) << input;
    }
    EXPECT_EQ(dfa_event(parse_dfa("1.337").hundredths), Message::Dfa134);
}
TEST(DfaDecimal, ChecksRawRangeBeforeRounding) {
    for (const auto* input : {"-0.001", "-1e-99999999999999999999", "-11", "-999"})
        EXPECT_EQ(parse_dfa(input).status, DfaInputStatus::Negative) << input;
    for (const auto* input : {"11.001", "11.00000000000000000001", "12", "999", "1e99999999999999"})
        EXPECT_EQ(parse_dfa(input).status, DfaInputStatus::AboveMaximum) << input;
    for (const auto* input : {"0.00", "-0.000", "0e99999999999999", "1e-99999999999999999999"}) {
        EXPECT_EQ(parse_dfa(input).status, DfaInputStatus::Valid) << input;
        EXPECT_EQ(parse_dfa(input).hundredths, 0) << input;
    }
    EXPECT_EQ(parse_dfa("11.00000000000000000000").hundredths, 1100);
    EXPECT_THROW(dfa_text(-1), std::invalid_argument);
    EXPECT_THROW(dfa_text(1101), std::invalid_argument);
}
TEST(DfaDecimal, HandlesLongPrecisionWithoutOverflowOrTruncation) {
    const std::string zeros(5000, '0');
    EXPECT_EQ(parse_dfa("11." + zeros + "1").status, DfaInputStatus::AboveMaximum);
    EXPECT_EQ(parse_dfa("-0." + zeros + "1").status, DfaInputStatus::Negative);
    EXPECT_EQ(parse_dfa("1." + zeros + "5").hundredths, 100);
    EXPECT_EQ(parse_dfa(std::string(5000, '9')).status, DfaInputStatus::AboveMaximum);
    EXPECT_EQ(parse_dfa("1e-" + std::string(5000, '9')).hundredths, 0);
}
TEST(DfaDecimal, RejectsMalformedAndNonfiniteInput) {
    for (const auto* input :
         {"", " ", ".", "+", "--1", "1..2", "NaN", "inf", "-inf", "1e", "1e+", "1.2x", "12x"})
        EXPECT_EQ(parse_dfa(input).status, DfaInputStatus::Invalid) << input;
}
struct ApprovedEgg {
    int hundredths;
    const char* norm;
    const char* uwu;
};
const std::array<ApprovedEgg, 26> approved{{
    {101, "Have you tried reading the documentation?", "Did you wead the instwuctions? (・ω・)"},
    {121, "Great Scott! 1.21 gigawatts!", "GWEAT SCOTT!! (⊙ω⊙)"},
    {134, "Skill issue.", "Skiww issue (╥﹏╥)"},
    {222, "That's a little too much.", "Two much fuw you? (｡•̀ᴗ-)✧"},
    {256, "All 256 possibilities, and you chose this.", "So many possibiwities! (≧▽≦)"},
    {300, "Make up your mind.", "Pwease pick one! (｡•́︿•̀｡)"},
    {314, "A La Mode Enabled", "I'll take mine cream fiwwed pls (─‿‿─)"},
    {333, "Diet Satan.", "Hawf the eviw, aww the cute (⸝⸝> ᴗ <⸝⸝)"},
    {401, "You're not authorized to fuck around.", "You don't have pewmission! (＞﹏＜)"},
    {403, "Fuck around privileges revoked.", "No mowe fwucking awound! (╥﹏╥)"},
    {404, "This is not the DFA you're looking for.", "Ewwow 404: Bwain not found (×﹏×)"},
    {409, "You have a conflict of interest. And audio devices.", "Too many confwicts! (＠_＠)"},
    {410, "It was here a minute ago.", "Whewe did it go?! (・_・;)"},
    {413, "Your hole isn't big enough.", "It won't fit!! (⁄ ⁄>⁄ ▽ ⁄<⁄ ⁄)"},
    {418, "I'm a teapot.", "I'm a wittwe teapot! (｡･ω･｡)ﾉ♡"},
    {429, "You're fucking around too fast.", "TOO MUCH FWUCKING A-WOUND!! (╯°□°)╯"},
    {451, "It was a pleasure to burn.", "Why is evewything on fiwe?! (⊙﹏⊙)"},
    {500, "Internal server error. Have you tried turning yourself off and on again?",
     "Intewnaw sewvew ewwow! Pwease gib headpats to westawt (╥﹏╥)"},
    {503, "Currently unavailable. Emotionally, mostly.", "Emotionawwy unavaiwabwe (｡•́︿•̀｡)"},
    {621, "I know what you are. (￣︿￣)", "(O‿‿O);;"},
    {707, "Drum machine detected.", "BEEP BOOP! DWUM TIME! (ﾉ≧∀≦)ﾉ"},
    {777, "Jackpot.", "JACKPOT!! ヽ(>∀<☆)ノ"},
    {808, "808s and heartbreak.", "BASS GO BWOOOOOM!! (ﾉ≧∀≦)ﾉ"},
    {909, "Four on the floor.", "UNTZ UNTZ UNTZ!! ヽ(⌐■_■)ノ♪"},
    {911, "Sir, a second audio stream has hit the buffer.", "A SECOND STWEAM?! (╯°□°)╯︵ ┻━┻"},
    {1024, "That's a whole kibibyte of fuckery.", "1024 tiny bits of mischief! (≧▽≦)"},
}};
void PrintTo(const ApprovedEgg& egg, std::ostream* output) {
    *output << dfa_text(egg.hundredths);
}
class DfaEgg : public testing::TestWithParam<ApprovedEgg> {};
TEST_P(DfaEgg, MatchesExactOwnerAuthoredNormAndUwUText) {
    Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    const auto egg = GetParam();
    const auto parsed = parse_dfa(dfa_text(egg.hundredths));
    ASSERT_EQ(parsed.status, DfaInputStatus::Valid);
    const auto event = dfa_event(parsed.hundredths);
    EXPECT_NE(event, Message::Dfa);
    EXPECT_EQ(messages.show(event, "default", 1).commentary, egg.norm);
    EXPECT_EQ(messages.show(event, "uwu", 1).commentary, egg.uwu);
    EXPECT_EQ(technical(event), "DFA Acknowledged");
}
INSTANTIATE_TEST_SUITE_P(OwnerApproved, DfaEgg, testing::ValuesIn(approved),
                         [](const testing::TestParamInfo<ApprovedEgg>& info) {
                             return "Value" + std::to_string(info.param.hundredths);
                         });
TEST(DfaCatalog, ValidationUsesAuthoredPoolsAndNoVanillaFlavor) {
    Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    EXPECT_EQ(messages.show(Message::DfaNegative, "default", 1).commentary, "NEGATIVE FUCK.");
    EXPECT_EQ(messages.show(Message::DfaNegative, "uwu", 1).commentary, "NYEGATIVE FWUCK?! (╥﹏╥)");
    EXPECT_EQ(messages.show(Message::DfaAboveMaximum, "default", 1).commentary,
              "Too much fucking around.");
    EXPECT_EQ(messages.show(Message::DfaAboveMaximum, "uwu", 1).commentary,
              "Too much fwucking awound!! (╥﹏╥)");
    EXPECT_TRUE(messages.variants(Message::DfaNegative, "vanilla").empty());
    EXPECT_TRUE(messages.variants(Message::DfaAboveMaximum, "vanilla").empty());
    EXPECT_EQ(messages.variants(Message::Dfa, "default").size(), 23u);
    EXPECT_EQ(messages.variants(Message::Dfa, "uwu").size(), 23u);
}
TEST(DfaCatalog, EachResponseCarriesOwnedExpressionMetadataLikeBother) {
    Messages messages;
    ASSERT_TRUE(messages.load_directory(AUDIOHOLE_TEST_PERSONALITIES_DIR).empty());
    for (const auto* id : {"default", "uwu"}) {
        const auto* package = messages.package(id);
        ASSERT_NE(package, nullptr);
        std::set<std::string> expressions;
        std::size_t count = 0;
        for (int i = 0; i < static_cast<int>(Message::Count); ++i) {
            const auto event = static_cast<Message>(i);
            if (!event_id(event).starts_with("dfa"))
                continue;
            const auto pool = messages.variants(event, id);
            ASSERT_FALSE(pool.empty());
            for (const auto& response : pool) {
                EXPECT_EQ(response.category, "dfa");
                EXPECT_FALSE(response.subcategory.empty());
                EXPECT_EQ(response.fallback_expression, "idle");
                EXPECT_NE(std::find(package->avatar_states.begin(), package->avatar_states.end(),
                                    response.expression),
                          package->avatar_states.end());
                expressions.insert(response.expression);
                ++count;
            }
            const auto selected = messages.show(event, id, 1);
            const auto found = std::find_if(pool.begin(), pool.end(), [&](const auto& response) {
                return response.id == selected.response_id;
            });
            ASSERT_NE(found, pool.end());
            EXPECT_EQ(selected.expression, found->expression);
            EXPECT_EQ(selected.subcategory, found->subcategory);
            EXPECT_EQ(messages.show(event, id, 1).expression, selected.expression);
        }
        EXPECT_GE(expressions.size(), 12u); // Prevent a return to one face for every response.
        EXPECT_EQ(count, std::string(id) == "default" ? 58u : 61u);
        EXPECT_EQ(messages.show(Message::Dfa121, id, 2).expression, "surprised");
        EXPECT_EQ(messages.show(Message::Dfa404, id, 2).expression, "confused");
        EXPECT_EQ(messages.show(Message::Dfa777, id, 2).expression, "celebrating");
    }
}
