/*
 * Dynamic data buffer
 * Copyright (c) 2007-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/trace.h"
#include "utils/fota_buf.h"

#define fota_printf(...)

static void *zalloc(size_t size)
{
    void *n = malloc(size);

    if (n) {
        memset(n, 0, size);
    }

    return n;
}

static int hex2num(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}


int hex2byte(const char *hex)
{
    int a, b;
    a = hex2num(*hex++);

    if (a < 0) {
        return -1;
    }

    b = hex2num(*hex++);

    if (b < 0) {
        return -1;
    }

    return (a << 4) | b;
}


int hexstr2bin(const char *hex, u8 *buf, size_t len)
{
    size_t i;
    int a;
    const char *ipos = hex;
    u8 *opos = buf;

    for (i = 0; i < len; i++) {
        a = hex2byte(ipos);

        if (a < 0) {
            return -1;
        }

        *opos++ = a;
        ipos += 2;
    }

    return 0;
}

#ifdef FOTA_TRACE
#define FOTABUF_MAGIC 0x51a974e3

struct fotabuf_trace {
    unsigned int magic;
} __attribute__((aligned(8)));

static struct fotabuf_trace *fotabuf_get_trace(const struct fotabuf *buf)
{
    return (struct fotabuf_trace *)
           ((const u8 *) buf - sizeof(struct fotabuf_trace));
}
#endif /* FOTA_TRACE */


static void fotabuf_overflow(const struct fotabuf *buf, size_t len)
{
#ifdef FOTA_TRACE
    struct fotabuf_trace *trace = fotabuf_get_trace(buf);

    if (trace->magic != FOTABUF_MAGIC) {
        fota_printf(MSG_ERROR, "fotabuf: invalid magic %x",
                     trace->magic);
    }

#endif /* FOTA_TRACE */
    fota_printf(MSG_ERROR, "fotabuf %p (size=%lu used=%lu) overflow len=%lu",
                 buf, (unsigned long) buf->size, (unsigned long) buf->used,
                 (unsigned long) len);
    fota_trace_show("fotabuf overflow");
    abort();
}


int fotabuf_resize(struct fotabuf **_buf, size_t add_len)
{
    struct fotabuf *buf = *_buf;
#ifdef FOTA_TRACE
    struct fotabuf_trace *trace;
#endif /* FOTA_TRACE */

    if (buf == NULL) {
        *_buf = fotabuf_alloc(add_len);
        return *_buf == NULL ? -1 : 0;
    }

#ifdef FOTA_TRACE
    trace = fotabuf_get_trace(buf);

    if (trace->magic != FOTABUF_MAGIC) {
        fota_printf(MSG_ERROR, "fotabuf: invalid magic %x",
                     trace->magic);
        fota_trace_show("fotabuf_resize invalid magic");
        abort();
    }

#endif /* FOTA_TRACE */

    if (buf->used + add_len > buf->size) {
        unsigned char *nbuf;

        if (buf->flags & FOTABUF_FLAG_EXT_DATA) {
            nbuf = os_realloc(buf->buf, buf->used + add_len);

            if (nbuf == NULL) {
                return -1;
            }

            os_memset(nbuf + buf->used, 0, add_len);
            buf->buf = nbuf;
        } else {
#ifdef FOTA_TRACE
            nbuf = os_realloc(trace, sizeof(struct fotabuf_trace) +
                              sizeof(struct fotabuf) +
                              buf->used + add_len);

            if (nbuf == NULL) {
                return -1;
            }

            trace = (struct fotabuf_trace *) nbuf;
            buf = (struct fotabuf *)(trace + 1);
            os_memset(nbuf + sizeof(struct fotabuf_trace) +
                      sizeof(struct fotabuf) + buf->used, 0,
                      add_len);
#else /* FOTA_TRACE */
            nbuf = realloc(buf, sizeof(struct fotabuf) +
                           buf->used + add_len);

            if (nbuf == NULL) {
                return -1;
            }

            buf = (struct fotabuf *) nbuf;
            os_memset(nbuf + sizeof(struct fotabuf) + buf->used, 0,
                      add_len);
#endif /* FOTA_TRACE */
            buf->buf = (u8 *)(buf + 1);
            *_buf = buf;
        }

        buf->size = buf->used + add_len;
    }

    return 0;
}


/**
 * fotabuf_alloc - Allocate a fotabuf of the given size
 * @len: Length for the allocated buffer
 * Returns: Buffer to the allocated fotabuf or %NULL on failure
 */
struct fotabuf *fotabuf_alloc(size_t len)
{
#ifdef FOTA_TRACE
    struct fotabuf_trace *trace = zalloc(sizeof(struct fotabuf_trace) +
                                          sizeof(struct fotabuf) + len);
    struct fotabuf *buf;

    if (trace == NULL) {
        return NULL;
    }

    trace->magic = FOTABUF_MAGIC;
    buf = (struct fotabuf *)(trace + 1);
#else /* FOTA_TRACE */
    struct fotabuf *buf = zalloc(sizeof(struct fotabuf) + len);

    if (buf == NULL) {
        return NULL;
    }

#endif /* FOTA_TRACE */

    buf->size = len;
    buf->buf = (u8 *)(buf + 1);
    return buf;
}


struct fotabuf *fotabuf_alloc_ext_data(u8 *data, size_t len)
{
#ifdef FOTA_TRACE
    struct fotabuf_trace *trace = zalloc(sizeof(struct fotabuf_trace) +
                                          sizeof(struct fotabuf));
    struct fotabuf *buf;

    if (trace == NULL) {
        return NULL;
    }

    trace->magic = FOTABUF_MAGIC;
    buf = (struct fotabuf *)(trace + 1);
#else /* FOTA_TRACE */
    struct fotabuf *buf = zalloc(sizeof(struct fotabuf));

