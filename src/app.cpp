#include "main.h"
#include "rpc_client.h"

#include <curl/curl.h>

#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
    std::vector<std::string> read_wallets_from_disk(BitcoinRPC& client) {
        auto result = client.call("listwalletdir");
        std::vector<std::string> wallets;
        for (const auto& entry : result["wallets"]) {
            wallets.push_back(entry["name"].get<std::string>());
        }
        return wallets;
    }

    std::vector<std::string> read_loaded_wallets(BitcoinRPC& client) {
        return client.call("listwallets").get<std::vector<std::string> >();
    }

    void ensure_wallet_exists(BitcoinRPC& client, const std::string& wallet) {
        auto on_disk = read_wallets_from_disk(client);
        auto loaded = read_loaded_wallets(client);

        switch (decide_wallet_action(on_disk, loaded, wallet)) {
            case WalletAction::Create:
                client.call("createwallet", nlohmann::json::array({wallet}));
                break;
            case WalletAction::Load:
                client.call("loadwallet", nlohmann::json::array({wallet}));
                break;
            case WalletAction::None:
                break;
        }
    }
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    int exit_code = 0;
    try {
        RpcConfig cfg;
        BitcoinRPC client(cfg);

        // 1. wallets exist
        ensure_wallet_exists(client, "Miner");
        ensure_wallet_exists(client, "Trader");

        // 2. generate miner address and mine 101 blocks
        //  Coinbase maturity requires 100 block confirmations before the bitcoin becomes spendable -> This prevents consensus fork attacks. The 101st block is mined to ensure the miner's reward is spendable.
        std::string miner_address = client.wallet_call("Miner", "getnewaddress", nlohmann::json::array({ "Mining Reward" })).get<std::string>();
        client.call("generatetoaddress", nlohmann::json::array({ 101, miner_address }));

        // 3. print miner balance
        double miner_balance = client.wallet_call("Miner", "getbalance").get<double>();
        std::cout << "Miner balance: " << format_btc(miner_balance) << "\n";

        // 4. generate trader receiving address
        std::string trader_address = client.wallet_call("Trader", "getnewaddress", nlohmann::json::array({ "Received" })).get<std::string>();

        // 5. send 20 BTC from Miner to Trader
        const std::string txid = client.wallet_call("Miner", "sendtoaddress", nlohmann::json::array({ trader_address, 20.0 })).get<std::string>();

        // 6. confirm transaction is in mempool
        const auto mempool = client.call("getrawmempool").get<std::vector<std::string> >();
        if (std::find(mempool.begin(), mempool.end(), txid) == mempool.end()) {
            throw std::runtime_error("Transaction not found in mempool: " + txid);
        }

        // 7. mine 1 block to confirm it
        client.call("generatetoaddress", nlohmann::json::array({ 1, miner_address }));

        // 8. fetch transaction details
        const auto tx_details = client.wallet_call("Trader", "gettransaction", nlohmann::json::array({ txid }));
        const double fee = tx_details["fee"].get<double>();
        const std::string blockhash = tx_details["blockhash"].get<std::string>();

        const long long block_height = client.call("getblockheader", nlohmann::json::array({ blockhash }))["height"].get<long long>();

        // decode transaction
        const auto decoded_tx = client.call("decoderawtransaction", nlohmann::json::array({ tx_details["hex"].get<std::string>() }));

        // resolve input using tested helper
        const auto& vin_entry = decoded_tx["vin"][0];
        const auto input_txid = vin_entry["txid"].get<std::string>();
        const auto prev_vout = vin_entry["vout"].get<long long>();
        const auto prev_tx = client.call("getrawtransaction", nlohmann::json::array({ input_txid, true }));

        const VoutEntry miner_input = resolve_input_prevout(prev_tx, prev_vout);

        const auto vouts = parse_vouts(decoded_tx);
        const auto recipient = select_recipient_vout(vouts, trader_address);
        const auto change = select_change_vout(vouts, trader_address);

        TxReport report;
        report.txid = txid;
        report.miner_input_address = miner_input.address;
        report.miner_input_amount = miner_input.value;
        report.trader_output_address = recipient.address;
        report.trader_output_amount = recipient.value;
        report.miner_change_address = change.address;
        report.miner_change_amount = change.value;
        report.fee = fee;
        report.block_height = block_height;
        report.block_hash = blockhash;

        // Write report to out.txt
        std::ofstream outfile("out.txt");
        if (!outfile) {
            throw std::runtime_error("Failed to open out.txt for writing");
        }
        outfile << format_report(report);

        std::cout << "Report written to out.txt\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        exit_code = 1;
    }

    curl_global_cleanup();
    return exit_code;
}
