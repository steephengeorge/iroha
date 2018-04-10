/**
 * Copyright Soramitsu Co., Ltd. 2018 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "builders/protobuf/queries.hpp"
#include "builders/protobuf/transaction.hpp"
#include "cryptography/crypto_provider/crypto_defaults.hpp"
#include "datetime/time.hpp"
#include "framework/base_tx.hpp"
#include "framework/integration_framework/integration_test_framework.hpp"
#include "interfaces/utils/specified_visitor.hpp"
#include "module/shared_model/builders/protobuf/test_transaction_builder.hpp"
#include "validators/permissions.hpp"

class GetAccount : public ::testing::Test {
 public:
  /**
   * Creates the transaction with the user creation commands
   * @param perms are the permissions of the user
   * @return built tx and a hash of its payload
   */
  auto makeUserWithPerms(const std::vector<std::string> &perms,
                         const std::string &user,
                         const std::string &role,
                         const shared_model::crypto::Keypair &key_pair) {
    auto new_perms = perms;
    new_perms.push_back(iroha::model::can_set_quorum);
    return framework::createUserWithPerms(
               user, key_pair.publicKey(), role, new_perms)
        .build()
        .signAndAddSignature(kAdminKeypair);
  }

  auto insertKV(const std::string &key,
                const std::string &value,
                const std::string &account_id,
                const shared_model::crypto::Keypair &key_pair) {
    return shared_model::proto::TransactionBuilder()
        .txCounter(tx_counter++)
        .createdTime(iroha::time::now())
        .creatorAccountId(account_id)
        .setAccountDetail(kUserId, key, value)
        .build()
        .signAndAddSignature(key_pair);
  }

  auto KVQuery() {
    return shared_model::proto::QueryBuilder()
        .creatorAccountId(kUserId)
        .createdTime(iroha::time::now())
        .queryCounter(query_counter++)
        .getAccount(kUserId)
        .build()
        .signAndAddSignature(kUserKeypair);
  }

  const std::string kUser = "user";
  const std::string kNewRole = "role";
  const std::string kUserId = kUser + "@test";
  const std::string kAdmin = "admin@test";
  const shared_model::crypto::Keypair kAdminKeypair =
      shared_model::crypto::DefaultCryptoAlgorithmType::generateKeypair();
  const shared_model::crypto::Keypair kUserKeypair =
      shared_model::crypto::DefaultCryptoAlgorithmType::generateKeypair();

 private:
  unsigned int tx_counter = 1;
  unsigned int query_counter = 1;
};

/**
 * @given account with values in kv storage
 * @when query GetAccount
 * @then receive account with only keys in kv
 */
TEST_F(GetAccount, GetAccountKVKeys) {
  integration_framework::IntegrationTestFramework()
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {iroha::model::can_get_my_account}, kUser, kNewRole, kUserKeypair))
      .skipBlock()
      .sendQuery(KVQuery(),
                 [](auto &query_response) {
                   auto resp = boost::apply_visitor(
                       shared_model::interface::SpecifiedVisitor<
                           shared_model::interface::AccountResponse>(),
                       query_response.get());
                   ASSERT_TRUE(resp);
                   ASSERT_EQ("{}", resp.value()->account().jsonData());
                 })
      .sendTx(insertKV("key1", "value1", kUserId, kUserKeypair))
      .skipBlock()
      .sendQuery(KVQuery(),
                 [](auto &query_response) {
                   auto resp = boost::apply_visitor(
                       shared_model::interface::SpecifiedVisitor<
                           shared_model::interface::AccountResponse>(),
                       query_response.get());
                   ASSERT_TRUE(resp);
                   ASSERT_EQ("{ \"user@test\" : [\"key1\"] }",
                             resp.value()->account().jsonData());
                 })
      .sendTx(insertKV("key2", "value2", kUserId, kUserKeypair))
      .skipBlock()
      .sendQuery(KVQuery(),
                 [](auto &query_response) {
                   auto resp = boost::apply_visitor(
                       shared_model::interface::SpecifiedVisitor<
                           shared_model::interface::AccountResponse>(),
                       query_response.get());
                   ASSERT_TRUE(resp);
                   ASSERT_EQ("{ \"user@test\" : [\"key1\",\"key2\"] }",
                             resp.value()->account().jsonData());
                 })
      .done();
}
