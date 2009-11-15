/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 *
 * midi.c
 * various MIDI-related stuff
 */

#include <assert.h>
#include <memory.h>
#include "vomid_local.h"

const uchar magic_mthd[4] = {'M', 'T', 'h', 'd'};
const uchar magic_mtrk[4] = {'M', 'T', 'r', 'k'};
const uchar magic_vomid[4] = {'V', 'o', 'm', 0x1D};

const uchar midi_eot[3] = {0xFF, VMD_META_EOT, 0};

static int
blog2(int n)
{
	int ret = 0;

	while (n > 1) {
		n /= 2;
		ret++;
	}
	return ret;
}

int
timesig_construct(int numer, int denom)
{
	return (numer & 0xFF) + (blog2(denom) << 8);
}

double
time2systime(time_t time, int tempo, unsigned int division)
{
	return (double)time * tempo / division / 1000000;
}

time_t
systime2time(double sec, int tempo, unsigned int division)
{
	return sec * 1000000 * division / tempo;
}

static int
read_tempo(uchar *data, int len)
{
	return data[0] * 0x10000 + data[1] * 0x100 + data[2];
}

static int
read_timesig(uchar *data, int len)
{
	return data[0] + data[1] * 0x100;
}

static void
write_ctrl(ctrl_info_t *info, int channel, int value, uchar *buf, int *len)
{
	*len = 3;
	buf[0] = VOICE_CONTROLLER + channel;
	buf[1] = info->midi_ctrl;
	assert(value >= 0 && value < 128);
	buf[2] = value;
}

static void
write_tempo(ctrl_info_t *info, int channel, int value, uchar *buf, int *len)
{
	assert(value >= 0 && value < 0x1000000);
	midi_write_meta(META_TEMPO, (uchar[]){value / 0x10000, value / 0x100, value}, 3, buf, len);
	/*
	*len = 4;
	buf[0] = 0xFF;
	buf[3] = value % 0x100;
	value /= 0x100;
	buf[2] = value % 0x100;
	value /= 0x100;
	buf[1] = value % 0x100;
	*/
}

static void
write_timesig(ctrl_info_t *info, int channel, int value, uchar *buf, int *len)
{
	midi_write_meta(META_TIMESIG, (uchar[]){value & 0xFF, value >> 8, 0x60, 8}, 4, buf, len);
}

static void
write_program(ctrl_info_t *info, int channel, int value, uchar *buf, int *len)
{
	*len = 2;
	buf[0] = VOICE_PROGRAM + channel;
	assert(value >= 0 && value < 128);
	buf[1] = value;
}

static void
write_pitchwheel(ctrl_info_t *info, int channel, int value, uchar *buf, int *len)
{
	*len = 3;
	buf[0] = VOICE_PITCHWHEEL + channel;
	assert(value >= -0x2000 && value < 0x2000);
	value += 0x2000;
	buf[1] = value % 128;
	buf[2] = value / 128;
}

static inline void
check_note(note_t *note)
{
	assert(note->channel->number >= 0 && note->channel->number < CHANNELS);
	assert(note->midipitch >= 0);
	assert(note->on_vel >= 0);
	assert(note->off_vel >= 0);
}

void
midi_write_noteon(note_t *note, uchar *buf, int *len)
{
	check_note(note);

	*len = 3;
	buf[0] = VOICE_NOTEON + note->channel->number;
	buf[1] = note->midipitch;
	buf[2] = note->on_vel;
}

void
midi_write_noteoff(note_t *note, uchar *buf, int *len)
{
	check_note(note);

	*len = 3;
	buf[0] = VOICE_NOTEOFF + note->channel->number;
	buf[1] = note->midipitch;
	buf[2] = note->off_vel;

}

void
midi_write_meta(uchar type, const uchar *data, int len, uchar *buf, int *ev_len)
{
	assert(len < 128);
	*ev_len = 3 + len;
	buf[0] = 0xFF;
	buf[1] = type;
	buf[2] = len;
	memcpy(buf + 3, data, len);
}

void
midi_fwrite_varlen(FILE *out, time_t time){
	assert(time >= 0);
	if (time > 0) {
		uchar buf[32];
		uchar *s = buf + sizeof(buf);
		uchar *e = s;

		while (time != 0) {
			*--s = time % 0x80;
			if (s != e - 1)
				*s += 0x80;
			time /= 0x80;
		}
		fwrite(s, 1, e - s, out);
	} else
		fputc(0, out);
}

void
midi_fwrite_meta_header(FILE *out, uchar type, int len)
{
	fputc(0xFF, out);
	fputc(type, out);
	midi_fwrite_varlen(out, len);
}

void
midi_fwrite_meta(FILE *out, uchar type, const uchar *data, int len)
{
	midi_fwrite_meta_header(out, type, len);
	fwrite(data, 1, len, out);
}

void
midi_fwrite_propr(FILE *out, uchar type, uchar *data, int s)
{
	midi_fwrite_meta_header(out, META_PROPRIETARY, PROPR_HEADER_SIZE + s);
	fwrite(magic_vomid, 1, sizeof(magic_vomid), out);
	fputc(type, out);
	fwrite(data, 1, s, out);
}

