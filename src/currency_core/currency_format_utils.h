// Copyright (c) 2012-2013 The Cryptonote developers
// Copyright (c) 2012-2013 The Boolberry developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
#include "currency_protocol/currency_protocol_defs.h"

#include "account.h"
#include "include_base_utils.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "crypto/wild_keccak.h"
#include "difficulty.h"

#define MAX_ALIAS_LEN         255
#define VALID_ALIAS_CHARS     "0123456789abcdefghijklmnopqrstuvwxyz-."

namespace currency
{

  struct tx_source_entry
  {
    typedef std::pair<uint64_t, crypto::public_key> output_entry;

    std::vector<output_entry> outputs;  //index + key
    uint64_t real_output;               //index in outputs vector of real output_entry
    crypto::public_key real_out_tx_key; //real output's transaction's public key
    size_t real_output_in_tx_index;     //index in transaction outputs vector
    uint64_t amount;                    //money
  };

  struct tx_destination_entry
  {
    uint64_t amount;                    //money
    account_public_address addr;        //destination address

    tx_destination_entry() : amount(0), addr(AUTO_VAL_INIT(addr)) { }
    tx_destination_entry(uint64_t a, const account_public_address &ad) : amount(a), addr(ad) { }
  };

  struct alias_info_base
  {
    account_public_address m_address;
    crypto::secret_key m_view_key;
    crypto::signature m_sign;     //is this field set no nonzero - that means update alias operation
    std::string m_text_comment;
  };

  struct alias_info: public alias_info_base
  {
    std::string m_alias;
  };

  struct tx_extra_info 
  {
    crypto::public_key m_tx_pub_key;
    crypto::hash m_offers_hash;
    alias_info m_alias;
    std::string m_user_data_blob;
    extra_attachment_info m_attachment_info;
  };

