#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Blocking HTTP(S) client API. https:// URLs use TLS without certificate
// verification (pragmatic for a hobby device). Calls block the caller —
// render a "Requesting..." frame before invoking (see HttpDemoApp).
namespace cardos::http {

using Headers = std::vector<std::pair<std::string, std::string>>;

struct Response {
  int status = -1;        // -1 = transport failure (see error)
  std::string body;
  std::string error;
  bool ok() const { return status >= 200 && status < 300; }
};

using Progress = std::function<void(size_t sent, size_t total)>;

Response get(const std::string& url, const Headers& h = {});
Response post(const std::string& url, const std::string& body,
              const std::string& contentType, const Headers& h = {});
Response postJson(const std::string& url, const std::string& json,
                  const Headers& h = {});
// Streams filePath (a /flash or /sd VFS path) as multipart/form-data.
Response uploadFile(const std::string& url, const std::string& filePath,
                    const std::string& fieldName = "file",
                    const Headers& h = {}, Progress onProgress = nullptr);
void setTimeout(uint32_t ms);  // default 10000

}  // namespace cardos::http
