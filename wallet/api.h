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

    struct CreateAddress
    {
        std::string metadata;

        struct Response
        {
            WalletID address;
        };
    };

    struct Balance
    {
        int type;
        WalletID address;

        struct Response
        {
            Amount amount;
        };
    };

    class IWalletApiHandler
    {
    public:
        virtual void onInvalidJsonRpc(const json& msg) = 0;

        virtual void onMessage(int id, const CreateAddress& data) = 0;
        virtual void onMessage(int id, const Balance& data) = 0;
    };

    class WalletApi
    {
    public:
        WalletApi(IWalletApiHandler& handler);

        void getResponse(int id, const CreateAddress::Response& data, json& msg);
        void getResponse(int id, const Balance::Response& data, json& msg);

        void getCreateAddressResponse(int id, const WalletID& address, json& msg);
        void getBalanceResponse(int id, const Amount& amount, json& msg);

        bool parse(const char* data, size_t size);

    private:

        void createAddressMethod(const json& msg);
        void balanceMethod(const json& msg);

    private:
        IWalletApiHandler& _handler;
        std::map<std::string, std::function<void(const json& msg)>> _methods;
    };
}
