#include <alsa/asoundlib.h>

int
main()
{
	snd_seq_t *seq_handle;
	snd_seq_open(&seq_handle, "hw", SND_SEQ_OPEN_DUPLEX, 0);

	return 0;
}
