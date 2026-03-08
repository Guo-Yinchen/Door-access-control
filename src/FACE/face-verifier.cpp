#include "FACE/face-verifier.hpp"

#include <iostream>

bool FaceVerifier::verify(const std::string& card_id) {
  std::cout << "[FACE] Face verification required for card: " << card_id << "\n";
  std::cout << "[FACE] Enter y to simulate success, n to simulate failure: ";

  char ch = 0;
  std::cin >> ch;

  return ch == 'y' || ch == 'Y';
}