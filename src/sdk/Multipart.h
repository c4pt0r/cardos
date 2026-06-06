#pragma once
#include <string>

// Pure multipart/form-data encoding helpers. No Arduino includes.
namespace cardos::http {

inline std::string multipartPrefix(
    const std::string& boundary, const std::string& fieldName,
    const std::string& filename,
    const std::string& contentType = "application/octet-stream") {
  return "--" + boundary + "\r\n" +
         "Content-Disposition: form-data; name=\"" + fieldName +
         "\"; filename=\"" + filename + "\"\r\n" +
         "Content-Type: " + contentType + "\r\n\r\n";
}

inline std::string multipartSuffix(const std::string& boundary) {
  return "\r\n--" + boundary + "--\r\n";
}

}  // namespace cardos::http
