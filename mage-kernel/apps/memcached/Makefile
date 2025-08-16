.DEFAULT: memcached
.PHONY: module
module: memcached
	echo '/memcached: $${MODULE_DIR}/memcached' > usr.manifest
	ldd memcached | grep libevent | \
		sed 's/ *[^ ] *\(.*\) => \(.*\) .*/\/\1: \2/' \
		>> usr.manifest
	: > bootfs.manifest


sources := assoc.c cache.c daemon.c hash.c items.c memcached.c slabs.c \
	stats.c thread.c util.c jenkins_hash.c murmur3_hash.c \
	test_thread.c zipf/zipf.c zipf/rand.c

sources := $(addprefix src/, $(sources))

$(sources): src

CFLAGS = -std=gnu99 -levent -lpthread -lm \
	-static -static-libgcc -static-libstdc++ \
	-pthread -Wl,--whole-archive -lpthread -Wl,--no-whole-archive \
	-fpie -rdynamic -DHAVE_CONFIG_H -D_GNU_SOURCE
	
LDFLAGS = -pie

# Compilation flags only
CFLAGS = -std=gnu99 -static -static-libgcc -static-libstdc++ \
	-pthread -fpie -rdynamic -DHAVE_CONFIG_H -D_GNU_SOURCE

# Linking flags and libraries
LDFLAGS = -pie -static -static-libgcc -static-libstdc++ \
	-Wl,--whole-archive -lpthread -Wl,--no-whole-archive

LIBS = -levent -lpthread -lm

CFLAGS += -g -ggdb

# To use a shared object instead of PIE (e.g., we needed to do this because
# of issue #689), use the following instead:
#CFLAGS = -std=gnu99 -fpic -rdynamic -DHAVE_CONFIG_H
#LDFLAGS = -shared

memcached: $(sources)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(sources) $(LIBS) -DSLEEP_TIME=$(SLEEP_TIME)
	echo sleep time $(SLEEP_TIME)

clean:
	rm -rf upstream src
	rm -f bootfs.manifest usr.manifest module.pyc memcached
