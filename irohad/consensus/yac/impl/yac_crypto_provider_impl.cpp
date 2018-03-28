/**
 * Copyright Soramitsu Co., Ltd. 2017 All Rights Reserved.
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

#include "consensus/yac/impl/yac_crypto_provider_impl.hpp"
#include "consensus/yac/transport/yac_pb_converters.hpp"
#include "cryptography/crypto_provider/crypto_signer.hpp"
#include "cryptography/crypto_provider/crypto_verifier.hpp"

namespace iroha {
  namespace consensus {
    namespace yac {
      std::shared_ptr<shared_model::interface::Signature> createEmptySig() {
        auto sig = shared_model::proto::SignatureBuilder()
                       .publicKey(shared_model::crypto::PublicKey(""))
                       .signedData(shared_model::crypto::Signed(""))
                       .build();
        return clone(sig);
      }

      CryptoProviderImpl::CryptoProviderImpl(const keypair_t &keypair)
          : keypair_(keypair) {}

      bool CryptoProviderImpl::verify(CommitMessage msg) {
        return std::all_of(
            std::begin(msg.votes),
            std::end(msg.votes),
            [this](const auto &vote) { return this->verify(vote); });
      }

      bool CryptoProviderImpl::verify(RejectMessage msg) {
        return std::all_of(
            std::begin(msg.votes),
            std::end(msg.votes),
            [this](const auto &vote) { return this->verify(vote); });
      }

      bool CryptoProviderImpl::verify(VoteMessage msg) {
        auto serialized =
            PbConverters::serializeVote(msg).hash().SerializeAsString();
        auto blob = shared_model::crypto::Blob(serialized);

        return shared_model::crypto::CryptoVerifier<>::verify(
            msg.signature->signedData(), blob, msg.signature->publicKey());
      }

      VoteMessage CryptoProviderImpl::getVote(YacHash hash) {
        VoteMessage vote;
        vote.hash = hash;
        vote.signature = createEmptySig();
        auto serialized =
            PbConverters::serializeVote(vote).hash().SerializeAsString();
        auto blob = shared_model::crypto::Blob(serialized);
        auto pubkey =
            shared_model::crypto::PublicKey(keypair_.pubkey.to_string());
        auto privkey =
            shared_model::crypto::PrivateKey(keypair_.privkey.to_string());
        auto signature = shared_model::crypto::CryptoSigner<>::sign(
            blob, shared_model::crypto::Keypair(pubkey, privkey));

        shared_model::builder::DefaultSignatureBuilder()
            .publicKey(pubkey)
            .signedData(signature)
            .build()
            .match(
                [&](iroha::expected::Value<
                    std::shared_ptr<shared_model::interface::Signature>> &sig) {
                  vote.signature = sig.value;
                },
                [](iroha::expected::Error<std::shared_ptr<std::string>> &err) {
                  std::cout << err.error << std::endl;
                });
        return vote;
      }

    }  // namespace yac
  }    // namespace consensus
}  // namespace iroha
