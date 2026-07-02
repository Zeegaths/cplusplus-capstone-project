// Pure helper functions for the capstone (no network / no I/O).
//
// These are unit-tested by tests/main_test.cpp without a running Bitcoin node.
// Implement each TODO so that the GoogleTest suite passes.

#include "main.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <stdexcept>
#include <string>

std::string build_base_url(const RpcConfig& cfg) {
    // TODO: return "<scheme>://<host>:<port>", e.g. "http://127.0.0.1:18443".    
    return cfg.scheme + "://" + cfg.host + ":" + std::to_string(cfg.port);
}

std::string build_wallet_url(const RpcConfig& cfg, const std::string& wallet) {
    // TODO: return the base URL with "/wallet/<wallet>" appended.
    return build_base_url(cfg) + "/wallet/" + wallet;
}

nlohmann::json build_rpc_request(const std::string& method,
                                 const nlohmann::json& params,
                                 const std::string& id) {
    // TODO: build {"jsonrpc":"1.0","id":id,"method":method,"params":params}.  
    return {
        {"jsonrpc", "1.0"},
        {"id", id},
        {"method", method},
        {"params", params}
    };
}

WalletAction decide_wallet_action(const std::vector<std::string>& on_disk,
                                  const std::vector<std::string>& loaded,
                                  const std::string& wallet) {
    // TODO: Create if not on disk; Load if on disk but not loaded; else None.
    auto contains = [](const std::vector<std::string>& vec, const std::string& val) {
        return std::find(vec.begin(), vec.end(), val) != vec.end();
    };
    if (!contains(on_disk, wallet))
    {
        return WalletAction::Create;
    }
    if (!contains(loaded, wallet))
    {
        return WalletAction::Load;
    }
    return WalletAction::None;
}

std::vector<VoutEntry> parse_vouts(const nlohmann::json& decoded_tx) {
    // TODO: read each entry of decoded_tx["vout"] into a VoutEntry (value, n,
    //       and scriptPubKey.address with fallback to addresses[0]).
    std::vector<VoutEntry> vouts;
    for (const auto& vout : decoded_tx["vout"])
    {
        VoutEntry entry;
        entry.value = vout["value"].get<double>();
        entry.n = vout["n"].get<long long>();
        if (vout["scriptPubKey"].contains("address"))
        {
            entry.address = vout["scriptPubKey"]["address"].get<std::string>();
        }
        else if (vout["scriptPubKey"].contains("addresses") && !vout["scriptPubKey"]["addresses"].empty())
        {
            entry.address = vout["scriptPubKey"]["addresses"][0].get<std::string>();
        }
        else
        {
            throw std::runtime_error("No address found in scriptPubKey");
        }
        vouts.push_back(entry);
    }
    return vouts;
}

VoutEntry select_recipient_vout(const std::vector<VoutEntry>& vouts,
                                const std::string& trader_address) {
    // TODO: return the output whose address == trader_address (throw if none).
    for (const auto& vout : vouts)
    {
        if (vout.address == trader_address)
        {
            return vout;
        }
    }
    throw std::runtime_error("recipient vout not found: " + trader_address);
}

VoutEntry select_change_vout(const std::vector<VoutEntry>& vouts,
                             const std::string& trader_address) {
    // TODO: return the output whose address != trader_address (throw if none).
    for (const auto& vout : vouts)
    {
        if (vout.address != trader_address)
        {
            return vout;
        }
    }
    throw std::runtime_error("change vout not found: " + trader_address);  
}

VoutEntry resolve_input_prevout(const nlohmann::json& prev_decoded,
                                long long input_vout) {
    // TODO: index prev_decoded["vout"] at input_vout (throw if out of range).
    const auto& vouts = prev_decoded["vout"];
    if (input_vout < 0 || input_vout >= vouts.size())
    {
        throw std::runtime_error("input_vout index out of range");
    }
    const auto& vout = vouts[input_vout];
    VoutEntry entry;
    entry.value = vout["value"].get<double>();
    entry.n = vout["n"].get<long long>();
    if (vout["scriptPubKey"].contains("address"))
    {
        entry.address = vout["scriptPubKey"]["address"].get<std::string>();
    }
    else if (vout["scriptPubKey"].contains("addresses") && !vout["scriptPubKey"]["addresses"].empty())
    {
        entry.address = vout["scriptPubKey"]["addresses"][0].get<std::string>();
    }
    else
    {
        throw std::runtime_error("No address found in scriptPubKey");
    }
    return entry; 
   
}

std::string format_btc(double amount) {
    std::array<char, 64> buf;
    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), amount);
    return std::string(buf.data(), ptr);
}

std::string format_report(const TxReport& r) {
    return r.txid + "\n" +
           r.miner_input_address + "\n" +
           format_btc(r.miner_input_amount) + "\n" +
           r.trader_output_address + "\n" +
           format_btc(r.trader_output_amount) + "\n" +
           r.miner_change_address + "\n" +
           format_btc(r.miner_change_amount) + "\n" +
           format_btc(r.fee) + "\n" +
           std::to_string(r.block_height) + "\n" +
           r.block_hash + "\n";
}
