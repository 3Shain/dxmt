#pragma once

#include <string>
#include <exception>

namespace dxmt {

/**
 * \brief DXMT error
 *
 * A generic exception class that stores a
 * message. Exceptions should be logged.
 */
class MTLD3DError : public std::exception {

public:
  MTLD3DError() {}
  MTLD3DError(std::string &&message) : m_message(std::move(message)) {}

  const std::string &message() const { return m_message; }

  const char *what() const noexcept { return m_message.c_str(); }

private:
  std::string m_message;
};

} // namespace dxmt