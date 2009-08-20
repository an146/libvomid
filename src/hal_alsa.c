/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifdef HAL_ALSA

#include <alloca.h>
#include <alsa/asoundlib.h>
#include "vomid_local.h"

snd_seq_t *seq;
int port;
snd_midi_event_t *event;
int conn_client = -1, conn_port;

#define INPUT_CAP  (SND_SEQ_PORT_CAP_READ  | SND_SEQ_PORT_CAP_SUBS_READ)
#define OUTPUT_CAP (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)

static status_t
_init()
{
	if (snd_seq_open(&seq, "hw", SND_SEQ_OPEN_DUPLEX, 0) >= 0) {
		//TODO: dynamic name?
		port = snd_seq_create_simple_port(
			seq,
			"main",
			INPUT_CAP,
			SND_SEQ_PORT_TYPE_MIDI_GENERIC
		);
		if (port < 0)
			return ERROR;
		if (snd_midi_event_new(1024, &event) < 0)
			return ERROR;
		return OK;
	} else
		return ERROR;
}

static void
_fini()
{
	snd_seq_close(seq);
	snd_midi_event_free(event);
}

static void
_enum_devices(int type, device_clb_t clb, void *arg)
{
	if (type == OUTPUT_DEVICE) {
		snd_seq_client_info_t *cinfo;
		snd_seq_port_info_t *pinfo;
		int client;

		snd_seq_client_info_alloca(&cinfo);
		snd_seq_client_info_set_client(cinfo, -1);

		while (snd_seq_query_next_client(seq, cinfo) >= 0) {
			client = snd_seq_client_info_get_client(cinfo);
			snd_seq_port_info_alloca(&pinfo);
			snd_seq_port_info_set_client(pinfo, client);

			snd_seq_port_info_set_port(pinfo, -1);
			while (snd_seq_query_next_port(seq, pinfo) >= 0) {
				if ((snd_seq_port_info_get_type(pinfo) & SND_SEQ_PORT_TYPE_MIDI_GENERIC) == 0)
					continue;
				if ((snd_seq_port_info_get_capability(pinfo) & OUTPUT_CAP) != OUTPUT_CAP)
					continue;

				char id[64];
				sprintf(
					id,
					"alsa/%i:%i",
					snd_seq_port_info_get_client(pinfo),
					snd_seq_port_info_get_port(pinfo)
				);
				clb(id, snd_seq_port_info_get_name(pinfo), arg);
			}
		}
	}
}

static status_t
_set_device(int type, const char *id)
{
	if (type == OUTPUT_DEVICE) {
		if (sscanf(id, "%i:%i", &conn_client, &conn_port) < 2)
			return ERROR;
		if (snd_seq_connect_to(seq, port, conn_client, conn_port) >= 0) {
			snd_seq_disconnect_from(seq, port, conn_client, conn_port);
			return OK;
		}
	}
	return ERROR;
}

static void
_output(const uchar *ev_raw, size_t size)
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	if (snd_midi_event_encode(event, ev_raw, size, &ev) <= 0 ||
		ev.type == SND_SEQ_EVENT_NONE)
			return;

	snd_seq_ev_set_source(&ev, port);
	snd_seq_ev_set_subs(&ev);
	snd_seq_ev_set_direct(&ev);

	snd_seq_event_output(seq, &ev);
}

static void
_flush_output()
{
	snd_seq_drain_output(seq);
}

platform_t vmd_platform_alsa = {
	.init = _init,
	.fini = _fini,
	.enum_devices = _enum_devices,
	.set_device = _set_device,
	.output = _output,
	.flush_output = _flush_output,
	.name = "alsa"
};

#endif // HAL_ALSA
