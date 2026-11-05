#pragma once
#include "audiohole/scanner.hpp"
#include <map>
namespace audiohole::presentation {
// Worker-side projection only. Structured/raw IDs stay in the journal; visible
// copy uses observed names and real plan counts, never guessed percentages.
class ScanProgressText {
  public:
    std::string consume(const scanner::Progress&);
    int percent() const;
    std::string describe(const scanner::Diagnostic&) const;

  private:
    std::map<std::string, std::string> names_;
    std::size_t device_ = 0, devices_ = 0, done_ = 0, planned_ = 0;
    std::string configuration_;
};
} // namespace audiohole::presentation
