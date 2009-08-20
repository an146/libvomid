include mk/prefix.mk
-include ${target_p}config.mk

EXAMPLES_=${basename ${shell find examples -name \*.c -printf "%p "}}
EXAMPLES=${addprefix ${target_p}, ${EXAMPLES_}}${EXE_SUFFIX}

all: ${OUTFILE} ${EXAMPLES}

INC=include/vomid.h include/vomid_local.h include/vomid_test.h
SRC=${shell find src -name \*.c -printf "%p "}
OBJ=${addprefix ${obj_p}, ${addsuffix .o,${SRC}}}
DIRS=${obj_p}src/3rdparty/sha1 ${obj_p}test ${obj_p}examples build/gen build/tmp build/examples

TESTFILE=${target_p}tester${EXE_SUFFIX}
TESTSRC=${shell find test -name \*.c -printf "%p "}
TESTOBJ=${addprefix ${obj_p}, ${addsuffix .o, ${TESTSRC}}}

COMMON=Makefile ${target_p}config.mk build/.dirs

${target_p}config.mk:
	@echo CONF
	@./configure >/dev/null

${OUTFILE}: ${OBJ}
	@echo AR $@
	@${AR} rcs ${OUTFILE} ${OBJ}

${obj_p}%.c.o: %.c ${INC} ${gen_p}shortnames.h ${gen_p}platforms.h ${COMMON}
	@echo C99 $<
	@${C99} ${CFLAGS} -DFILE_ID=${shell ./file-id $<} -c $< -o $@

${target_p}examples/%${EXE_SUFFIX}: examples/%.c ${OUTFILE} ${COMMON}
	@echo C99 $@
	@${C99} ${CFLAGS} -o $@ $< ${LDFLAGS}

${TESTOBJ}: include/vomid_test.h ${gen_p}reg_tests.h

${TESTFILE}: ${OUTFILE} ${TESTOBJ}
	@echo LINK $@
	@${CC} -o $@ ${TESTOBJ} ${LDFLAGS}

test: all ${TESTFILE} ${LUAFILE} ./do-tests
	@./do-tests -i bst

test-all: all ${TESTFILE} ${LUAFILE} ./do-tests
	@./do-tests

clean:
	@echo RM build/
	@rm -Rf build

types: ${target_p}types.vim

build/.dirs:
	@echo MKDIR ${DIRS}
	@mkdir -p ${DIRS}
	@touch $@

${target_p}types.vim: ${INC} ${SRC} ${COMMON}
	@echo GEN $@
	@ctags --c-kinds=gstu -o- ${INC} ${SRC} | \
		awk 'BEGIN{printf("syntax keyword MyType\t")} {t=$$1; sub(/^vmd_/, "", t); printf("%s ", t)}' > $@

${gen_p}platforms.h: ${COMMON}
	@echo GEN $@
	@echo -n > $@
	@for p in ${PLATFORMS}; do \
		vp="vmd_platform_$$p"; \
		echo "extern platform_t $$vp; PLATFORM((&$$vp));" >> $@; \
	done

${gen_p}reg_tests.h: ${TESTSRC} ${COMMON}
	@echo GEN $@
	@./reg-tests ${TESTSRC} > $@

${gen_p}shortnames.h: ${INC} ${COMMON}
	@echo GEN $@
	@egrep -oih "vmdl?_[a-z0-9_]*" ${INC} | sort | uniq | \
		sed -r 's/([^_]*_)(.*)/#define \2 \1\2/' > $@

check_win32:
	@mkdir -p ${tmp_p}
	@${CC} mk/win32.c -o ${tmp_p}$@ -lwinmm > /dev/null 2>/dev/null
	@echo -lwinmm

check_posix:
	@mkdir -p ${tmp_p}
	@${CC} mk/posix.c -o ${tmp_p}$@

check_alsa:
	@mkdir -p ${tmp_p}
	@${CC} mk/alsa.c -o ${tmp_p}$@ -lasound
	@echo -lasound

.PHONY: check_win32 check_posix
.PHONY: all test test-all options clean dist install uninstall types dirs