    if (buf == NULL) {
        return NULL;
    }

#endif /* FOTA_TRACE */

    buf->size = len;
    buf->used = len;
    buf->buf = data;
    buf->flags |= FOTABUF_FLAG_EXT_DATA;

    return buf;
}


struct fotabuf *fotabuf_alloc_copy(const void *data, size_t len)
{
    struct fotabuf *buf = fotabuf_alloc(len);

    if (buf) {
        fotabuf_put_data(buf, data, len);
    }

    return buf;
}


struct fotabuf *fotabuf_dup(const struct fotabuf *src)
{
    struct fotabuf *buf = fotabuf_alloc(fotabuf_len(src));

    if (buf) {
        fotabuf_put_data(buf, fotabuf_head(src), fotabuf_len(src));
    }

    return buf;
}


/**
 * fotabuf_free - Free a fotabuf
 * @buf: fotabuf buffer
 */
void fotabuf_free(struct fotabuf *buf)
{
#ifdef FOTA_TRACE
    struct fotabuf_trace *trace;

    if (buf == NULL) {
        return;
    }

    trace = fotabuf_get_trace(buf);

    if (trace->magic != FOTABUF_MAGIC) {
        fota_printf(MSG_ERROR, "fotabuf_free: invalid magic %x",
                     trace->magic);
        fota_trace_show("fotabuf_free magic mismatch");
        abort();
    }

    if (buf->flags & FOTABUF_FLAG_EXT_DATA) {
        os_free(buf->buf);
    }

    os_free(trace);
#else /* FOTA_TRACE */

    if (buf == NULL) {
        return;
    }

    if (buf->flags & FOTABUF_FLAG_EXT_DATA) {
        os_free(buf->buf);
    }

    os_free(buf);
#endif /* FOTA_TRACE */
}


void fotabuf_clear_free(struct fotabuf *buf)
{
    if (buf) {
        os_memset(fotabuf_mhead(buf), 0, fotabuf_len(buf));
        fotabuf_free(buf);
    }
}


void *fotabuf_put(struct fotabuf *buf, size_t len)
{
    void *tmp = fotabuf_mhead_u8(buf) + fotabuf_len(buf);
    buf->used += len;

    if (buf->used > buf->size) {
        fotabuf_overflow(buf, len);
    }

    return tmp;
}


/**
 * fotabuf_concat - Concatenate two buffers into a newly allocated one
 * @a: First buffer
 * @b: Second buffer
 * Returns: fotabuf with concatenated a + b data or %NULL on failure
 *
 * Both buffers a and b will be freed regardless of the return value. Input
 * buffers can be %NULL which is interpreted as an empty buffer.
 */
struct fotabuf *fotabuf_concat(struct fotabuf *a, struct fotabuf *b)
{
    struct fotabuf *n = NULL;
    size_t len = 0;

    if (b == NULL) {
        return a;
    }

    if (a) {
        len += fotabuf_len(a);
    }

    len += fotabuf_len(b);

    n = fotabuf_alloc(len);

    if (n) {
        if (a) {
            fotabuf_put_buf(n, a);
        }

        fotabuf_put_buf(n, b);
    }

    fotabuf_free(a);
    fotabuf_free(b);

    return n;
}


/**
 * fotabuf_zeropad - Pad buffer with 0x00 octets (prefix) to specified length
 * @buf: Buffer to be padded
 * @len: Length for the padded buffer
 * Returns: fotabuf padded to len octets or %NULL on failure
 *
 * If buf is longer than len octets or of same size, it will be returned as-is.
 * Otherwise a new buffer is allocated and prefixed with 0x00 octets followed
 * by the source data. The source buffer will be freed on error, i.e., caller
 * will only be responsible on freeing the returned buffer. If buf is %NULL,
 * %NULL will be returned.
 */
struct fotabuf *fotabuf_zeropad(struct fotabuf *buf, size_t len)
{
    struct fotabuf *ret;
    size_t blen;

    if (buf == NULL) {
        return NULL;
    }

    blen = fotabuf_len(buf);

    if (blen >= len) {
        return buf;
    }

    ret = fotabuf_alloc(len);

    if (ret) {
        os_memset(fotabuf_put(ret, len - blen), 0, len - blen);
        fotabuf_put_buf(ret, buf);
    }

    fotabuf_free(buf);

    return ret;
}


void fotabuf_printf(struct fotabuf *buf, char *fmt, ...)
{
    va_list ap;
    void *tmp = fotabuf_mhead_u8(buf) + fotabuf_len(buf);
    int res;

    va_start(ap, fmt);
    res = vsnprintf(tmp, buf->size - buf->used, fmt, ap);
    va_end(ap);

    if (res < 0 || (size_t) res >= buf->size - buf->used) {
        fotabuf_overflow(buf, res);
    }

    buf->used += res;
}


/**
 * fotabuf_parse_bin - Parse a null terminated string of binary data to a fotabuf
 * @buf: Buffer with null terminated string (hexdump) of binary data
 * Returns: fotabuf or %NULL on failure
 *
 * The string len must be a multiple of two and contain only hexadecimal digits.
 */
struct fotabuf *fotabuf_parse_bin(const char *buf)
{
    size_t len;
    struct fotabuf *ret;

    len = os_strlen(buf);

    if (len & 0x01) {
        return NULL;
    }

    len /= 2;

    ret = fotabuf_alloc(len);

    if (ret == NULL) {
        return NULL;
    }

    if (hexstr2bin(buf, fotabuf_put(ret, len), len)) {
        fotabuf_free(ret);
        return NULL;
    }

    return ret;
}
