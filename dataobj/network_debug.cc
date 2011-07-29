#include "network_debug.h"
#include "network_packet.h"
#include "network_socket_list.h"
#include "umgebung.h"

slist_tpl<nwc_debug_t::node_t*> nwc_debug_t::info;
uint32 nwc_debug_t::max_nodes = 1;

void network_debug_desync(uint32 check_failed_sync_step, cbuffer_t &buf)
{
	typedef nwc_debug_t::node_t node_t;
	SOCKET sock = socket_list_t::get_server_connection_socket();
	buf.printf("Check failed at ss=%d<br>\n", check_failed_sync_step);
	slist_tpl<nwc_debug_t::node_t*> &info = nwc_debug_t::info;
	// check the checksums in the list
	node_t *first_failed = NULL;
	node_t *last_success = NULL;
	for(slist_tpl<node_t*>::iterator iter = info.begin(), end = info.end(); iter!=end; ++iter)
	{
		node_t *node = *iter;
		// request info		
		nwc_debug_t nwc(nwc_debug_t::get_chk, node->sync_step);
		dbg->message("network_debug_desync", "request ss=%d", node->sync_step);
		if(!nwc.send(sock)) {
			goto send_error;
		}
		nwc_debug_t *nwd = NULL;
		// wait for nwc_debug_t, ignore other commands
		for(uint8 i=0; i<5; i++) {
			network_command_t* nwc = network_check_activity( NULL, 10000 );
			if (nwc  &&  nwc->get_id() == NWC_DEBUG) {
				nwd = (nwc_debug_t*)nwc;
				break;
			}
			delete nwc;
		}
		if(nwd == NULL) {
			buf.append("ERR: server did not respond<br>\n");
			return;
		}
		if(nwd->state == nwc_debug_t::send_chk) {
			bool ok = nwd->chk == node->chk;
			buf.printf("checksums at %d %s<br>\n", node->sync_step, ok ? "are equal" : "differ");
			if (ok) {
				last_success = node;
			}
			else {
				first_failed = node;
			}
		}
		else {
			buf.printf("WARN: no data for sync step %d available, earliest available is %d<br>\n", node->sync_step, nwd->sync_step);
		}
	}
	if(first_failed == NULL) {
		buf.append("ERR: all checksums identic. the end.<br>\n");
		return;
	}
	else if (last_success == NULL) {
		buf.append("WARN: all checksums differ<br>\n");
	}

	buf.append("the end.<br>\n");
	return;
send_error:
	buf.append("ERR: send failed. the end.<br>\n");
}


bool nwc_debug_t::execute(karte_t *)
{
	if (umgebung_t::server) {
		switch(state) {
			case nwc_debug_t::get_chk: {
				if (node_t *node = get_node(sync_step)) {
					nwc_debug_t nwd(send_chk, sync_step);
					nwd.chk = node->chk;
					nwd.send( packet->get_sender() );
				}
				else {
					nwc_debug_t nwd(no_data, info.empty() ? (uint32)-1 : info.front()->sync_step);
					nwd.send( packet->get_sender() );
				}
				break;
			}
			default: ;
		}
	}
	return true;
}


void nwc_debug_t::rdwr()
{
	network_command_t::rdwr();
	packet->rdwr_byte(state);
	packet->rdwr_long(sync_step);
	if (state == send_chk) {
		chk.rdwr(packet);
	}
	else if (packet->is_loading()) {
		chk.reset();
	}
}


void nwc_debug_t::init(uint32 max_nodes_)
{
	max_nodes = max_nodes_;
	info.clear();
}


void nwc_debug_t::new_sync_step(uint32 syncstep)
{
	dbg->message("nwc_debug_t::new_sync_step", "node at ss=%d", syncstep);
	if (info.get_count() > 0) {
		info.back()->chk.finish();
	}
	if (info.get_count() == max_nodes  &&  max_nodes>0) {
		node_t *node = info.front();
		info.erase( info.begin() );
		node->sync_step = syncstep;
		node->chk.reset();
		node->buf.clear();
		info.append(node);
	}
	else {
		info.append( new node_t(syncstep) );
	}
}


const char* strip_path(const char *file)
{
	for(int i = strlen(file); i>=0; i--) {
		if (file[i]=='/'  ||  file[i]=='\\') {
			return file+i+1;
		}
	}
	return file;
}

void nwc_debug_t::add_msg(const char* file, int line, const char* fmt, ...)
{
	if (info.empty() ||  !umgebung_t::networkmode) {
		// logging not yet started?
		return;
	}
	node_t *node = info.back();
	const char* filename = strip_path(file);
	// add to buffer
	char msg[256];
	va_list ap;
	va_start(ap, fmt);
	int count = vsnprintf( msg, lengthof(msg), fmt, ap);
	va_end(ap);

	node->buf.printf("%s:%d: %s\n", filename, line, msg);

	// checksum
	node->chk.input(file);
	node->chk.input((uint16)line);
	node->chk.input(msg);
}


nwc_debug_t::node_t* nwc_debug_t::get_node(uint32 sync)
{
	for(slist_tpl<node_t*>::iterator iter = info.begin(), end = info.end(); iter!=end; ++iter) {
		dbg->message("nwc_debug_t::get_node", "node at ss=%d wanted ss=%d", (*iter)->sync_step, sync);
		if (sync == (*iter)->sync_step) {
			return *iter;
		}
	}
	return NULL;
}
