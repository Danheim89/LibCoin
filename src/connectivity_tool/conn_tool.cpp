// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.



#include "include_base_utils.h"
#include "version.h"

using namespace epee;
#include <boost/regex.hpp>
#include <boost/program_options.hpp>
#include "p2p/p2p_protocol_defs.h"
#include "common/command_line.h"
#include "currency_core/currency_core.h"
#include "currency_protocol/currency_protocol_handler.h"
#include "net/levin_client.h"
#include "storages/levin_abstract_invoke2.h"
#include "currency_core/currency_core.h"
#include "storages/portable_storage_template_helper.h"
#include "crypto/crypto.h"
#include "storages/http_abstract_invoke.h"
#include "net/http_client.h"
#include "md5_l.h"

namespace po = boost::program_options;
using namespace currency;
using namespace nodetool;

namespace
{
  const command_line::arg_descriptor<std::string> arg_ip                 = {"ip", "set ip"};
  const command_line::arg_descriptor<size_t>      arg_port               = {"port", "set port"};
  const command_line::arg_descriptor<size_t>      arg_rpc_port           = {"rpc_port", "set rpc port", RPC_DEFAULT_PORT};
  const command_line::arg_descriptor<uint32_t>    arg_timeout            = {"timeout", "set timeout"};
  const command_line::arg_descriptor<std::string> arg_priv_key           = {"private_key", "private key to subscribe debug command", "", true};
  const command_line::arg_descriptor<uint64_t>    arg_peer_id            = {"peer_id", "peer_id if known(if not - will be requested)", 0};
  const command_line::arg_descriptor<bool>        arg_generate_keys      = {"generate_keys_pair", "generate private and public keys pair"};
  const command_line::arg_descriptor<bool>        arg_request_stat_info  = {"request_stat_info", "request statistics information"};
  const command_line::arg_descriptor<bool>        arg_request_net_state  = {"request_net_state", "request network state information (peer list, connections count)"};
  const command_line::arg_descriptor<bool>        arg_get_daemon_info    = {"rpc_get_daemon_info", "request daemon state info vie rpc (--rpc_port option should be set ).", "", true};
  const command_line::arg_descriptor<bool>        arg_get_aliases        = {"rpc_get_aliases", "request daemon aliases all list", "", true};
  const command_line::arg_descriptor<std::string> arg_upate_maintainers_info = {"upate_maintainers_info", "Push maintainers info into the network, upate_maintainers_info=file_with_info.json", "", true};
  const command_line::arg_descriptor<std::string> arg_update_build_no    = {"update_build_no", "Updated version number in version template file", "", true};
  const command_line::arg_descriptor<std::string> arg_pack_file          = {"pack_file", "Pack(using gzip) and calculate md5 hash for file", "", true };
  const command_line::arg_descriptor<std::string> arg_unpack_file        = { "unpack_file", "UnPack(using gzip) and calculate md5 hash", "", true };
  const command_line::arg_descriptor<std::string> arg_target_file        = { "target_file", "Specify target file for pack and upack commands", "", true };
  
  const command_line::arg_descriptor<std::string> arg_sort_sources = { "sort_sources", "", "", true };
}

typedef COMMAND_REQUEST_STAT_INFO_T<t_currency_protocol_handler<core>::stat_info> COMMAND_REQUEST_STAT_INFO;

