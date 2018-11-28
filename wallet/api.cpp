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

namespace beam
{
    namespace wallet_api
    {
        void append_json_msg(io::FragmentWriter& packer, const Balance& balance)
        {
            json msg;

            msg["jsonrpc"] = "2.0";
            msg["id"] = balance.id;

            msg["method"] = balance.method;
            msg["params"] =
            {
                {"type", balance.type},
                {"addr", std::to_string(balance.addr)},
            };

            serialize_json_msg(packer, msg);
        }

        void append_json_msg(io::FragmentWriter& packer, const BalanceRes& balance)
        {
            json msg;

            msg["jsonrpc"] = "2.0";
            msg["id"] = balance.id;

            msg["result"] = balance.amount;

            serialize_json_msg(packer, msg);
        }

        void append_json_msg(io::FragmentWriter& packer, const UnknownMethodError& error)
        {
            json msg;

            msg["jsonrpc"] = "2.0";
            msg["id"] = error.id;

            msg["error"] = 
            {
                {"code" , -32601},
                {"message", "Procedure not found."}
            };

            serialize_json_msg(packer, msg);
        }
    }
}
