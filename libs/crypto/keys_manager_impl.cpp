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

#include "crypto/keys_manager_impl.hpp"

#include <fstream>

#include "common/byteutils.hpp"
#include "cryptography/crypto_provider/crypto_defaults.hpp"
#include "cryptography/ed25519_sha3_impl/internal/sha3_hash.hpp"

using iroha::operator|;

namespace iroha {
  /**
   * Function for the key encryption via XOR
   * @tparam is a key type
   * @param privkey is a private key
   * @param pass_phrase is a key for encryption
   * @return encrypted string
   */
  template <typename T>
  static std::string encrypt(const T &key, const std::string &pass_phrase) {
    std::string ciphertext(key.size(), '\0');
    const auto pass_size = std::max(1ul, pass_phrase.size());

    for (auto i = 0u; i < key.size(); i++) {
      ciphertext[i] = key[i] ^ pass_phrase[i % pass_size];
    }
    return ciphertext;
  }

  /**
   * Function for XOR decryption
   */
  static constexpr auto decrypt = encrypt<shared_model::crypto::Blob::Bytes>;

  KeysManagerImpl::KeysManagerImpl(const std::string &account_name)
      : account_name_(std::move(account_name)),
        log_(logger::log("KeysManagerImpl")) {}

  bool KeysManagerImpl::validate(
      const shared_model::crypto::Keypair &keypair) const {
    try {
      auto test = shared_model::crypto::Blob(sha3_256("12345").to_string());
      auto sig =
          shared_model::crypto::DefaultCryptoAlgorithmType::sign(test, keypair);
      if (not shared_model::crypto::DefaultCryptoAlgorithmType::verify(
              sig, test, keypair.publicKey())) {
        log_->error("key validation failed");
        return false;
      }
    } catch (const BadFormatException &exception) {
      log_->error("Cannot validate keyapir: {}", exception.what());
      return false;
    }
    return true;
  }

  bool KeysManagerImpl::loadFile(const std::string &filename,
                                 std::string &res) {
    std::ifstream file(filename);
    if (not file) {
      log_->error("Cannot read '" + filename + "'");
      return false;
    }
    file >> res;
    return true;
  }

  boost::optional<shared_model::crypto::Keypair> KeysManagerImpl::loadKeys() {
    std::string pub_key;
    std::string priv_key;

    if (not loadFile(account_name_ + public_key_extension, pub_key)
        or not loadFile(account_name_ + private_key_extension, priv_key))
      return boost::none;

    shared_model::crypto::Keypair keypair = shared_model::crypto::Keypair(
        shared_model::crypto::PublicKey(
            shared_model::crypto::Blob::fromHexString(pub_key)),
        shared_model::crypto::PrivateKey(
            shared_model::crypto::Blob::fromHexString(priv_key)));

    return this->validate(keypair) ? boost::make_optional(keypair)
                                   : boost::none;
  }

  boost::optional<shared_model::crypto::Keypair> KeysManagerImpl::loadKeys(
      const std::string &pass_phrase) {
    std::string pub_key;
    std::string priv_key;

    if (not loadFile(account_name_ + public_key_extension, pub_key)
        or not loadFile(account_name_ + private_key_extension, priv_key))
      return boost::none;

    shared_model::crypto::Keypair keypair = shared_model::crypto::Keypair(
        shared_model::crypto::PublicKey(
            shared_model::crypto::Blob::fromHexString(pub_key)),
        shared_model::crypto::PrivateKey(
            decrypt(shared_model::crypto::Blob::fromHexString(priv_key).blob(),
                    pass_phrase)));

    return this->validate(keypair) ? boost::make_optional(keypair)
                                   : boost::none;
  }

  bool KeysManagerImpl::createKeys() {
    shared_model::crypto::Keypair keypair =
        shared_model::crypto::DefaultCryptoAlgorithmType::generateKeypair();

    auto pub = keypair.publicKey().hex();
    auto priv = keypair.privateKey().hex();
    return store(pub, priv);
  }

  bool KeysManagerImpl::createKeys(const std::string &pass_phrase) {
    shared_model::crypto::Keypair keypair =
        shared_model::crypto::DefaultCryptoAlgorithmType::generateKeypair();

    auto pub = keypair.publicKey().hex();
    auto priv = bytestringToHexstring(
        encrypt(keypair.privateKey().blob(), pass_phrase));
    return store(pub, priv);
  }

  bool KeysManagerImpl::store(const std::string &pub, const std::string &priv) {
    std::ofstream pub_file(account_name_ + public_key_extension);
    std::ofstream priv_file(account_name_ + private_key_extension);
    if (not pub_file or not priv_file) {
      return false;
    }

    pub_file << pub;
    priv_file << priv;
    return pub_file.good() && priv_file.good();
  }

  const std::string KeysManagerImpl::public_key_extension = ".pub";
  const std::string KeysManagerImpl::private_key_extension = ".priv";
}  // namespace iroha
