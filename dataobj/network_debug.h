#ifndef _NETWORK_DEBUG_H_
#define _NETWORK_DEBUG_H_

#ifdef DEBUG_DESYNC

#include "network_cmd.h"
#include "../utils/cbuffer_t.h"
#include "../utils/checksum.h"

#define network_debug_add(fmt, ...)            nwc_debug_t::add_msg(__FILE__, __LINE__, fmt, __VA_ARGS__)
#define network_debug_new_sync_step(sync_step) nwc_debug_t::new_sync_step(sync_step)

void network_debug_desync(uint32 check_failed_sync_step, cbuffer_t &buf);
/**
 * nwc_gameinfo_t
 * @from-client:
 * @from-server:
 */
class nwc_debug_t : public network_command_t {
public:

	enum dbg_state {
		init_dbg = 1,   // initialize sending of data to client
		get_chk  = 2,   // get checksum from syncstep
		send_chk = 3,   // send checksum from syncstep
		get_msg  = 4,   // get messages from syncstep
		send_msg = 5,   // send messages from syncstep
		no_data  = 101, // no data available
		end_dbg  = 255  // end of communication
	};

	nwc_debug_t(dbg_state state_=end_dbg, uint32 sync_step_=0) : network_command_t(NWC_DEBUG),
		state(state_), sync_step(sync_step_), chk(), pbuf(NULL) {}

	~nwc_debug_t() { if (pbuf) delete pbuf; }
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
		static bool compare(const node_t &a,  const node_t &b) { return a.sync_step <= b.sync_step; }
		bool operator==(const node_t &a) { return sync_step = a.sync_step; }
	};

	static node_t* get_node(uint32 sync);

	static vector_tpl<node_t*> info;
	static uint32 max_nodes;

	uint8 state;
	uint32 sync_step;
	checksum_t chk;
	cbuffer_t* pbuf;
};

#else

#define network_debug_add(fmt, ...)            (void)(fmt)
#define network_debug_new_sync_step(sync_step) (void)(sync_step)

#endif // ifdef DEBUG_DESYNC

#endif
