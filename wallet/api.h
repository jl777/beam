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

namespace beam
{
    namespace wallet_api
    {
        struct Message
        {
            int id;
            std::string method;

        protected:
            Message(int id_, const std::string method_)
                : id(id_)
                , method(method_)
            {}

        };

        struct Balance : Message
        {
            int type;
            WalletID addr;

            Balance() : Message(6, "balance") {}
        };

        struct BalanceRes : Message
        {
            Amount amount;

            BalanceRes() : Message(6, "result") {}
        };

        struct UnknownMethodError : Message
        {
            UnknownMethodError() : Message(0, "error") {}
        };

        void append_json_msg(io::FragmentWriter&, const Balance&);
        void append_json_msg(io::FragmentWriter&, const BalanceRes&);
        void append_json_msg(io::FragmentWriter&, const UnknownMethodError&);
    };
}