struct response_schema
{
  std::string status;
  std::string COMMAND_REQUEST_STAT_INFO_status;
  std::string COMMAND_REQUEST_NETWORK_STATE_status;
  enableable<COMMAND_REQUEST_STAT_INFO::response> si_rsp;
  enableable<COMMAND_REQUEST_NETWORK_STATE::response> ns_rsp;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(status)
    KV_SERIALIZE(COMMAND_REQUEST_STAT_INFO_status)
    KV_SERIALIZE(COMMAND_REQUEST_NETWORK_STATE_status)
    KV_SERIALIZE(si_rsp)
    KV_SERIALIZE(ns_rsp)
  END_KV_SERIALIZE_MAP() 
};

  std::string get_response_schema_as_json(response_schema& rs)
  {
    std::stringstream ss;
    ss << "{" << ENDL 
       << "  \"status\": \"" << rs.status << "\"," << ENDL
       << "  \"COMMAND_REQUEST_NETWORK_STATE_status\": \"" << rs.COMMAND_REQUEST_NETWORK_STATE_status << "\"," << ENDL
       << "  \"COMMAND_REQUEST_STAT_INFO_status\": \"" << rs.COMMAND_REQUEST_STAT_INFO_status <<  "\"";
    if(rs.si_rsp.enabled)
    {
      ss << "," << ENDL << "  \"si_rsp\": " <<  epee::serialization::store_t_to_json(rs.si_rsp.v, 1);
    }
    if(rs.ns_rsp.enabled)
    {
      ss << "," << ENDL << "  \"ns_rsp\": {" << ENDL
        << "    \"local_time\": " << rs.ns_rsp.v.local_time << "," << ENDL
        << "    \"my_id\": \"" << rs.ns_rsp.v.my_id << "\"," << ENDL;

      if (!rs.ns_rsp.v.connections_list.empty())
      {
        ss << "    \"connections_list\": [" << ENDL;
        size_t i = 0;
        for (auto& ce : rs.ns_rsp.v.connections_list)
        {
          ss << "      {"
            "\"peer_id\": \"" << ce.id << "\", "
            "\"ip\": \"" << string_tools::get_ip_string_from_int32(ce.adr.ip) << "\", "
            "\"port\": " << ce.adr.port << ", "
            "\"is_income\": " << ce.is_income << "}";
          if (rs.ns_rsp.v.connections_list.size() - 1 != i)
            ss << ",";
          ss << ENDL;
          ++i;
        }
        ss << "    ]," << ENDL;
      }

      if (!rs.ns_rsp.v.connections_list_2.empty())
      {
        ss << "    \"connections_list_2\": [" << ENDL;
        size_t i = 0;
        for (auto& ce : rs.ns_rsp.v.connections_list_2)
        {
          ss << "      {"
            "\"peer_id\": \"" << ce.id << "\", "
            "\"ip\": \"" << string_tools::get_ip_string_from_int32(ce.adr.ip) << "\", "
            "\"port\": " << ce.adr.port << ", "
            "\"time_started\": " << ce.time_started << ", "
            "\"last_recv\": " << ce.last_recv << ", "
            "\"last_send\": " << ce.last_send << ", "
            "\"version\": \"" << ce.version << "\", "
            "\"is_income\": " << ce.is_income << "}";
          if (rs.ns_rsp.v.connections_list_2.size() - 1 != i)
            ss << ",";
          ss << ENDL;
          ++i;
        }
        ss << "    ]," << ENDL;
      }

      size_t i = 0;
      ss << "    \"local_peerlist_white\": [" << ENDL;      
      i = 0;
      BOOST_FOREACH(const peerlist_entry& pe, rs.ns_rsp.v.local_peerlist_white)
      {
        ss <<  "      {\"peer_id\": \"" << pe.id << "\", \"ip\": \"" << string_tools::get_ip_string_from_int32(pe.adr.ip) << "\", \"port\": " << pe.adr.port << ", \"last_seen\": "<< rs.ns_rsp.v.local_time - pe.last_seen << "}";
        if(rs.ns_rsp.v.local_peerlist_white.size()-1 != i)
          ss << ",";
        ss << ENDL; 
        i++;
      }
      ss << "    ]," << ENDL;

      ss << "    \"local_peerlist_gray\": [" << ENDL;      
      i = 0;
      BOOST_FOREACH(const peerlist_entry& pe, rs.ns_rsp.v.local_peerlist_gray)
      {
        ss <<  "      {\"peer_id\": \"" << pe.id << "\", \"ip\": \"" << string_tools::get_ip_string_from_int32(pe.adr.ip) << "\", \"port\": " << pe.adr.port << ", \"last_seen\": "<< rs.ns_rsp.v.local_time - pe.last_seen << "}";
        if(rs.ns_rsp.v.local_peerlist_gray.size()-1 != i)
          ss << ",";
        ss << ENDL; 
        i++;
      }
      ss << "    ]" << ENDL << "  }" << ENDL;
    }
    ss << "}";
    return ss.str();
  }
