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

#pragma once

#include "wallet/wallet.h"
#include "nlohmann/json.hpp"

#define INVALID_JSON_RPC -32600
#define NOTFOUND_JSON_RPC -32601

namespace beam
{
    using json = nlohmann::json;

    class IWalletApiHandler
    {
    public:
        virtual void onInvalidJsonRpc(const json& msg) = 0;
        virtual void onBalanceMessage(int id, int type, const WalletID& address) = 0;
    };

    class WalletApi
    {
    public:
        WalletApi(IWalletApiHandler& handler);

        void getBalanceResponse(int id, const Amount& amount, json& msg);

        bool parse(const char* data, size_t size);

    private:
        void balanceMethod(const json& msg);

    private:
        IWalletApiHandler& _handler;
        std::map<std::string, std::function<void(const json& msg)>> _methods;
    };
}