  //---------------------------------------------------------------
  void get_transaction_prefix_hash(const transaction_prefix& tx, crypto::hash& h);
  crypto::hash get_transaction_prefix_hash(const transaction_prefix& tx);
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, transaction& tx, crypto::hash& tx_hash, crypto::hash& tx_prefix_hash);
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, transaction& tx);  
  bool construct_miner_tx(size_t height, size_t median_size, uint64_t already_generated_coins, 
                                                             const wide_difficulty_type pos_diff,
                                                             size_t current_block_size, 
                                                             uint64_t fee, 
                                                             const account_public_address &miner_address, 
                                                             transaction& tx, 
                                                             const blobdata& extra_nonce = blobdata(), 
                                                             size_t max_outs = 11, 
                                                             const alias_info& alias = alias_info(),
                                                             bool pos = false,
                                                             const pos_entry& pe = pos_entry());

  bool construct_miner_tx(size_t height, size_t median_size, uint64_t already_generated_coins, 
                                                             size_t current_block_size, 
                                                             uint64_t fee, 
                                                             const std::vector<tx_destination_entry>& destinations,
                                                             transaction& tx, 
                                                             const blobdata& extra_nonce = blobdata(),
                                                             size_t max_outs = 11,
                                                             const alias_info& alias = alias_info(),
                                                             bool pos = false,
                                                             const pos_entry& pe = pos_entry());


  //---------------------------------------------------------------
  bool construct_tx_out(const account_public_address& destination_addr, const crypto::secret_key& tx_sec_key, size_t output_index, uint64_t amount, transaction& tx, uint8_t tx_outs_attr = CURRENCY_TO_KEY_OUT_RELAXED);
  bool validate_alias_name(const std::string& al);
  void get_type_in_variant_container_details(const transaction& tx, extra_attachment_info& eai);
  bool construct_tx(const account_keys& sender_account_keys, 
    const std::vector<tx_source_entry>& sources, 
    const std::vector<tx_destination_entry>& destinations, 
    const std::vector<attachment_v>& attachments,
    transaction& tx, 
    uint64_t unlock_time, 
    uint8_t tx_outs_attr = CURRENCY_TO_KEY_OUT_RELAXED);
  bool construct_tx(const account_keys& sender_account_keys, 
    const std::vector<tx_source_entry>& sources, 
    const std::vector<tx_destination_entry>& destinations,
    const std::vector<extra_v>& extra,
    const std::vector<attachment_v>& attachments,
    transaction& tx, 
    crypto::secret_key& one_time_secrete_key,
    uint64_t unlock_time,
    uint8_t tx_outs_attr = CURRENCY_TO_KEY_OUT_RELAXED);
  bool sign_update_alias(alias_info& ai, const crypto::public_key& pkey, const crypto::secret_key& skey);
  bool make_tx_extra_alias_entry(std::vector<uint8_t>& buff, const alias_info& alinfo, bool make_buff_to_sign = false);
  bool make_tx_extra_alias_entry(std::string& buff, const alias_info& alinfo, bool make_buff_to_sign = false);
  bool add_tx_extra_alias(transaction& tx, const alias_info& alinfo);
  bool parse_and_validate_tx_extra(const transaction& tx, tx_extra_info& extra);
  bool parse_and_validate_tx_extra(const transaction& tx, crypto::public_key& tx_pub_key);
  crypto::public_key get_tx_pub_key_from_extra(const transaction& tx);
  bool add_tx_pub_key_to_extra(transaction& tx, const crypto::public_key& tx_pub_key);
  bool add_tx_extra_userdata(transaction& tx, const blobdata& extra_nonce);
  bool is_out_to_acc(const account_keys& acc, const txout_to_key& out_key, const crypto::public_key& tx_pub_key, size_t output_index);
  bool lookup_acc_outs(const account_keys& acc, const transaction& tx, const crypto::public_key& tx_pub_key, std::vector<size_t>& outs, uint64_t& money_transfered);
  bool lookup_acc_outs(const account_keys& acc, const transaction& tx, std::vector<size_t>& outs, uint64_t& money_transfered);
  bool get_tx_fee(const transaction& tx, uint64_t & fee);
  uint64_t get_tx_fee(const transaction& tx);
  bool generate_key_image_helper(const account_keys& ack, const crypto::public_key& tx_public_key, size_t real_output_index, keypair& in_ephemeral, crypto::key_image& ki);
  void get_blob_hash(const blobdata& blob, crypto::hash& res);
  crypto::hash get_blob_hash(const blobdata& blob);
  std::string short_hash_str(const crypto::hash& h);
  bool is_mixattr_applicable_for_fake_outs_counter(uint8_t mix_attr, uint64_t fake_attr_count);
  bool is_tx_spendtime_unlocked(uint64_t unlock_time, uint64_t current_blockchain_height);
  bool decrypt_attachments(const transaction& tx, const account_keys& acc_keys, std::vector<attachment_v>& decrypted_att);
  void encrypt_attachments(transaction& tx, const account_public_address& destination_add, const keypair& tx_random_key);

  crypto::hash get_transaction_hash(const transaction& t);
  bool get_transaction_hash(const transaction& t, crypto::hash& res);
  //bool get_transaction_hash(const transaction& t, crypto::hash& res, size_t& blob_size);
  blobdata get_block_hashing_blob(const block& b);
  bool get_block_hash(const block& b, crypto::hash& res);
  crypto::hash get_block_hash(const block& b);
  bool generate_genesis_block(block& bl);
  bool parse_and_validate_block_from_blob(const blobdata& b_blob, block& b);
  bool get_inputs_money_amount(const transaction& tx, uint64_t& money);
  uint64_t get_outs_money_amount(const transaction& tx);
  bool check_inputs_types_supported(const transaction& tx);
  bool check_outs_valid(const transaction& tx);
  blobdata get_block_hashing_blob(const block& b);
  bool parse_amount(uint64_t& amount, const std::string& str_amount);
  bool parse_payment_id_from_hex_str(const std::string& payment_id_str, crypto::hash& payment_id);
  void get_block_longhash(const block& b, crypto::hash& res);
  crypto::hash get_block_longhash(const block& b);


  bool check_money_overflow(const transaction& tx);
  bool check_outs_overflow(const transaction& tx);
  bool check_inputs_overflow(const transaction& tx);
  uint64_t get_block_height(const block& b);
  std::vector<uint64_t> relative_output_offsets_to_absolute(const std::vector<uint64_t>& off);
  std::vector<uint64_t> absolute_output_offsets_to_relative(const std::vector<uint64_t>& off);
  std::string print_money(uint64_t amount);
  
  bool addendum_to_hexstr(const std::vector<crypto::hash>& add, std::string& hex_buff);
  bool hexstr_to_addendum(const std::string& hex_buff, std::vector<crypto::hash>& add);
  bool set_payment_id_to_tx_extra(std::vector<extra_v>& extra, const std::string& payment_id);
  bool get_payment_id_from_tx_extra(const transaction& tx, std::string& payment_id);
  crypto::hash get_blob_longhash(const blobdata& bd);
  bool add_padding_to_tx(transaction& tx, size_t count);
  bool is_service_tx(const transaction& tx);
  bool is_anonymous_tx(const transaction& tx);
  //std::string get_comment_from_tx(const transaction& tx);
  std::string print_stake_kernel_info(const stake_kernel& sk);
  //PoS
  bool is_pos_block(const block& b);
  bool is_pos_block(const transaction& tx);
  uint64_t get_coinday_weight(uint64_t amount);
  wide_difficulty_type correct_difficulty_with_sequence_factor(size_t sequence_factor, wide_difficulty_type diff);
  blobdata make_cancel_offer_sig_blob(const cancel_offer& co);
  void print_currency_details();


  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  template<class t_array>
  struct array_hasher : std::unary_function<t_array&, std::size_t>
  {
    std::size_t operator()(const t_array& val) const
    {
      return boost::hash_range(&val.data[0], &val.data[sizeof(val.data)]);
    }
  };


