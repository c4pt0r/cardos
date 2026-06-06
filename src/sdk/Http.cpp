#include "Http.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_random.h>

#include <algorithm>
#include <cstring>

#include "Multipart.h"

namespace cardos::http {
namespace {
uint32_t timeoutMs_ = 10000;

bool isHttps(const std::string& url) { return url.rfind("https://", 0) == 0; }

// One client per request; secure clients skip cert verification.
struct ClientBox {
  WiFiClient plain;
  WiFiClientSecure secure;
  WiFiClient& pick(const std::string& url) {
    if (!isHttps(url)) return plain;
    secure.setInsecure();
    return secure;
  }
};

void applyHeaders(HTTPClient& http, const Headers& h) {
  for (const auto& kv : h) http.addHeader(kv.first.c_str(), kv.second.c_str());
}

Response finish(HTTPClient& http, int code) {
  Response r;
  if (code > 0) {
    r.status = code;
    r.body = std::string(http.getString().c_str());
  } else {
    r.error = std::string(HTTPClient::errorToString(code).c_str());
  }
  http.end();
  Serial.printf("[http] -> %d %s\n", r.status, r.error.c_str());
  return r;
}

// Streams: multipart prefix + FILE* + multipart suffix, with progress.
class MultipartStream : public Stream {
 public:
  MultipartStream(std::string prefix, FILE* f, size_t fileSize,
                  std::string suffix, Progress progress)
      : prefix_(std::move(prefix)), f_(f), fileSize_(fileSize),
        suffix_(std::move(suffix)), progress_(std::move(progress)) {
    total_ = prefix_.size() + fileSize_ + suffix_.size();
  }
  size_t totalSize() const { return total_; }

  int available() override {
    size_t remaining = total_ - pos_;
    return remaining > 0x7FFFFFFF ? 0x7FFFFFFF : (int)remaining;
  }
  int read() override {
    uint8_t b;
    return readBytes((char*)&b, 1) == 1 ? b : -1;
  }
  size_t readBytes(char* dst, size_t len) override {
    size_t n = 0;
    while (n < len && pos_ < total_) {
      if (pos_ < prefix_.size()) {                       // prefix region
        size_t take = std::min(len - n, prefix_.size() - pos_);
        memcpy(dst + n, prefix_.data() + pos_, take);
        n += take; pos_ += take;
      } else if (pos_ < prefix_.size() + fileSize_) {    // file region
        size_t take = fread(dst + n, 1, len - n, f_);
        if (take == 0) break;
        n += take; pos_ += take;
      } else {                                           // suffix region
        size_t off = pos_ - prefix_.size() - fileSize_;
        size_t take = std::min(len - n, suffix_.size() - off);
        memcpy(dst + n, suffix_.data() + off, take);
        n += take; pos_ += take;
      }
    }
    if (progress_ && pos_ - lastReport_ >= 4096) {
      lastReport_ = pos_;
      progress_(pos_, total_);
    }
    return n;
  }
  int peek() override { return -1; }
  size_t write(uint8_t) override { return 0; }  // read-only stream

 private:
  std::string prefix_;
  FILE* f_;
  size_t fileSize_;
  std::string suffix_;
  Progress progress_;
  size_t total_ = 0, pos_ = 0, lastReport_ = 0;
};
}  // namespace

void setTimeout(uint32_t ms) { timeoutMs_ = ms; }

Response get(const std::string& url, const Headers& h) {
  Serial.printf("[http] GET %s\n", url.c_str());
  ClientBox box;
  HTTPClient http;
  http.setConnectTimeout(timeoutMs_);
  http.setTimeout(timeoutMs_);
  Response r;
  if (!http.begin(box.pick(url), url.c_str())) {
    r.error = "begin() failed";
    return r;
  }
  applyHeaders(http, h);
  return finish(http, http.GET());
}

Response post(const std::string& url, const std::string& body,
              const std::string& contentType, const Headers& h) {
  Serial.printf("[http] POST %s (%u B)\n", url.c_str(), (unsigned)body.size());
  ClientBox box;
  HTTPClient http;
  http.setConnectTimeout(timeoutMs_);
  http.setTimeout(timeoutMs_);
  Response r;
  if (!http.begin(box.pick(url), url.c_str())) {
    r.error = "begin() failed";
    return r;
  }
  http.addHeader("Content-Type", contentType.c_str());
  applyHeaders(http, h);
  return finish(http, http.POST((uint8_t*)body.data(), body.size()));
}

Response postJson(const std::string& url, const std::string& json,
                  const Headers& h) {
  return post(url, json, "application/json", h);
}

Response uploadFile(const std::string& url, const std::string& filePath,
                    const std::string& fieldName, const Headers& h,
                    Progress onProgress) {
  Response r;
  FILE* f = fopen(filePath.c_str(), "rb");
  if (!f) {
    r.error = "cannot open " + filePath;
    return r;
  }
  fseek(f, 0, SEEK_END);
  size_t fileSize = ftell(f);
  fseek(f, 0, SEEK_SET);

  std::string boundary = "----cardos";
  char rnd[9];
  snprintf(rnd, sizeof(rnd), "%08x", (unsigned)esp_random());
  boundary += rnd;

  const char* slash = strrchr(filePath.c_str(), '/');
  std::string filename = slash ? slash + 1 : filePath;

  Serial.printf("[http] UPLOAD %s (%u B) -> %s\n", filePath.c_str(),
                (unsigned)fileSize, url.c_str());
  MultipartStream stream(multipartPrefix(boundary, fieldName, filename), f,
                         fileSize, multipartSuffix(boundary), onProgress);
  ClientBox box;
  HTTPClient http;
  http.setConnectTimeout(timeoutMs_);
  http.setTimeout(timeoutMs_);
  if (!http.begin(box.pick(url), url.c_str())) {
    fclose(f);
    r.error = "begin() failed";
    return r;
  }
  http.addHeader("Content-Type",
                 ("multipart/form-data; boundary=" + boundary).c_str());
  applyHeaders(http, h);
  r = finish(http, http.sendRequest("POST", &stream, stream.totalSize()));
  fclose(f);
  return r;
}

}  // namespace cardos::http
