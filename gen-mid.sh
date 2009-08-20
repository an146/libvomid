source mk/prefix.mk

sym() {
	echo -n $@
}

hexbyte() {
	for b in $@; do
		echo -e -n "\x$b"
	done
}

byte() {
	for b in $@; do
		hexbyte `printf "%x" $(($b))`
	done
}

word() {
	byte $(($1 / 256))
	byte $(($1 % 256))
}

dword() {
	word $(($1 / 65536))
	word $(($1 % 65536))
}

varlen() {
	d=""
	n="$1"
	f="1"
	byte $(while true; do
		if [ -n "$f" ]; then
			d="$(($n % 128)) $d"
			f=""
		else
			d="$((128 + $n % 128)) $d"
		fi
		if [ "$n" -lt 128 ]; then
			echo $d
			break
		fi
		n=$(($n / 128))
	done)
}

delay() {
	delay=$1
}

cmd() {
	if [ -n "$delay" ]; then
		varlen "$delay"
	else
		varlen 0
	fi
	delay=""
}

on() {
	cmd
	byte $((9*16 + $1))
	byte $2 $3
}

off() {
	cmd
	byte $((8*16 + $1))
	byte $2 $3
}

ctrl() {
	cmd
	byte $((0xB*16 + $1))
	byte $2 $3
}

program() {
	cmd
	byte $((0xC*16 + $1))
	byte $2
}

pitchwheel() {
	cmd
	byte $((0xE*16 + $1))
	byte $(($2 % 128))
	byte $(($2 / 128))
}

meta() {
	cmd
	byte 0xFF
	byte $1
	varlen ${#2}
	echo -n $2
}

notesystem() {
	cmd
	byte 0xFF
	byte 0x7F
	byte 6

	sym Vom
	byte 0x1D
	byte 0
	byte $1
}

pitch() {
	cmd
	byte 0xFF
	byte 0x7F
	byte 7

	sym Vom
	byte 0x1D
	byte 1
	word $1
}

end() {
	meta 0x2F ""
}

track() {
	sym MTrk
	delay=
	while read line; do
		eval $line
	done > ${tmp_p}track.tmp
	dword `cat ${tmp_p}track.tmp | wc -c`
	cat ${tmp_p}track.tmp
}

header() {
	sym MThd
	dword 6
	word 1
	word $1
	word 240
}
