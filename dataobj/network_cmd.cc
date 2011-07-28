#include "network_cmd.h"
#include "network.h"
#include "network_debug.h"
//#include "network_file_transfer.h"
#include "network_packet.h"
#include "network_socket_list.h"

#include "loadsave.h"
#include "gameinfo.h"
#include "../simtools.h"
#include "../simmenu.h"
#include "../simmesg.h"
#include "../simsys.h"
#include "../simversion.h"
#include "../dataobj/umgebung.h"
#include "../player/simplay.h"

#ifdef _MSC_VER
#include <direct.h>
#else
#include <unistd.h>
#endif


#ifdef DO_NOT_SEND_HASHES
static vector_tpl<uint16>hashes_ok;	// bit set, if this player on that client is not protected
#endif

network_command_t* network_command_t::read_from_packet(packet_t *p)
{
	// check data
	if (p==NULL  ||  p->has_failed()  ||  !p->check_version()) {
		delete p;
		dbg->warning("network_command_t::read_from_packet", "error in packet");
		return NULL;
	}
	network_command_t* nwc = NULL;
	switch (p->get_id()) {
		case NWC_GAMEINFO:    nwc = new nwc_gameinfo_t(); break;
		case NWC_JOIN:	      nwc = new nwc_join_t(); break;
		case NWC_SYNC:        nwc = new nwc_sync_t(); break;
		case NWC_GAME:        nwc = new nwc_game_t(); break;
		case NWC_READY:       nwc = new nwc_ready_t(); break;
		case NWC_TOOL:        nwc = new nwc_tool_t(); break;
		case NWC_CHECK:       nwc = new nwc_check_t(); break;
		case NWC_PAKSETINFO:  nwc = new nwc_pakset_info_t(); break;
		case NWC_DEBUG:       nwc = new nwc_debug_t(); break;
		default:
			dbg->warning("network_command_t::read_from_socket", "received unknown packet id %d", p->get_id());
	}
	if (nwc) {
		if (!nwc->receive(p) ||  p->has_failed()) {
			dbg->warning("network_command_t::read_from_packet", "error while reading cmd from packet");
			delete nwc;
			nwc = NULL;
		}
	}
	return nwc;
}


// needed by world to kick clients if needed
SOCKET network_command_t::get_sender()
{
	return packet->get_sender();
}


network_command_t::network_command_t(uint16 id_)
{
	packet = new packet_t();
	id = id_;
	our_client_id = (uint32)network_get_client_id();
	ready = false;
}


// default constructor
network_command_t::network_command_t()
: packet(NULL), id(0), our_client_id(0), ready(false)
{}


bool network_command_t::receive(packet_t *p)
{
	ready = true;
	if(  packet  ) {
		delete packet;
	}
	packet = p;
	id = p->get_id();
	rdwr();
	return (!packet->has_failed());
}

network_command_t::~network_command_t()
{
	if (packet) {
		delete packet;
		packet = NULL;
	}
}

void network_command_t::rdwr()
{
	if (packet->is_saving()) {
		packet->set_id(id);
		ready = true;
	}
	packet->rdwr_long(our_client_id);
	dbg->message("network_command_t::rdwr", "%s packet_id=%d, client_id=%ld", packet->is_saving() ? "write" : "read", id, our_client_id);
}


void network_command_t::prepare_to_send()
{
	// saves the data to the packet
	if(!ready) {
		rdwr();
	}
}


bool network_command_t::send(SOCKET s)
{
	prepare_to_send();
	packet->send(s, true);
	bool ok = packet->is_ready();
	if (!ok) {
		dbg->warning("network_command_t::send", "send packet_id=%d to [%d] failed", id, s);
	}
	return ok;
}


bool network_command_t::is_local_cmd()
{
	return (our_client_id == (uint32)network_get_client_id());
}


packet_t* network_command_t::copy_packet() const
{
	if (packet) {
		return new packet_t(*packet);
	}
	else {
		return NULL;
	}
}

nwc_service_t::~nwc_service_t()
{
	if (socket_info) {
		delete socket_info;
	}
	if (address_list) {
		delete address_list;
	}
	if (text) {
		free(text);
	}
}

extern address_list_t blacklist;

void nwc_service_t::rdwr()
{
	network_command_t::rdwr();
	packet->rdwr_long(flag);
	packet->rdwr_long(number);
	switch(flag) {
		case SRVC_LOGIN_ADMIN:
		case SRVC_BAN_IP:
		case SRVC_UNBAN_IP:
		case SRVC_ADMIN_MSG:
			packet->rdwr_str(text);
			break;

		case SRVC_GET_CLIENT_LIST:
			if (packet->is_loading()) {
				socket_info = new vector_tpl<socket_info_t>(10);
				// read the list
				socket_list_t::rdwr(packet, socket_info);
			}
			else {
				// write the socket list
				socket_list_t::rdwr(packet);
			}
			break;
		case SRVC_GET_BLACK_LIST:
			if (packet->is_loading()) {
				address_list = new address_list_t();
				address_list->rdwr(packet);
			}
			else {
				blacklist.rdwr(packet);
			}
			break;
		default: ;
	}
}
