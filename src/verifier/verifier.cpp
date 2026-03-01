#include "verifier/verifier.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>

static std::string trim(const std::string& s) {
  auto a = s.find_first_not_of(" \t\r\n");
  auto b = s.find_last_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  return s.substr(a, b - a + 1);
}

CardVerifier::CardVerifier(const std::string& allowlist_path) {
  std::ifstream in(allowlist_path);
  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    allow_.insert(line);
  }
}

std::string CardVerifier::extract_card_id(const std::string& raw_in) {
  std::string raw = trim(raw_in);
  if (raw.empty()) return "";

  // 纯数字直接用
  bool all_digits = std::all_of(raw.begin(), raw.end(),
                                [](unsigned char c){ return std::isdigit(c); });
  if (all_digits) return raw;

  // Track2 常见：;1234...=...
  auto semi = raw.find(';');
  auto eq   = raw.find('=');
  if (semi != std::string::npos && eq != std::string::npos && eq > semi + 1) {
    std::string pan = raw.substr(semi + 1, eq - (semi + 1));
    bool pan_digits = !pan.empty() && std::all_of(pan.begin(), pan.end(),
                         [](unsigned char c){ return std::isdigit(c); });
    if (pan_digits) return pan;
  }

  // fallback：抽取所有数字
  std::string digits;
  for (unsigned char c : raw) if (std::isdigit(c)) digits.push_back(char(c));
  return digits;
}

bool CardVerifier::verify(const std::string& raw, std::string& out_card_id) const {
  out_card_id = extract_card_id(raw);
  if (out_card_id.empty()) return false;
  if (out_card_id.size() < 4 || out_card_id.size() > 32) return false;
  return allow_.find(out_card_id) != allow_.end();
}