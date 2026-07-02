// JSON-RPC client for Bitcoin Core, backed by libcurl.
//
// Implement each TODO so the program can talk to the node.

#include "rpc_client.h"

#include <curl/curl.h>

#include <stdexcept>
#include <string>
#include <utility>

BitcoinRPC::BitcoinRPC(RpcConfig cfg) : cfg_(std::move(cfg))
{
    // TODO: initialise a libcurl easy handle into curl_ (throw on failure).
    curl_ = curl_easy_init();
    if (!curl_)
    {
        throw std::runtime_error("failed to initialize CURL easy handle");
    }
}

BitcoinRPC::~BitcoinRPC()
{
    // TODO: clean up the libcurl easy handle if non-null.
    if (curl_)
    {
        curl_easy_cleanup(static_cast<CURL *>(curl_));
    }
}

BitcoinRPC::BitcoinRPC(BitcoinRPC &&other) noexcept
    : cfg_(std::move(other.cfg_)), curl_(other.curl_)
{
    other.curl_ = nullptr;
}

BitcoinRPC &BitcoinRPC::operator=(BitcoinRPC &&other) noexcept
{
    // TODO: free any existing handle, then steal other's handle/config.
    if (this == &other) return *this;
    if (curl_)
        {
            curl_easy_cleanup(static_cast<CURL *>(curl_));
        }
        cfg_ = std::move(other.cfg_);
        curl_ = other.curl_;
        other.curl_ = nullptr;
    return *this;    
}

size_t BitcoinRPC::write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    // TODO: append (size * nmemb) bytes from ptr to the std::string in userdata
    //       and return the number of bytes handled.
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);    
    return size * nmemb;
}

nlohmann::json BitcoinRPC::post(const std::string &url, const nlohmann::json &body)
{
    // TODO: POST body.dump() to url with a JSON content-type header and HTTP
    //       basic auth; capture the response via write_cb; parse it; raise on a
    //       non-null JSON-RPC "error"; return the "result" field.
    CURL* curl = static_cast<CURL*>(curl_);

    std::string request_body = body.dump();
    std::string response_body;

    std::string auth = cfg_.user + ":" + cfg_.pass;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,         url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD,     auth.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH,    CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,  headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,  request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request_body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,  &response_body);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK)
        throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));

    nlohmann::json response = nlohmann::json::parse(response_body);

    if (!response["error"].is_null())
        throw std::runtime_error("RPC error: " + response["error"].dump());
  
    return response["result"];
}

nlohmann::json BitcoinRPC::call(const std::string &method, const nlohmann::json &params)
{
    // TODO: post(build_base_url(cfg_), build_rpc_request(method, params)).
    return post(build_base_url(cfg_), build_rpc_request(method, params));
}

nlohmann::json BitcoinRPC::wallet_call(const std::string &wallet,
                                       const std::string &method,
                                       const nlohmann::json &params)
{
    // TODO: post(build_wallet_url(cfg_, wallet), build_rpc_request(method, params)).
    return post(build_wallet_url(cfg_, wallet), build_rpc_request(method, params));
}
