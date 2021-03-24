
/***************************************************************************
 *  refbox.h - LLSF RefBox main program
 *
 *  Created: Thu Feb 07 11:02:51 2013
 *  Copyright  2013  Tim Niemueller [www.niemueller.de]
 *             2019  Till Hofmann <hofmann@kbsg.rwth-aachen.de>
 ****************************************************************************/

/*  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the authors nor the names of its contributors
 *   may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LLSF_REFBOX_REFBOX_H_
#define __LLSF_REFBOX_REFBOX_H_

#include <boost/asio.hpp>
#include <google/protobuf/message.h>
#include <logging/logger.h>
#include <core/threading/mutex.h>
#include <core/threading/mutex_locker.h>
#include <utils/llsf/machines.h>
#include <protobuf_comm/server.h>
#include <llsf_sps/mps_refbox_interface.h>
#include <core/threading/thread_list.h>

#include <clipsmm.h>
#include <future>

namespace mps_placing_clips {
  class MPSPlacingGenerator;
}
namespace protobuf_clips {
  class ClipsProtobufCommunicator;
}

namespace llsf_sps {
  class SPSComm;
}

#ifdef HAVE_AVAHI
namespace fawkes {
  class AvahiThread;
  class NetworkNameResolver;
  class ServicePublisher;
  class ServiceBrowser;
}
#endif

#ifdef HAVE_MONGODB
#	include <mongocxx/database.hpp>
#	include <mongocxx/client.hpp>
class MongoDBLogProtobuf;
#endif

namespace llsfrb {
#if 0 /* just to make Emacs auto-indent happy */
}
#endif

class Configuration;
class MultiLogger;
class WebviewServer;

class LLSFRefBox
{
 public:
  LLSFRefBox(int argc, char **argv);
  ~LLSFRefBox();

  int run();

  void handle_signal(const boost::system::error_code& error, int signum);

 private: // methods
  void start_timer();
  void handle_timer(const boost::system::error_code& error);

  void setup_protobuf_comm();

  void          start_clips();
  void          setup_clips();
  void          handle_clips_periodic();
  void          setup_clips_mongodb();

  CLIPS::Values clips_now();
  CLIPS::Values clips_get_clips_dirs();
  void          clips_load_config(std::string cfg_prefix);
  CLIPS::Value  clips_config_path_exists(std::string path);
  CLIPS::Value  clips_config_get_bool(std::string path);

#ifdef HAVE_MONGODB
	CLIPS::Value clips_bson_create();
	CLIPS::Value clips_bson_parse(std::string document);
	void         clips_bson_builder_destroy(void *bson);
	void         clips_bson_destroy(void *bson);
	void         clips_bson_append(void *bson, std::string field_name, CLIPS::Value value);
	void         clips_bson_append_array(void *bson, std::string field_name, CLIPS::Values values);
	void         clips_bson_append_time(void *bson, std::string field_name, CLIPS::Values time);
	CLIPS::Value clips_bson_array_start();
	void         clips_bson_array_finish(void *bson, std::string field_name, void *array);
	void         clips_bson_array_append(void *barr, CLIPS::Value value);
	std::string  clips_bson_tostring(void *bson);
	void         clips_mongodb_upsert(std::string collection, void *bson, CLIPS::Value query);
	void         clips_mongodb_update(std::string collection, void *bson, CLIPS::Value query);
	void         clips_mongodb_replace(std::string collection, void *bson, CLIPS::Value query);
	void         clips_mongodb_insert(std::string collection, void *bson);
	void         mongodb_update(std::string &                  collection,
	                            const bsoncxx::document::view &doc,
	                            CLIPS::Value &                 query,
	                            bool                           upsert);
	CLIPS::Value clips_mongodb_query_sort(std::string collection, void *bson, void *bson_sort);
	CLIPS::Value clips_mongodb_query(std::string collection, void *bson);
	//	CLIPS::Value  clips_mongodb_cursor_more(void *cursor);
	CLIPS::Value  clips_mongodb_cursor_next(void *cursor);
	void          clips_mongodb_cursor_destroy(void *cursor);
	CLIPS::Values clips_bson_field_names(void *bson);
	CLIPS::Value  clips_bson_get(void *bson, std::string field_name);
	CLIPS::Values clips_bson_get_array(void *bson, std::string field_name);
	CLIPS::Values clips_bson_get_time(void *bson, std::string field_name);
#endif