void
midi_fwrite_pitch(FILE *out, pitch_t pitch)
{
	assert(pitch >= 0);
	midi_fwrite_propr(out, PROPR_PITCH, (uchar []){pitch / 0x100, pitch % 0x100}, 2);
}

void
midi_fwrite_notesystem(FILE *out, const notesystem_t *ns)
{
	midi_fwrite_propr(out, PROPR_NOTESYSTEM, (uchar *)ns->scala, strlen(ns->scala));
}

ctrl_info_t tvalue_info[TVALUES] = {
	{"Volume", 100, 7, -1, NULL, write_ctrl},
	{"Pan", 64, 10, -1, NULL, write_ctrl},
	//{"Track Name", NULL, -1, META_TRACKNAME, read_text, write_text},
};

ctrl_info_t fctrl_info[FCTRLS] = {
	{"Tempo", TEMPO_MIDI(120), -1, META_TEMPO, read_tempo, write_tempo},
	{"Time Signature", /* TIMESIG(4, 4) */ 0x204, -1, META_TIMESIG, read_timesig, write_timesig},
};

ctrl_info_t tctrl_info[TCTRLS] = {
	{"Balance", 64, 8, -1, NULL, write_ctrl},
	{"Expression", 127, 11, -1, NULL, write_ctrl},
};

ctrl_info_t cctrl_info[CCTRLS] = {
	{"Program", 0, -1, -1, NULL, write_program},
	{"Pitch Wheel", 0, -1, -1, NULL, write_pitchwheel},
};

const char *gm_instr_name[PROGRAMS] = {
	"Acoustic Grand Piano",
	"Bright Acoustic Piano",
	"Electric Grand Piano",
	"Honky-tonk Piano",
	"Rhodes Piano",
	"Chorused Piano",
	"Harpsichord",
	"Clavinet",
	"Celesta",
	"Glockenspiel",
	"Music Box",
	"Vibraphone",
	"Marimba",
	"Xylophone",
	"Tubular Bells",
	"Dulcimer",
	"Hammond Organ",
	"Percussive Organ",
	"Rock Organ",
	"Church Organ",
	"Reed Organ",
	"Accordion",
	"Harmonica",
	"Tango Accordion",
	"Acoustic Guitar (nylon)",
	"Acoustic Guitar (steel)",
	"Electric Guitar (jazz)",
	"Electric Guitar (clean)",
	"Electric Guitar (muted)",
	"Overdriven Guitar",
	"Distortion Guitar",
	"Guitar Harmonics",
	"Acoustic Bass",
	"Electric Bass (finger)",
	"Electric Bass (pick)",
	"Fretless Bass",
	"Slap Bass 1",
	"Slap Bass 2",
	"Synth Bass 1",
	"Synth Bass 2",
	"Violin",
	"Viola",
	"Cello",
	"Contrabass",
	"Tremolo Strings",
	"Pizzicato Strings",
	"Orchestral Harp",
	"Timpani",
	"String Ensemble 1",
	"String Ensemble 2",
	"SynthStrings 1",
	"SynthStrings 2",
	"Choir Aahs",
	"Voice Oohs",
	"Synth Voice",
	"Orchestra Hit",
	"Trumpet",
	"Trombone",
	"Tuba",
	"Muted Trumpet",
	"French Horn",
	"Brass Section",
	"Synth Brass 1",
	"Synth Brass 2",
	"Soprano Sax",
	"Alto Sax",
	"Tenor Sax",
	"Baritone Sax",
	"Oboe",
	"English Horn",
	"Bassoon",
	"Clarinet",
	"Piccolo",
	"Flute",
	"Recorder",
	"Pan Flute",
	"Bottle Blow",
	"Shakuhachi",
	"Whistle",
	"Ocarina",
	"Lead 1 (square)",
	"Lead 2 (sawtooth)",
	"Lead 3 (calliope lead)",
	"Lead 4 (chiff lead)",
	"Lead 5 (charang)",
	"Lead 6 (voice)",
	"Lead 7 (fifths)",
	"Lead 8 (bass + lead)",
	"Pad 1 (new age)",
	"Pad 2 (warm)",
	"Pad 3 (polysynth)",
	"Pad 4 (choir)",
	"Pad 5 (bowed)",
	"Pad 6 (metallic)",
	"Pad 7 (halo)",
	"Pad 8 (sweep)",
	"FX 1 (rain)",
	"FX 2 (soundtrack)",
	"FX 3 (crystal)",
	"FX 4 (atmosphere)",
	"FX 5 (brightness)",
	"FX 6 (goblins)",
	"FX 7 (echoes)",
	"FX 8 (sci-fi)",
	"Sitar",
	"Banjo",
	"Shamisen",
	"Koto",
	"Kalimba",
	"Bagpipe",
	"Fiddle",
	"Shanai",
	"Tinkle Bell",
	"Agogo",
	"Steel Drums",
	"Woodblock",
	"Taiko Drum",
	"Melodic Tom",
	"Synth Drum",
	"Reverse Cymbal",
	"Guitar Fret Noise",
	"Breath Noise",
	"Seashore",
	"Bird Tweet",
	"Telephone Ring",
	"Helicopter",
	"Applause",
	"Gunshot"
};

void
notes_off()
{
	for (int i = 0; i < CHANNELS; i++)
		output((uchar []){VOICE_CONTROLLER + i, 123, 0}, 3);
	flush_output();
}
