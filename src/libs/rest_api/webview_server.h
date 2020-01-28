
/***************************************************************************
 *  webview_thread.h - Thread that handles web interface requests
 *
 *  Created: Mon Oct 13 17:49:52 2008 (I5 Developer's Day)
 *  Copyright  2006-2018  Tim Niemueller [www.niemueller.de]
 *             2020       Mostafa Gomaa  [mostafa.go@gmail.com]
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#ifndef _PLUGINS_WEBVIEW_WEBVIEW_THREAD_H_
#define _PLUGINS_WEBVIEW_WEBVIEW_THREAD_H_

#include <core/threading/thread.h>
#include <functional>
#include <string>
#include <vector>

namespace CLIPS {
class Environment;
}

namespace fawkes {
class NetworkService;
class WebServer;
class WebRequestDispatcher;
class WebReply;

// from fawkes::WebviewAspect
class WebUrlManager;
class WebNavManager;
class WebRequestManager;
class WebviewRestApiManager;

//from fawkes::NetworkAspect
class NetworkNameResolver;
class ServicePublisher;
class ServiceBrowser;
} // namespace fawkes

namespace llsfrb {
class WebviewStaticRequestProcessor;
class WebviewRESTRequestProcessor;
class WebviewServiceBrowseHandler;
class WebviewUserVerifier;
class ClipsRestApi;
class Configuration;
class Logger;

class WebviewServer : public fawkes::Thread
{
public:
	WebviewServer(bool enable_tp
		   ,fawkes::NetworkNameResolver *nnresolver
		   ,fawkes::ServicePublisher         *service_publisher
		   ,fawkes::ServiceBrowser           *service_browser
		   ,fawkes::Mutex                    &clips_mutex
		   ,CLIPS::Environment               *env
		   ,Configuration                    *config
		   ,Logger                           *logger);
	~WebviewServer();

	virtual void init();
	virtual void finalize();
	virtual void loop();

private:
	void              tls_create(const char *tls_key_file, const char *tls_cert_file);
	fawkes::WebReply *produce_404();

	/** Stub to see name in backtrace for easier debugging. @see Thread::run() */
protected:
	virtual void
	run()
	{
		Thread::run();
	}

private:
	fawkes::WebServer *           webserver_;
	fawkes::WebRequestDispatcher *dispatcher_;

	WebviewStaticRequestProcessor *static_processor_;
	WebviewRESTRequestProcessor *  rest_processor_;
	WebviewServiceBrowseHandler *  service_browse_handler_;
	WebviewUserVerifier *          user_verifier_;
	ClipsRestApi *                 clips_rest_api_;

	unsigned int             cfg_port_;
	bool                     cfg_use_ipv4_;
	bool                     cfg_use_ipv6_;
	bool                     cfg_use_tls_;
	bool                     cfg_tls_create_;
	std::string              cfg_tls_key_;
	std::string              cfg_tls_cert_;
	std::string              cfg_tls_cipher_suite_;
	bool                     cfg_use_basic_auth_;
	std::string              cfg_basic_auth_realm_;
	std::string              cfg_access_log_;
	bool                     cfg_use_thread_pool_;
	unsigned int             cfg_num_threads_;
	std::vector<std::string> cfg_explicit_404_;

	fawkes::NetworkService *webview_service_;

	//From fawkes::WebviewAspect
	fawkes::WebUrlManager        *url_manager_;
	fawkes::WebRequestManager    *request_manager_;
	fawkes::WebviewRestApiManager *rest_api_manager_;

	//From fawkes::NetworkAspect
	fawkes::NetworkNameResolver  *nnresolver_;
	fawkes::ServicePublisher     *service_publisher_;
	fawkes::ServiceBrowser       *service_browser_;

	Configuration                * config_;
	Logger                       * logger_;
};
} // namespace llsfrb

#endif
