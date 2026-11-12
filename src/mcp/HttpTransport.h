#pragma once

#include "Transport.h"
#include "../provider/CurlUtil.h"

#include <map>
#include <string>

namespace pluto {

class HttpTransport : public Transport {
public:
    HttpTransport(std::string url, std::map<std::string, std::string> headers);
    bool start(std::string&) override { return true; }
    void stop() override {}
    std::string request(const std::string& body, std::string& err) override;
    void notify(const std::string& body) override;

private:
    std::string post(const std::string& body, std::string& err);

    std::string fUrl;
    std::map<std::string, std::string> fHeaders;
    RetryPolicy fRetryPolicy;
    CircuitBreaker fCircuitBreaker;
};

}  // namespace pluto
