#pragma once
#include <string>
#include <unordered_set>

class CardVerifier {
public:
  explicit CardVerifier(const std::string& allowlist_path);

  // raw -> card_id，并返回是否允许
  bool verify(const std::string& raw, std::string& out_card_id) const;

private:
  std::unordered_set<std::string> allow_;
  static std::string extract_card_id(const std::string& raw);
};
