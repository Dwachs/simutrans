#include "network_debug.h"
#include "network_packet.h"
#include "network_socket_list.h"
#include "umgebung.h"
#include <stdio.h>
#ifdef _MSC_VER
#include <direct.h>
#else
#include <unistd.h>
#endif

slist_tpl<nwc_debug_t::node_t*> nwc_debug_t::info;
uint32 nwc_debug_t::max_nodes = 1;

void network_debug_desync(uint32 check_failed_sync_step, cbuffer_t &buf)
{
	typedef nwc_debug_t::node_t node_t;
	SOCKET sock = socket_list_t::get_server_connection_socket();
	buf.printf("Check failed at sync-step=%d<br>", check_failed_sync_step);
	slist_tpl<nwc_debug_t::node_t*> &info = nwc_debug_t::info;
	// check the checksums in the list
	node_t *first_failed = NULL;
	node_t *last_success = NULL;
	for(slist_tpl<node_t*>::iterator iter = info.begin(), end = info.end(); iter!=end; ++iter)
	{
		node_t *node = *iter;
		// request info		
		nwc_debug_t nwc(nwc_debug_t::get_chk, node->sync_step);
		dbg->message("network_debug_desync", "request sync-step=%d", node->sync_step);
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
			//buf.printf("checksums at %d %s<br>\n", node->sync_step, ok ? "are equal" : "differ");
			if (first_failed == NULL) {
				if (ok) {
					last_success = node;
				}
				else {
					first_failed = node;
				}
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
	else {
		buf.printf("checksums at %d are equal<br>\n", last_success->sync_step);
	}
	buf.printf("checksums at %d differ<br>\n", first_failed->sync_step);

	// now obtain the log string and compare it
	// request string		
	{
		nwc_debug_t nwc(nwc_debug_t::get_msg, first_failed->sync_step);
		dbg->message("network_debug_desync", "request msg ss=%d", first_failed->sync_step);
		if(!nwc.send(sock)) {
			goto send_error;
		}
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
	buf.printf("<br><h1>Log message of sync-step %d</h1><br>", first_failed->sync_step);
	// pointer to start of current messages
	const char* mc = (const char*)(first_failed->buf);
	const char* ms = (const char*)(*nwd->pbuf);
	// now loop through the strings
	const char* pc = mc;
	const char* ps = ms;
	bool equal = true;
	bool eol = false;
	while((*pc)  ||  (*ps)) {
		// compare
		if (equal  &&  (*ps)!=(*pc)) {
			equal = false;
		}
		// advance till end of string / end of line if necessary
		if (*ps  &&  *ps < 32) {
			eol = true;
			for (; (*pc)  &&  (*pc)>=32; pc++) {}
		}
		else if  (*pc  &&  *pc < 32) {
			eol = true;
			for (; (*ps)  &&  (*ps)>=32; ps++) {}
		}
		// output
		if (eol) {
			if (equal) {
				std::string str_c(mc, pc-mc);
				buf.printf("%s<br>", str_c.c_str());
			}
			else {
				std::string str_c(mc, pc-mc);
				buf.printf("<em>&gt;&gt;client&gt;&gt;</em><br><em>%s</em><br>", str_c.c_str());
				std::string str_s(ms, ps-ms);
				buf.printf("====<br><st>%s<br>&lt;&lt;server&lt;&lt;</st><br>", str_s.c_str());
			}
			for (; (*pc)  &&  (*pc)<32; pc++) {}
			for (; (*ps)  &&  (*ps)<32; ps++) {}
			mc = pc;
			ms = ps;
			eol = false;
			equal = true;
		}
		
		if (*pc) pc++;
		if (*ps) ps++;
	}

	// finally put everything into a log-file
	chdir( umgebung_t::user_dir );
	const char* filename = "desync.log";
	if (FILE *f = fopen(filename, "wb")) {
		fprintf(f, "Check failed at sync-step=%d\n", check_failed_sync_step);
		if (last_success) {
			fprintf(f, "Last identic checksum at sync-step=%d\n", last_success->sync_step);
			fprintf(f, "-- Client log --\n%s\n", (const char*)last_success->buf);
		}
		fprintf(f, "First failed checksum at sync-step=%d\n", first_failed->sync_step);
		fprintf(f, "-- Client log --\n%s\n", (const char*)first_failed->buf);
		fprintf(f, "-- Server log --\n%s\n", (const char*)(*nwd->pbuf));
		fclose(f);
		buf.printf("Wrote report to file %s<br>\n", filename);
	}
	else {
		buf.printf("ERR: could not open file %s<br>\n", filename);
	}

	return;
send_error:
	buf.append("ERR: send failed. the end.<br>\n");
}


bool nwc_debug_t::execute(karte_t *)
{
	if (umgebung_t::server) {
		if (state == get_chk  ||  state == get_msg) {
			// assume no data
			nwc_debug_t nwd(no_data, info.empty() ? (uint32)-1 : info.front()->sync_step);
			// search for node
			if (node_t *node = get_node(sync_step)) {
				nwd.sync_step = sync_step;
				if (state == get_chk) {
					nwd.state = send_chk;
					nwd.chk = node->chk;
				}
				else {
					nwd.state = send_msg;
					nwd.pbuf  = &(node->buf);
				}
			}
			nwd.send( packet->get_sender() );
			nwd.pbuf = NULL;
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
	if (state == send_msg) {
		char *buf = packet->is_saving() ? strdup( (const char*)(*pbuf) ) : NULL;
		packet->rdwr_str(buf);
		if (packet->is_loading()) {
			if (pbuf == NULL) {
				pbuf = new cbuffer_t();
			}
			pbuf->clear();
			pbuf->append(buf);
		}
		free(buf);
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

	for(uint16 i=0; i<lengthof(msg)  &&  msg[i]; i++) {
		if (msg[i]==' ') {
			msg[i]='_';
		}
	}
	node->buf.printf("%s:%d:_%s\n", filename, line, msg);

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
