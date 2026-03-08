#pragma once

enum class AuthResult {
  granted,
  denied,
  idle,
  pending_face
};