#pragma pack(push, 1)
  struct public_address_outer_blob
  {
    uint8_t m_ver;
    account_public_address m_address;
    uint8_t check_sum;
  };
#pragma pack (pop)


  /************************************************************************/
  /* helper functions                                                     */
  /************************************************************************/
  size_t get_max_block_size();
  size_t get_max_tx_size();
  bool get_block_reward(size_t median_size, size_t current_block_size, uint64_t already_generated_coins, uint64_t &reward, uint64_t height, const wide_difficulty_type& pos_diff);
  uint8_t get_account_address_checksum(const public_address_outer_blob& bl);
  std::string get_account_address_as_str(const account_public_address& adr);
  bool get_account_address_from_str(account_public_address& adr, const std::string& str);
  bool is_coinbase(const transaction& tx);

  bool operator ==(const currency::transaction& a, const currency::transaction& b);
  bool operator ==(const currency::block& a, const currency::block& b);
  //---------------------------------------------------------------
  template<typename specic_type_t, typename variant_t>
  bool get_type_in_variant_container(const std::vector<variant_t>& av, specic_type_t& a)
  {
    for (auto& ai : av)
    {
      if (ai.type() == typeid(specic_type_t))
      {
        a = boost::get<specic_type_t>(ai);
        return true;
      }
    }
    return false;
  }
  //---------------------------------------------------------------
  template<typename specic_type_t, typename variant_t>
  bool have_type_in_variant_container(const std::vector<variant_t>& av)
  {
    for (auto& ai : av)
    {
      if (ai.type() == typeid(specic_type_t))
      {
        return true;
      }
    }
    return false;
  }
  template<class extra_t>
  extra_t& get_or_add_field_to_extra(std::vector<extra_v>& extra)
  {
    for (auto& ev : extra)
    {
      if (ev.type() == typeid(extra_t))
        return boost::get<extra_t>(ev);
    }
    extra.push_back(extra_t());
    return boost::get<extra_t>(extra.back());
  }
  //---------------------------------------------------------------
  template<class payment_id_type>
  bool set_payment_id_to_tx_extra(std::vector<extra_v>& extra, const payment_id_type& payment_id)
  {
    std::string payment_id_blob;
    epee::string_tools::apped_pod_to_strbuff(payment_id_blob, payment_id);
    return set_payment_id_to_tx_extra(extra, payment_id_blob);
  }
  //---------------------------------------------------------------
  template<class payment_id_type>
  bool get_payment_id_from_tx_extra(const transaction& tx, payment_id_type& payment_id)
  {
     std::string payment_id_blob;
     if(!get_payment_id_from_tx_extra(tx, payment_id_blob))
       return false;

     if(payment_id_blob.size() != sizeof(payment_id_type))
       return false;
     payment_id = *reinterpret_cast<const payment_id_type*>(payment_id_blob.data());
     return true;
  }
  //---------------------------------------------------------------
  template<class t_object>
  bool t_serializable_object_to_blob(const t_object& to, blobdata& b_blob)
  {
    std::stringstream ss;
    binary_archive<true> ba(ss);
    bool r = ::serialization::serialize(ba, const_cast<t_object&>(to));
    b_blob = ss.str();
    return r;
  }
  //---------------------------------------------------------------
  template<class t_object>
  blobdata t_serializable_object_to_blob(const t_object& to)
  {
    blobdata b;
    t_serializable_object_to_blob(to, b);
    return b;
  }
  //---------------------------------------------------------------
  template<class t_object>
  bool get_object_hash(const t_object& o, crypto::hash& res)
  {
    get_blob_hash(t_serializable_object_to_blob(o), res);
    return true;
  }
  //---------------------------------------------------------------
  template<class t_object>
  crypto::hash get_object_hash(const t_object& o)
  {
    crypto::hash h;
    get_object_hash(o, h);
    return h;
  }
  //---------------------------------------------------------------

  template<class t_object>
  size_t get_object_blobsize(const t_object& o)
  {
    blobdata b = t_serializable_object_to_blob(o);
    return b.size();
  }
  //---------------------------------------------------------------
  size_t get_object_blobsize(const transaction& t);
  //---------------------------------------------------------------
  template<class t_object>
  bool get_object_hash(const t_object& o, crypto::hash& res, size_t& blob_size)
  {
    blobdata bl = t_serializable_object_to_blob(o);
    blob_size = bl.size();
    get_blob_hash(bl, res);
    return true;
  }
  //---------------------------------------------------------------
  template <typename T>
  std::string obj_to_json_str(T& obj)
  {
    std::stringstream ss;
    json_archive<true> ar(ss, true);
    bool r = ::serialization::serialize(ar, obj);
    CHECK_AND_ASSERT_MES(r, "", "obj_to_json_str failed: serialization::serialize returned false");
    return ss.str();
  }
  //---------------------------------------------------------------
  // 62387455827 -> 455827 + 7000000 + 80000000 + 300000000 + 2000000000 + 60000000000, where 455827 <= dust_threshold
  template<typename chunk_handler_t, typename dust_handler_t>
  void decompose_amount_into_digits(uint64_t amount, uint64_t dust_threshold, const chunk_handler_t& chunk_handler, const dust_handler_t& dust_handler)
  {
    if (0 == amount)
    {
      return;
    }

    bool is_dust_handled = false;
    uint64_t dust = 0;
    uint64_t order = 1;
    while (0 != amount)
    {
      uint64_t chunk = (amount % 10) * order;
      amount /= 10;
      order *= 10;

      if (dust + chunk <= dust_threshold)
      {
        dust += chunk;
      }
      else
      {
        if (!is_dust_handled && 0 != dust)
        {
          dust_handler(dust);
          is_dust_handled = true;
        }
        if (0 != chunk)
        {
          chunk_handler(chunk);
        }
      }
    }

    if (!is_dust_handled && 0 != dust)
    {
      dust_handler(dust);
    }
  }

  blobdata block_to_blob(const block& b);
  bool block_to_blob(const block& b, blobdata& b_blob);
  blobdata tx_to_blob(const transaction& b);
  bool tx_to_blob(const transaction& b, blobdata& b_blob);
  void get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes, crypto::hash& h);
  crypto::hash get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes);
  crypto::hash get_tx_tree_hash(const block& b);

#define CHECKED_GET_SPECIFIC_VARIANT(variant_var, specific_type, variable_name, fail_return_val) \
  CHECK_AND_ASSERT_MES(variant_var.type() == typeid(specific_type), fail_return_val, "wrong variant type: " << variant_var.type().name() << ", expected " << typeid(specific_type).name()); \
  specific_type& variable_name = boost::get<specific_type>(variant_var);

}

template <class T>
std::ostream &print256(std::ostream &o, const T &v) {
  return o << "<" << epee::string_tools::pod_to_hex(v) << ">";
}

bool parse_hash256(const std::string str_hash, crypto::hash& hash);

namespace crypto {
  inline std::ostream &operator <<(std::ostream &o, const crypto::public_key &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::secret_key &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::key_derivation &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::key_image &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::signature &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::hash &v) { return print256(o, v); }
}
