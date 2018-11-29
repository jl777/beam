// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>

#include "test_helpers.h"

#include "wallet/api.h"
#include "nlohmann/json.hpp"

using namespace std;
using namespace beam;
using json = nlohmann::json;

WALLET_TEST_INIT

#define JSON_CODE(...) #__VA_ARGS__
#define CHECK_JSON_FIELD(msg, name) WALLET_CHECK(msg.find(name) != msg.end())

using jsonFunc = std::function<void(const json&)>;

namespace
{
    class WalletApiHandlerBase : public IWalletApiHandler
    {
        void onInvalidJsonRpc(const json& msg) override {}
        void onBalanceMessage(int id, int type, const WalletID& address) override {}
    };

    void testInvalidJsonRpc(jsonFunc func, const std::string& msg)
    {
        class WalletApiHandler : public WalletApiHandlerBase
        {
        public:
            jsonFunc func;

            void onInvalidJsonRpc(const json& msg) override
            {
                cout << msg << endl;
                func(msg);
            }
        };

        WalletApiHandler handler;
        handler.func = func;

        WalletApi api(handler);

        api.parse(msg.data(), msg.size());
    }

    void testErrorHeader(const json& msg)
    {
        CHECK_JSON_FIELD(msg, "jsonrpc");
        CHECK_JSON_FIELD(msg, "id");
        CHECK_JSON_FIELD(msg, "error");
        CHECK_JSON_FIELD(msg["error"], "code");
        CHECK_JSON_FIELD(msg["error"], "message");

        WALLET_CHECK(msg["jsonrpc"] == "2.0");
    }
}

int main()
{
    testInvalidJsonRpc([](const json& msg)
    {
        testErrorHeader(msg);

        WALLET_CHECK(msg["id"] == nullptr);
        WALLET_CHECK(msg["error"]["code"] == INVALID_JSON_RPC);
    }, JSON_CODE({}));

    testInvalidJsonRpc([](const json& msg)
    {
        testErrorHeader(msg);

        WALLET_CHECK(msg["id"] == nullptr);
        WALLET_CHECK(msg["error"]["code"] == INVALID_JSON_RPC);
    }, JSON_CODE(
    {
        "jsonrpc": "2.0",
        "method" : 1,
        "params" : "bar"
    }));

    return WALLET_CHECK_RESULT;
}