  void          clips_sps_set_signal(std::string machine, std::string light, std::string state);
  void          clips_mps_bs_dispense(std::string machine, std::string color, std::string side);
  void          clips_mps_rs_mount_ring(std::string machine, int slide);
  void          clips_mps_cs_process(std::string machine, std::string operation);
  void          clips_mps_ds_process(std::string machine, int slide);
  void          clips_mps_set_light(std::string machine, std::string light, std::string state);
  void          clips_mps_set_lights(std::string machine, std::string red_state,
                                     std::string yellow_state, std::string green_state);
  void          clips_mps_reset(std::string machine);
  void          clips_mps_reset_base_counter(std::string machine);
  void          clips_mps_deliver(std::string machine);
  void          sps_read_rfids();

  void handle_server_client_msg(protobuf_comm::ProtobufStreamServer::ClientID client,
				uint16_t component_id, uint16_t msg_type,
				std::shared_ptr<google::protobuf::Message> msg);
  void handle_server_client_fail(protobuf_comm::ProtobufStreamServer::ClientID client,
				 uint16_t component_id, uint16_t msg_type,
				 std::string msg);
  void handle_peer_msg(boost::asio::ip::udp::endpoint &endpoint,
		       uint16_t component_id, uint16_t msg_type,
		       std::shared_ptr<google::protobuf::Message> msg);

  void handle_server_sent_msg(protobuf_comm::ProtobufStreamServer::ClientID client,
			      std::shared_ptr<google::protobuf::Message> msg);

  void handle_peer_sent_msg(std::shared_ptr<google::protobuf::Message> msg);

  void handle_client_sent_msg(std::string host, unsigned short port,
			      std::shared_ptr<google::protobuf::Message> msg);

#ifdef HAVE_MONGODB
	void add_comp_type(google::protobuf::Message &m, bsoncxx::builder::basic::document *doc);
#endif

 private: // members
  Configuration *config_;
  Logger        *logger_;
  MultiLogger   *clips_logger_;
  Logger::LogLevel log_level_;
  llsf_sps::SPSComm *sps_;
  protobuf_clips::ClipsProtobufCommunicator *pb_comm_;
  std::shared_ptr<mps_placing_clips::MPSPlacingGenerator> mps_placing_generator_;

  CLIPS::Environment                       *clips_;
  //std::recursive_mutex                      clips_mutex_;
  fawkes::Mutex                             clips_mutex_;
  std::map<long int, CLIPS::Fact::pointer>  clips_msg_facts_;

  boost::asio::io_service      io_service_;
  boost::asio::deadline_timer  timer_;
  boost::posix_time::ptime     timer_last_;

  unsigned int cfg_timer_interval_;
  std::string  cfg_clips_dir_;
  llsf_utils::MachineAssignment cfg_machine_assignment_;

#ifdef HAVE_AVAHI
  fawkes::AvahiThread          *avahi_thread_;
#endif
  WebviewServer                *rest_api_thread_;

  fawkes::NetworkNameResolver  *nnresolver_;
  fawkes::ServicePublisher     *service_publisher_;
  fawkes::ServiceBrowser       *service_browser_;


#ifdef HAVE_MONGODB
	bool                cfg_mongodb_enabled_;
	std::string         cfg_mongodb_hostport_;
	std::unique_ptr<MongoDBLogProtobuf> mongodb_protobuf_;
	mongocxx::client    client_;
	mongocxx::database  database_;
#endif

  MPSRefboxInterface  *mps_;
};


} // end of namespace llsfrb

#endif
