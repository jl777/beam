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

#include "api.h"

#include "p2p/json_serializer.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace
{
    int parse_json(const void* buf, size_t bufSize, json& o)
    {
        if (bufSize == 0) return -30000;

        const char* bufc = (const char*)buf;

        try
        {
            o = json::parse(bufc, bufc + bufSize);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR() << "json parse: " << e.what() << "\n" << std::string(bufc, bufc + (bufSize > 1024 ? 1024 : bufSize));
            return -30000;
        }
        return 0; // OK
    }
}

namespace beam
{
    struct jsonrpc_exception
    {
        int code;
        std::string message;
        int id;
    };

    void throwInvalidJsonRpc(int id = 0)
    {
        throw jsonrpc_exception{ -32600 , "Invalid JSON-RPC.", id };
    }

    void throwUnknownJsonRpc(int id)
    {
        throw jsonrpc_exception{ -32601 , "Procedure not found.", id};
    }

    std::string getJsonString(const char* data, size_t size)
    {
        return std::string(data, data + (size > 1024 ? 1024 : size));
    }

    WalletApi::WalletApi(IWalletApiHandler& handler)
        : _handler(handler)
    {
        _methods["balance"] = BIND_THIS_MEMFN(balanceMethod);
    };

    void WalletApi::balanceMethod(const nlohmann::json& msg)
    {
        LOG_DEBUG() << "balanceMethod()";

        auto params = msg["params"];

        int type = params["type"];
        WalletID address;
        address.FromHex(params["addr"]);

        _handler.onBalanceMessage(msg["id"], type, address);
    }

    void WalletApi::getBalanceResponse(int id, const Amount& amount, json& msg)
    {
        msg = json
        {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", amount}
        };
    }

    bool WalletApi::parse(const char* data, size_t size)
    {
        if (size == 0) return false;

        try
        {
            json msg = json::parse(data, data + size);

            if (msg["jsonrpc"] != "2.0") throwInvalidJsonRpc();
            if (msg["id"] <= 0) throwInvalidJsonRpc();
            if (msg["method"] == nullptr) throwInvalidJsonRpc();
            if (msg["params"] == nullptr) throwInvalidJsonRpc();
            if (_methods.find(msg["method"]) == _methods.end()) throwUnknownJsonRpc(msg["id"]);

            try
            {
                _methods[msg["method"]](msg);
            }
            catch (const nlohmann::detail::exception& e)
            {
                LOG_ERROR() << "json parse: " << e.what() << "\n" << getJsonString(data, size);

                throwInvalidJsonRpc(msg["id"]);
            }
        }
        catch (const jsonrpc_exception& e)
        {
            _handler.onInvalidJsonRpc(json
            {
                {"jsonrpc", "2.0"},
                {"id", e.id},
                {"error",
                    {
                        {"code", e.code},
                        {"message", e.message},
                    }
                }
            });
        }
        catch (const std::exception& e)
        {
            LOG_ERROR() << "json parse: " << e.what() << "\n" << getJsonString(data, size);
            return false;
        }

        return true;
    }
}
