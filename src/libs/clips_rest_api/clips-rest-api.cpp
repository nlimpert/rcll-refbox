
/***************************************************************************
 *  clips-rest-api.cpp -  CLIPS REST API
 *
 *  Created: Sat Mar 31 01:36:11 2018
 *  Copyright  2006-2018  Tim Niemueller [www.niemueller.de]
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

#include "clips-rest-api.h"

#include <clips/clips.h>
#include <core/threading/mutex_locker.h>
#include <webview/rest_api_manager.h>

using namespace fawkes;

/** @class ClipsRestApi "clips-rest-api.h"
 * REST API backend for CLIPS.
 * @author Tim Niemueller
 */


namespace llsfrb {
/** Constructor. */
ClipsRestApi::ClipsRestApi(CLIPS::Environment *env,
	   fawkes::Mutex &env_mutex,
	   fawkes::WebviewRestApiManager *webview_rest_api_manager,
	   Logger *logger)
	:fawkes::Thread("ClipsRestApi", Thread::OPMODE_CONTINUOUS),
	env_(env),
	env_mutex_(env_mutex)
{
	webview_rest_api_manager_ = webview_rest_api_manager;
	logger_ = logger;


}

/** Destructor. */
ClipsRestApi::~ClipsRestApi()
{
}

void
ClipsRestApi::init()
{
	rest_api_ = new WebviewRestApi("clips", logger_);
	rest_api_->add_handler<WebviewRestArray<Fact>>(WebRequest::METHOD_GET,
	                                               "/facts",
	                                               std::bind(&ClipsRestApi::cb_get_facts,
	                                                         this,
	                                                         std::placeholders::_1));
	rest_api_->add_handler<WebviewRestArray<Environment>>(
	  WebRequest::METHOD_GET, "/", std::bind(&ClipsRestApi::cb_list_environments, this));
	rest_api_->add_handler<WebviewRestArray<Machine>>(
	  WebRequest::METHOD_GET, "/machines", std::bind(&ClipsRestApi::cb_list_machines, this));
	webview_rest_api_manager_->register_api(rest_api_);
}

void
ClipsRestApi::finalize()
{
	webview_rest_api_manager_->unregister_api(rest_api_);
	delete rest_api_;
}

void
ClipsRestApi::loop()
{
}

/** Get a value from a fact.
 * @param fact pointer to CLIPS fact
 * @param slot_name name of field to retrieve
 * @return template-specific return value
 */
template <typename T>
T get_value(const CLIPS::Fact::pointer &fact, const std::string &slot_name)
{
	CLIPS::Values v = fact->slot_value(slot_name);
	if (v.empty()) {
	    throw Exception("No value for slot '%s'", slot_name.c_str());
	}
	if (v[0].type() == CLIPS::TYPE_SYMBOL && v[0].as_string() == "nil") {
	    return T();
	}
	return v[0];
}

/** Specialization for bool.
 * @param fact pointer to CLIPS fact
 * @param slot_name name of field to retrieve
 * @return boolean value
 */
template <>
bool get_value(const CLIPS::Fact::pointer &fact, const std::string &slot_name)
{
    CLIPS::Values v = fact->slot_value(slot_name);
    if (v.empty()) {
	   throw Exception("No value for slot '%s'", slot_name.c_str());
    }
    if (v[0].type() != CLIPS::TYPE_SYMBOL) {
	   throw Exception("Value for slot '%s' is not a boolean", slot_name.c_str());
    }
    return (v[0].as_string() == "TRUE");
}

/** Get value array.
 * This is not a template because the overly verbose operator API
 * of CLIPS::Value can lead to ambiguous overloads, e.g., resolving
 * std::string to std::string or const char * operators.
 * @param fact pointer to CLIPS fact
 * @param slot_name name of field to retrieve
 * @return vector of strings from multislot
 */
static std::vector<std::string>
get_values(const CLIPS::Fact::pointer &fact, const std::string &slot_name)
{
    CLIPS::Values v = fact->slot_value(slot_name);
    std::vector<std::string> rv(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
	   rv[i] = static_cast<std::string&>(v[i]);
    }
    return rv;
}




Fact
ClipsRestApi::gen_fact(CLIPS::Fact::pointer &       fact,
                       bool                         formatted)
{
	Fact retf;
	retf.set_kind("Fact");
	retf.set_apiVersion(Fact::api_version());
	retf.set_index(fact->index());
	CLIPS::Template::pointer fact_template = fact->get_template();
	if (fact_template) {
		retf.set_template_name(fact_template->name());
	} else {
		retf.set_template_name("implied");
	}

	if (formatted) {
		char tmp[16384];
		tmp[16383] = 0;
		OpenStringDestination(env_->cobj(), (char *)"ProcPPForm", tmp, 16383);
		PrintFact(env_->cobj(), (char *)"ProcPPForm", (struct fact *)fact->cobj(), FALSE, FALSE);
		CloseStringDestination(env_->cobj(), (char *)"ProcPPForm");
		retf.set_formatted(tmp);
	} else {
		std::vector<std::string> slots = fact->slot_names();
		for (const auto &s : slots) {
			CLIPS::Values fval = fact->slot_value(s);
			SlotValue     sval;
			sval.set_name(s);
			sval.set_is_multifield(fact_template ? fact_template->is_multifield_slot(s)
			                                     : (fval.size() > 1));
			for (const auto &v : fval) {
				switch (v.type()) {
				case CLIPS::TYPE_FLOAT: sval.addto_values(std::to_string(v.as_float())); break;
				case CLIPS::TYPE_INTEGER: sval.addto_values(std::to_string(v.as_integer())); break;
				case CLIPS::TYPE_SYMBOL:
				case CLIPS::TYPE_STRING:
				case CLIPS::TYPE_INSTANCE_NAME: sval.addto_values(v.as_string()); break;
				default: sval.addto_values("ADDR"); break;
				}
			}
			retf.addto_slots(std::move(sval));
		}
	}

	return retf;
}



WebviewRestArray<Fact>
ClipsRestApi::cb_get_facts(WebviewRestParams &params)
{
	bool formatted = (params.query_arg("formatted") == "true");

	WebviewRestArray<Fact> rv;

	MutexLocker lock(&env_mutex_);
	CLIPS::Fact::pointer fact = env_->get_facts();
	while (fact) {
		CLIPS::Template::pointer tmpl = fact->get_template();
		rv.push_back(std::move(gen_fact(fact, formatted)));
		fact = fact->next();
	}

	return rv;
}

WebviewRestArray<Environment>
ClipsRestApi::cb_list_environments()
{
	WebviewRestArray<Environment> rv;

	Environment env;
	env.set_kind("Environment");
	env.set_apiVersion(Environment::api_version());
	env.set_name("Refbox Game CLIPS Environment");
	rv.push_back(std::move(env));

	return rv;
}

Machine
ClipsRestApi::gen_machine(CLIPS::Fact::pointer &fact)
{
	Machine m;
	m.set_name(get_value<std::string>(fact, "name"));
	m.set_team(get_value<std::string>(fact, "team"));
	m.set_mtype(get_value<std::string>(fact, "mtype"));
	m.set_actual_lights(get_values(fact, "actual-lights"));
	m.set_state(get_value<std::string>(fact, "state"));
	m.set_zone(get_value<std::string>(fact, "zone"));
	m.set_rotation(get_value<int64_t>(fact, "rotation"));
	m.set_bases_added(get_value<int64_t>(fact, "bases-added"));
	m.set_bases_used(get_value<int64_t>(fact, "bases-used"));
	m.set_bs_side(get_value<std::string>(fact, "bs-side"));
	m.set_bs_color(get_value<std::string>(fact, "bs-color"));
	m.set_ds_order(get_value<int64_t>(fact, "ds-order"));
	m.set_rs_ring_color(get_value<std::string>(fact, "rs-ring-color"));
	m.set_cs_operation(get_value<std::string>(fact, "cs-operation"));
	m.set_cs_retrieved(get_value<bool>(fact, "cs-retrieved"));
	return m;
}

void
ClipsRestApi::gen_machines_precompute(MachineMap & machines)
{
    CLIPS::Fact::pointer fact =  env_->get_facts();
    while(fact) {
	   CLIPS::Template::pointer tmpl = fact->get_template();
	   if(tmpl->name() == "machine")
	   machines[get_value<std::string>(fact,  "name")] = fact;
	   fact = fact->next();
    }
}

WebviewRestArray<Machine>
ClipsRestApi::cb_list_machines()
{
     MutexLocker lock(&env_mutex_);
	WebviewRestArray<Machine> rv;

	std::map<std::string, CLIPS::Fact::pointer> machines;
	gen_machines_precompute(machines);

	for (auto &mi : machines )
	rv.push_back(std::move(gen_machine(mi.second)));
	return rv;
}


} //end namespace llsfrb
