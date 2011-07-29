#ifndef _NETWORK_DEBUG_H_
#define _NETWORK_DEBUG_H_

#include "network_cmd.h"
#include "../utils/cbuffer_t.h"
#include "../utils/checksum.h"


#define network_add_debug(fmt, ...) nwc_debug_t::add_msg(__FILE__, __LINE__, fmt, __VA_ARGS__);

void network_debug_desync(uint32 check_failed_sync_step, cbuffer_t &buf);
/**
 * nwc_gameinfo_t
 * @from-client:
 * @from-server:
 */
class nwc_debug_t : public network_command_t {
public:

	enum dbg_state {
		init_dbg = 1, // initialize sending of data to client
		get_chk = 2,  // get checksum from syncstep
		send_chk = 3,  // sent checksum from syncstep
		get_msg = 13,  // get messages from syncstep
		no_data = 101,  // no data available
		end_dbg = 255 // end of communication
	};

	nwc_debug_t(dbg_state state_=end_dbg, uint32 sync_step_=0) : network_command_t(NWC_DEBUG), state(state_), sync_step(sync_step_) {}

	// server side part of communication

	virtual bool execute(karte_t *);
	virtual void rdwr();
	virtual const char* get_name() { return "nwc_debug_t"; }



	// static stuff: ring list to save random number states etc
	static void init(uint32 max_nodes_);
	static void new_sync_step(uint32 syncstep);
	static void add_msg(const char* file, int line, const char* fmt, ...);

	struct node_t {
		uint32 sync_step;
		cbuffer_t buf;
		checksum_t chk;
		node_t(uint32 sync_step_) : sync_step(sync_step_), buf(), chk() { }
	};

	static node_t* get_node(uint32 sync);

	static slist_tpl<node_t*> info;
	static uint32 max_nodes;

	uint8 state;
	uint32 sync_step;
	checksum_t chk;
};

#endif
