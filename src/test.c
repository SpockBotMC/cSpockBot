#include "datautils.h"
#include "logc/log.h"
#include "sds.h"
#include <stdlib.h>

sds ByteToHex(void *ptr, size_t len) {
    sds str = sdsempty();
	unsigned char *bytes = ptr;
	for (size_t i = 0; i < len; i++) {
		if (i > 0) str = sdscatprintf(str, " ");
		str = sdscatprintf(str, "%02X", bytes[i]);
	}
	return str;
}

int main(int argc, char *argv[]) {
    char *buf = calloc(1, 1024);
	enc_varint(buf, 0x8F);
	size_t size = walk_varint(buf, 1024);
	log_debug("Walk len: %zu", size);
	sds str = ByteToHex(buf, size);
	log_debug("Hex: %s", str);
	sdsfree(str);
	int32_t varint;
	dec_varint(&varint, buf);
	log_debug("Varint: %d", varint);
    free(buf);
}