//---------------------------------------------------------------------------------------------------------------
bool print_COMMAND_REQUEST_STAT_INFO(const COMMAND_REQUEST_STAT_INFO::response& si)
{
  std::cout << " ------ COMMAND_REQUEST_STAT_INFO ------ " << ENDL;
  std::cout << "Version:             " << si.version << ENDL;
  std::cout << "Connections:          " << si.connections_count << ENDL;
  std::cout << "INC Connections:     " << si.incoming_connections_count << ENDL;


  std::cout << "Tx pool size:        " << si.payload_info.tx_pool_size << ENDL;
  std::cout << "BC height:           " << si.payload_info.blockchain_height << ENDL;
  std::cout << "Mining speed:          " << si.payload_info.mining_speed << ENDL;
  std::cout << "Alternative blocks:  " << si.payload_info.alternative_blocks << ENDL;
  std::cout << "Top block id:        " << si.payload_info.top_block_id_str << ENDL;
  return true;
}
//---------------------------------------------------------------------------------------------------------------
bool print_COMMAND_REQUEST_NETWORK_STATE(const COMMAND_REQUEST_NETWORK_STATE::response& ns)
{
  std::cout << " ------ COMMAND_REQUEST_NETWORK_STATE ------ " << ENDL;
  std::cout << "Peer id: " << ns.my_id << ENDL;
  std::cout << "Active connections:"  << ENDL;
  BOOST_FOREACH(const connection_entry& ce, ns.connections_list)
  {
    std::cout <<  ce.id << "\t" << string_tools::get_ip_string_from_int32(ce.adr.ip) << ":" << ce.adr.port << (ce.is_income ? "(INC)":"(OUT)") << ENDL; 
  }
  
  std::cout << "Peer list white:" << ns.my_id << ENDL;
  BOOST_FOREACH(const peerlist_entry& pe, ns.local_peerlist_white)
  {
    std::cout <<  pe.id << "\t" << string_tools::get_ip_string_from_int32(pe.adr.ip) << ":" << pe.adr.port <<  "\t" << misc_utils::get_time_interval_string(ns.local_time - pe.last_seen) << ENDL; 
  }

  std::cout << "Peer list gray:" << ns.my_id << ENDL;
  BOOST_FOREACH(const peerlist_entry& pe, ns.local_peerlist_gray)
  {
    std::cout <<  pe.id << "\t" << string_tools::get_ip_string_from_int32(pe.adr.ip) << ":" << pe.adr.port <<  "\t" << misc_utils::get_time_interval_string(ns.local_time - pe.last_seen) << ENDL; 
  }


  return true;
}
//---------------------------------------------------------------------------------------------------------------
bool handle_get_aliases(po::variables_map& vm)
{
  if(!command_line::has_arg(vm, arg_rpc_port))
  {
    std::cout << "{" << ENDL << "  \"status\": \"ERROR: " << "RPC port not set \"" << ENDL << "}";
    return false;
  }

  epee::net_utils::http::http_simple_client http_client;

  currency::COMMAND_RPC_GET_ALL_ALIASES::request req = AUTO_VAL_INIT(req);
  currency::COMMAND_RPC_GET_ALL_ALIASES::response res = AUTO_VAL_INIT(res);
  std::string daemon_addr = command_line::get_arg(vm, arg_ip) + ":" + std::to_string(command_line::get_arg(vm, arg_rpc_port));
  bool r = epee::net_utils::invoke_http_json_rpc(daemon_addr + "/json_rpc", "get_all_alias_details", req, res, http_client, command_line::get_arg(vm, arg_timeout));
  if(!r)
  {
    std::cout << "{" << ENDL << "  \"status\": \"ERROR: " << "Failed to invoke request \"" << ENDL << "}";
    return false;
  }
  std::cout << epee::serialization::store_t_to_json(res);

  return true;
}
//---------------------------------------------------------------------------------------------------------------
bool handle_get_daemon_info(po::variables_map& vm)
{
  if(!command_line::has_arg(vm, arg_rpc_port))
  {
    std::cout << "ERROR: rpc port not set" << ENDL;
    return false;
  }

  epee::net_utils::http::http_simple_client http_client;

  currency::COMMAND_RPC_GET_INFO::request req = AUTO_VAL_INIT(req);
  currency::COMMAND_RPC_GET_INFO::response res = AUTO_VAL_INIT(res);
  std::string daemon_addr = command_line::get_arg(vm, arg_ip) + ":" + std::to_string(command_line::get_arg(vm, arg_rpc_port));
  bool r = net_utils::invoke_http_json_remote_command2(daemon_addr + "/getinfo", req, res, http_client, command_line::get_arg(vm, arg_timeout));
  if(!r)
  {
    std::cout << "ERROR: failed to invoke request" << ENDL;
    return false;
  }
  std::cout << "OK" << ENDL
  << "height: " << res.height << ENDL
  << "difficulty: " << res.difficulty << ENDL
  << "tx_count: " << res.tx_count << ENDL
  << "tx_pool_size: " << res.tx_pool_size << ENDL
  << "alt_blocks_count: " << res.alt_blocks_count << ENDL
  << "outgoing_connections_count: " << res.outgoing_connections_count << ENDL
  << "incoming_connections_count: " << res.incoming_connections_count << ENDL
  << "white_peerlist_size: " << res.white_peerlist_size << ENDL
  << "grey_peerlist_size: " << res.grey_peerlist_size << ENDL
  << "current_network_hashrate_50: " << res.current_network_hashrate_50 << ENDL
  << "current_network_hashrate_350: " << res.current_network_hashrate_350 << ENDL
  << "scratchpad_size: " << res.scratchpad_size << ENDL
  << "alias_count: " << res.alias_count << ENDL
  << "transactions_cnt_per_day: " << res.transactions_cnt_per_day << ENDL
  << "transactions_volume_per_day: " << res.transactions_volume_per_day << ENDL;
  return true;
}
//---------------------------------------------------------------------------------------------------------------
bool handle_request_stat(po::variables_map& vm, peerid_type peer_id)
{

  if(!command_line::has_arg(vm, arg_priv_key))
  {
    std::cout << "{" << ENDL << "  \"status\": \"ERROR: " << "secret key not set \"" << ENDL << "}";
    return false;
  }
  crypto::secret_key prvk = AUTO_VAL_INIT(prvk);
  if(!string_tools::hex_to_pod(command_line::get_arg(vm, arg_priv_key) , prvk))
  {
    std::cout << "{" << ENDL << "  \"status\": \"ERROR: " << "wrong secret key set \"" << ENDL << "}";
    return false;
  }


  response_schema rs = AUTO_VAL_INIT(rs);

  levin::levin_client_impl2 transport;
  if(!transport.connect(command_line::get_arg(vm, arg_ip), static_cast<int>(command_line::get_arg(vm, arg_port)), static_cast<int>(command_line::get_arg(vm, arg_timeout))))
  {
    std::cout << "{" << ENDL << "  \"status\": \"ERROR: " << "Failed to connect to " << command_line::get_arg(vm, arg_ip) << ":" << command_line::get_arg(vm, arg_port) << "\"" << ENDL << "}";
    return false;
  }else
    rs.status = "OK";

  if(!peer_id)
  {
    COMMAND_REQUEST_PEER_ID::request req = AUTO_VAL_INIT(req);
    COMMAND_REQUEST_PEER_ID::response rsp = AUTO_VAL_INIT(rsp);
    if(!net_utils::invoke_remote_command2(COMMAND_REQUEST_PEER_ID::ID, req, rsp, transport))
    {
      std::cout << "{" << ENDL << "  \"status\": \"ERROR: " << "Failed to connect to " << command_line::get_arg(vm, arg_ip) << ":" << command_line::get_arg(vm, arg_port) << "\"" << ENDL << "}";
      return false;
    }else
    {
      peer_id = rsp.my_id;
    }
  }


  nodetool::proof_of_trust pot = AUTO_VAL_INIT(pot);
  pot.peer_id = peer_id;
  pot.time = time(NULL);
  crypto::public_key pubk = AUTO_VAL_INIT(pubk);
  if (!crypto::secret_key_to_public_key(prvk, pubk))
  {
    std::cout << "{" << ENDL << "  \"status\": \"ERROR: failed to convert secret key to public\"" << ENDL << "}" << ENDL;
    return false;
  }
  crypto::hash h = tools::get_proof_of_trust_hash(pot);
  crypto::generate_signature(h, pubk, prvk, pot.sign);

  if(command_line::get_arg(vm, arg_request_stat_info))
  {
    COMMAND_REQUEST_STAT_INFO::request req = AUTO_VAL_INIT(req);
    req.tr = pot;
    if(!net_utils::invoke_remote_command2(COMMAND_REQUEST_STAT_INFO::ID, req, rs.si_rsp.v, transport))
    {
      std::stringstream ss;
      ss << "ERROR: " << "Failed to invoke remote command COMMAND_REQUEST_STAT_INFO to " << command_line::get_arg(vm, arg_ip) << ":" << command_line::get_arg(vm, arg_port);
      ss << ", pubk: " << pubk << ", sign: " << pot.sign << ", proof_h: " << h;
      rs.COMMAND_REQUEST_STAT_INFO_status = ss.str();
    }else
    {
      rs.si_rsp.enabled = true;
      rs.COMMAND_REQUEST_STAT_INFO_status = "OK";
    }
  }


  if(command_line::get_arg(vm, arg_request_net_state))
  {
    ++pot.time;
    h = tools::get_proof_of_trust_hash(pot);
    crypto::generate_signature(h, pubk, prvk, pot.sign);
    COMMAND_REQUEST_NETWORK_STATE::request req = AUTO_VAL_INIT(req);    
    req.tr = pot;
    if(!net_utils::invoke_remote_command2(COMMAND_REQUEST_NETWORK_STATE::ID, req, rs.ns_rsp.v, transport))
    {
      std::stringstream ss;
      ss << "ERROR: " << "Failed to invoke remote command COMMAND_REQUEST_NETWORK_STATE to " << command_line::get_arg(vm, arg_ip) << ":" << command_line::get_arg(vm, arg_port);
      ss << ", pubk: " << pubk << ", sign: " << pot.sign << ", proof_h: " << h;
      rs.COMMAND_REQUEST_NETWORK_STATE_status = ss.str();
    }else
    {
      rs.ns_rsp.enabled = true;
      rs.COMMAND_REQUEST_NETWORK_STATE_status = "OK";
    }
  }
  std::cout << get_response_schema_as_json(rs);
  return true;
}
//---------------------------------------------------------------------------------------------------------------
bool get_private_key(crypto::secret_key& pk, po::variables_map& vm)
{
  if(!command_line::has_arg(vm, arg_priv_key))
  {
    std::cout << "ERROR: secret key not set" << ENDL;
    return false;
  }
  
  if(!string_tools::hex_to_pod(command_line::get_arg(vm, arg_priv_key) , pk))
  {
    std::cout << "ERROR: wrong secret key set" << ENDL;
    return false;
  }
  crypto::public_key pubkey = AUTO_VAL_INIT(pubkey);
  if(!crypto::secret_key_to_public_key(pk, pubkey))
  {
    std::cout << "ERROR: wrong secret key set(secret_key_to_public_key failed)" << ENDL;
    return false;
  }
  if( pubkey != tools::get_public_key_from_string(P2P_MAINTAINERS_PUB_KEY))
  {
    std::cout << "ERROR: wrong secret key set(public keys not match)" << ENDL;
    return false;
  }
  return true;
}
//---------------------------------------------------------------------------------------------------------------
template<class archive_processor_t>
bool process_archive(archive_processor_t& arch_processor, bool is_md5_from_source, std::ifstream& source, std::ofstream& target)
{
  source.seekg(0, std::ios::end);
  uint64_t sz = source.tellg();
  uint64_t remainin = sz;
  uint64_t packed_size = 0;

  //MD5 stream calculator
  md5::MD5_CTX md5_state = AUTO_VAL_INIT(md5_state);
  md5::MD5Init(&md5_state);


  source.seekg(0, std::ios::beg);
#define PACK_READ_BLOCKS_SIZE  1048576 // 1MB blocks
  std::string buff;

  auto writer_cb = [&](const std::string& piece_of_transfe)
  {
    target.write(piece_of_transfe.data(), piece_of_transfe.size());
    packed_size += piece_of_transfe.size();
    if (!is_md5_from_source)
    {
      //update MD5 state
      md5::MD5Update(&md5_state, reinterpret_cast<const unsigned char *>(piece_of_transfe.data()), piece_of_transfe.size());
    }
    return true;
  };

  while (remainin)
  {
    uint64_t read_sz = remainin >= PACK_READ_BLOCKS_SIZE ? PACK_READ_BLOCKS_SIZE : remainin;
    buff.resize(read_sz);
    source.read(const_cast<char*>(buff.data()), buff.size());
    if (!source)
    {
      std::cout << "Error on read from source" << ENDL;
      return true;
    }
    if (is_md5_from_source)
    {
      //update MD5 state
      md5::MD5Update(&md5_state, reinterpret_cast<const unsigned char *>(buff.data()), buff.size());
    }

    arch_processor.update_in(buff, writer_cb);

    remainin -= read_sz;
    std::cout << "Progress " << ((sz - remainin) * 100) / sz << "%\r";
  }

  //flush gzip decoder
  arch_processor.stop(writer_cb);

  source.close();
  target.close();

  unsigned char md5_signature[16] = { 0 };
  md5::MD5Final(md5_signature, &md5_state);

  std::cout << "\r\nFile packed from size " << sz << " to " << packed_size <<
    "\r\nMD5 of source file is " << epee::string_tools::pod_to_hex(md5_signature) << "\r\n";
  return true;

}

