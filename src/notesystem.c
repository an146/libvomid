#include <assert.h>
#include <ctype.h>
#include "vomid_local.h"

status_t
pitch_info(const notesystem_t *ns, pitch_t pitch, midipitch_t *midipitch, int *wheel)
{
	float octaves = pitch / ns->size * ns->pitches[ns->size] + ns->pitches[pitch % ns->size];
	float fmp = octaves * 12;
	int mp = (int)(fmp + 0.5f);
	if (mp < 0 || mp >= NOTES)
		return ERROR;

	*midipitch = mp;
	*wheel = (int)((fmp - mp) * 0x1000);
	return OK;
}

static float *
tet_pitches(int n)
{
	float *pitches = malloc((n+1) * sizeof(*pitches));
	for (int i = 0; i <= n; i++)
		pitches[i] = i / (float)n;
	return pitches;
}

static FILE *
tet_scala(int n)
{
	FILE *f = tmpfile();

	fprintf(f, "! Created by libvomid (http://vomid.org)\r\n");
	fprintf(f, "%i-TET\r\n", n);
	fprintf(f, "%i\r\n", n);
	for (int i = 0; i < n; i++)
		fprintf(f, "%i/%i\r\n", n + i + 1, n);
	return f;
}

notesystem_t
notesystem_tet(int n)
{
	if (n <= 0 || n == 12)
		return notesystem_midistd();

	FILE *scala = tet_scala(n);
	notesystem_t ret = notesystem_import_f(scala);
	fclose(scala);

	assert(ret.pitches != NULL);
	return ret;
}

notesystem_t
notesystem_midistd(void)
{
	return (notesystem_t){
		.size = 12,
		.pitches = tet_pitches(12),
		.scala = NULL,
		.end_pitch = 128
	};
}

bool_t
notesystem_is_midistd(const notesystem_t *ns)
{
	return ns->scala == NULL;
}

void
notesystem_fini(notesystem_t ns)
{
	free(ns.pitches);
	free(ns.scala);
}

pitch_t
notesystem_level2pitch(const notesystem_t *ns, int level)
{
	pitch_t beg = 0, end = notesystem_levels(ns);
	while (beg < end) {
		pitch_t p = (beg + end) / 2;
		int l = notesystem_pitch2level(ns, p);
		if (level < l)
			end = p;
		else if (level == l)
			return p;
		else
			beg = p + 1;
	}
	return -1;
}

int
notesystem_pitch2level(const notesystem_t *ns, pitch_t p)
{
	if (notesystem_is_midistd(ns)) {
		int ef_lines = p >= 5 ? 1 : 0;
		return p + ef_lines;
	} else
		return p;
}

int
notesystem_levels(const notesystem_t *ns)
{
	return notesystem_pitch2level(ns, ns->size - 1) + 1;
}

static float
read_pitch(const char *s)
{
	while (isspace(*s))
		s++;
	if (!isdigit(*s))
		return -1;

	const char *i = s;
	while (isdigit(*i))
		i++;
	if (*i == '/') {
		i++;
		if (!isdigit(*i))
			return -1;
		return atoi(s) / (float)atoi(i) - 1;
	} else
		return atof(s) / 1200;
}

notesystem_t
notesystem_import_f(FILE *f)
{
	notesystem_t ret;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	ret.scala = malloc(size + 1);
	fread(ret.scala, 1, size, f);
	ret.scala[size] = '\0';

	char buf[1024];
	char *succ = NULL;
	fseek(f, 0, SEEK_SET);

	while ((succ = fgets(buf, sizeof(buf), f)) && buf[0] == '!')
		;
	if (!succ)
		goto error_before_pitches;
	// here we could process the description

	while ((succ = fgets(buf, sizeof(buf), f)) && buf[0] == '!')
		;
	if (!succ || sscanf(buf, "%i", &ret.size) < 1)
		goto error_before_pitches;

	ret.pitches = malloc((ret.size+1) * sizeof(*ret.pitches));
	ret.pitches[0] = 0;
	for (int i = 0; i < ret.size; i++) {
		while ((succ = fgets(buf, sizeof(buf), f)) && buf[0] == '!')
			;
		if (!succ)
			goto error;
		ret.pitches[i+1] = read_pitch(buf);
		if (ret.pitches[i+1] < 0)
			goto error;
	}
	midipitch_t mp;
	int pw;
	for (ret.end_pitch = 0; pitch_info(&ret, ret.end_pitch, &mp, &pw) == OK; ret.end_pitch++)
		;
	return ret;

error:
	free(ret.pitches);
error_before_pitches:
	free(ret.scala);
	ret.pitches = NULL;
	ret.scala = NULL;
	return ret;
}