//---------------------------------------------------------------------------------------------------------------

bool handle_pack_file(po::variables_map& vm)
{
  bool do_pack = false;
  std::string path_source;
  std::string path_target;
  if (command_line::has_arg(vm, arg_pack_file))
  {
    path_source = command_line::get_arg(vm, arg_pack_file);
    do_pack = true;
  }
  else if (command_line::has_arg(vm, arg_unpack_file))
  {
    path_source = command_line::get_arg(vm, arg_unpack_file);
    do_pack = false;
  }

  if (!command_line::has_arg(vm, arg_target_file))
    std::cout << "Error: Parameter target_file not set." << ENDL;
  path_target = command_line::get_arg(vm, arg_target_file);

  std::ifstream source;
  std::ofstream target;
  source.open(path_source, std::ios::binary | std::ios::in );
  target.open(path_target, std::ios::binary | std::ios::out | std::ios::trunc);

  if (!source.is_open() || !target.is_open())
  {
    std::cout << "Error: Unable to open " << path_source << " or " << path_source << ENDL;
    return true;
  }
  if (do_pack)
  {
    //gzip packer
    epee::net_utils::gzip_encoder_lyambda gzip_encoder(Z_BEST_COMPRESSION);
    return process_archive(gzip_encoder, true, source, target);
  }else
  {
    //gzip unpacker
    epee::net_utils::gzip_decoder_lambda gzip_decoder;
    return process_archive(gzip_decoder, false, source, target);
  }
}
bool sort_sources(std::string path)
{
  std::string header_buff;
  std::string cpp_buff;
  bool r = file_io_utils::load_file_to_string(path+ ".h", header_buff);
  CHECK_AND_ASSERT_MES(r, false, "failed to load");
   r = file_io_utils::load_file_to_string(path + ".cpp", cpp_buff);
  CHECK_AND_ASSERT_MES(r, false, "failed to load");


  const boost::regex	match_func_definition("^.   ([\\w:<>&]*) ([\\w_]*)\\(([\\w_ :&=<>,\\*]*)\\)( const)?;", boost::regex::icase | boost::regex::normal);
  //
  std::list<std::string> functions;
  //boost::smatch result;

  boost::sregex_token_iterator iter(header_buff.begin(), header_buff.end(), match_func_definition, 2);
  boost::sregex_token_iterator end;



  for (; iter != end; ++iter) 
  {
    functions.push_back(*iter);
  }

  const boost::regex	match_func_body("(\\/\\/-+)?\\n(([\\w:<>&]*) )?([\\w_]+)::([\\w]+)\\(.*?\\n}", boost::regex::icase | boost::regex::normal);
  //
  std::map<std::string, std::string> bodies;
  boost::sregex_iterator iter_body(cpp_buff.begin(), cpp_buff.end(), match_func_body);
  boost::sregex_iterator end_iter_body;

  for (; iter_body != end_iter_body; ++iter_body)
  {
    boost::smatch result = *iter_body;
    bodies[result[4]] = result[0];
  }
  std::string final_buff;
  for (auto& f : functions)
  {
    auto f_map_it = bodies.find(f);
    if (f_map_it == bodies.end())
    {
      std::cout << "Missed " << f << ENDL;
      continue;
    }
    final_buff += "\r\n//------------------------------------------------------" + f_map_it->second;
    bodies.erase(f_map_it);
  }
  return true;
}
//---------------------------------------------------------------------------------------------------------------
bool handle_update_maintainers_info(po::variables_map& vm)
{
  if(!command_line::has_arg(vm, arg_rpc_port))
  {
    std::cout << "ERROR: rpc port not set" << ENDL;
    return false;
  }
  crypto::secret_key prvk = AUTO_VAL_INIT(prvk);
  if(!get_private_key(prvk, vm))
  {
    std::cout << "ERROR: secrete key error" << ENDL;
    return false;
  }
  std::string path = command_line::get_arg(vm, arg_upate_maintainers_info);

  epee::net_utils::http::http_simple_client http_client;

  currency::COMMAND_RPC_SET_MAINTAINERS_INFO::request req = AUTO_VAL_INIT(req);
  currency::COMMAND_RPC_SET_MAINTAINERS_INFO::response res = AUTO_VAL_INIT(res);

  maintainers_info mi = AUTO_VAL_INIT(mi);
  bool r = epee::serialization::load_t_from_json_file(mi, path);
  CHECK_AND_ASSERT_MES(r, false, "Failed to load maintainers_info from json file: " << path);
  mi.timestamp = time(NULL);  
  std::cout << "timestamp: " << mi.timestamp << ENDL;
  epee::serialization::store_t_to_binary(mi, req.maintainers_info_buff);
  crypto::generate_signature(currency::get_blob_hash(req.maintainers_info_buff), tools::get_public_key_from_string(P2P_MAINTAINERS_PUB_KEY), prvk, req.sign);

  std::string daemon_addr = command_line::get_arg(vm, arg_ip) + ":" + std::to_string(command_line::get_arg(vm, arg_rpc_port));
  r = net_utils::invoke_http_bin_remote_command2(daemon_addr + "/set_maintainers_info.bin", req, res, http_client, command_line::get_arg(vm, arg_timeout));
  if(!r)
  {
    std::cout << "ERROR: failed to invoke request" << ENDL;
    return false;
  }
  if(res.status != CORE_RPC_STATUS_OK)
  {
    std::cout << "ERROR: failed to update maintainers info: " << res.status << ENDL;
    return false;
  }

  std::cout << "OK" << ENDL;
  return true;
}
//---------------------------------------------------------------------------------------------------------------
bool generate_and_print_keys()
{
  crypto::public_key pk = AUTO_VAL_INIT(pk);
  crypto::secret_key sk = AUTO_VAL_INIT(sk);
  generate_keys(pk, sk);
  std::cout << "PUBLIC KEY: " << epee::string_tools::pod_to_hex(pk) << ENDL 
    << "PRIVATE KEY: " << epee::string_tools::pod_to_hex(sk);
  return true;
}
int main(int argc, char* argv[])
{
  string_tools::set_module_name_and_folder(argv[0]);
  log_space::get_set_log_detalisation_level(true, LOG_LEVEL_0);

  // Declare the supported options.
  po::options_description desc_general("General options");
  command_line::add_arg(desc_general, command_line::arg_help);

  po::options_description desc_params("Connectivity options");
  command_line::add_arg(desc_params, arg_ip);
  command_line::add_arg(desc_params, arg_port);
  command_line::add_arg(desc_params, arg_rpc_port);
  command_line::add_arg(desc_params, arg_timeout);
  command_line::add_arg(desc_params, arg_request_stat_info);
  command_line::add_arg(desc_params, arg_request_net_state);
  command_line::add_arg(desc_params, arg_generate_keys);
  command_line::add_arg(desc_params, arg_peer_id);
  command_line::add_arg(desc_params, arg_priv_key);
  command_line::add_arg(desc_params, arg_get_daemon_info);
  command_line::add_arg(desc_params, arg_get_aliases);
  command_line::add_arg(desc_params, arg_upate_maintainers_info);
  command_line::add_arg(desc_params, arg_pack_file);
  command_line::add_arg(desc_params, arg_unpack_file);
  command_line::add_arg(desc_params, arg_target_file);
  command_line::add_arg(desc_params, arg_sort_sources);
  
  command_line::add_arg(desc_params, command_line::arg_version);
  

  po::options_description desc_all;
  desc_all.add(desc_general).add(desc_params);

  po::variables_map vm;
  bool r = command_line::handle_error_helper(desc_all, [&]()
  {
    po::store(command_line::parse_command_line(argc, argv, desc_general, true), vm);
    if (command_line::get_arg(vm, command_line::arg_help))
    {
      std::cout << desc_all << ENDL;
      return false;
    }

    po::store(command_line::parse_command_line(argc, argv, desc_params, false), vm);
    po::notify(vm);

    return true;
  });
  if (!r)
    return 1;


  if (command_line::get_arg(vm, command_line::arg_version))
  {
    std::cout << CURRENCY_NAME << " v" << PROJECT_VERSION_LONG << ENDL;
    return 0;
  }
  if(command_line::has_arg(vm, arg_request_stat_info) || command_line::has_arg(vm, arg_request_net_state))
  {
    return handle_request_stat(vm, command_line::get_arg(vm, arg_peer_id)) ? 0:1;
  }
  if(command_line::has_arg(vm, arg_get_daemon_info))
  {
    return handle_get_daemon_info(vm) ? 0:1;
  }
  else if(command_line::has_arg(vm, arg_generate_keys))
  {
    return  generate_and_print_keys() ? 0:1;
  }
  else if(command_line::has_arg(vm, arg_get_aliases))
  {
    return handle_get_aliases(vm) ? 0:1;
  }
  else if (command_line::has_arg(vm, arg_pack_file) || command_line::has_arg(vm, arg_unpack_file))
  {
    return handle_pack_file(vm) ? 0 : 1;
  }
  else if(command_line::has_arg(vm, arg_upate_maintainers_info))
  {
    return handle_update_maintainers_info(vm) ? 0:1;
  }
  else if (command_line::has_arg(vm, arg_sort_sources))
  {
    return sort_sources(command_line::get_arg(vm, arg_sort_sources));
  }
  else
  {
    std::cerr << "Not enough arguments." << ENDL;
    std::cerr << desc_all << ENDL;
  }

  return 1;
}

