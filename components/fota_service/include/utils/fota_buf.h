/*
 * Dynamic data buffer
 * Copyright (c) 2007-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef FOTABUF_H
#define FOTABUF_H

#include <stdint.h>

/* fotabuf::buf is a pointer to external data */
#define FOTABUF_FLAG_EXT_DATA BIT(0)

/*
 * Internal data structure for fotabuf. Please do not touch this directly from
 * elsewhere. This is only defined in header file to allow inline functions
 * from this file to access data.
 */
struct fotabuf {
    size_t size; /* total size of the allocated buffer */
    size_t used; /* length of data in the buffer */
    uint8_t *buf; /* pointer to the head of the buffer */
    unsigned int flags;
    /* optionally followed by the allocated buffer */
};


int fotabuf_resize(struct fotabuf **buf, size_t add_len);
struct fotabuf *fotabuf_alloc(size_t len);
struct fotabuf *fotabuf_alloc_ext_data(uint8_t *data, size_t len);
struct fotabuf *fotabuf_alloc_copy(const void *data, size_t len);
struct fotabuf *fotabuf_dup(const struct fotabuf *src);
void fotabuf_free(struct fotabuf *buf);
void fotabuf_clear_free(struct fotabuf *buf);
void *fotabuf_put(struct fotabuf *buf, size_t len);
struct fotabuf *fotabuf_concat(struct fotabuf *a, struct fotabuf *b);
struct fotabuf *fotabuf_zeropad(struct fotabuf *buf, size_t len);
void fotabuf_printf(struct fotabuf *buf, char *fmt, ...) PRINTF_FORMAT(2, 3);
struct fotabuf *fotabuf_parse_bin(const char *buf);


/**
 * fotabuf_size - Get the currently allocated size of a fotabuf buffer
 * @buf: fotabuf buffer
 * Returns: Currently allocated size of the buffer
 */
static inline size_t fotabuf_size(const struct fotabuf *buf)
{
    return buf->size;
}

/**
 * fotabuf_len - Get the current length of a fotabuf buffer data
 * @buf: fotabuf buffer
 * Returns: Currently used length of the buffer
 */
static inline size_t fotabuf_len(const struct fotabuf *buf)
{
    return buf->used;
}

/**
 * fotabuf_tailroom - Get size of available tail room in the end of the buffer
 * @buf: fotabuf buffer
 * Returns: Tail room (in bytes) of available space in the end of the buffer
 */
static inline size_t fotabuf_tailroom(const struct fotabuf *buf)
{
    return buf->size - buf->used;
}

/**
 * fotabuf_head - Get pointer to the head of the buffer data
 * @buf: fotabuf buffer
 * Returns: Pointer to the head of the buffer data
 */
static inline const void *fotabuf_head(const struct fotabuf *buf)
{
    return buf->buf;
}

static inline const u8 *fotabuf_head_u8(const struct fotabuf *buf)
{
    return (const u8 *) fotabuf_head(buf);
}

/**
 * fotabuf_mhead - Get modifiable pointer to the head of the buffer data
 * @buf: fotabuf buffer
 * Returns: Pointer to the head of the buffer data
 */
static inline void *fotabuf_mhead(struct fotabuf *buf)
{
    return buf->buf;
}

static inline u8 *fotabuf_mhead_u8(struct fotabuf *buf)
{
    return (u8 *) fotabuf_mhead(buf);
}

static inline void fotabuf_put_u8(struct fotabuf *buf, u8 data)
{
    u8 *pos = (u8 *) fotabuf_put(buf, 1);
    *pos = data;
}

static inline void fotabuf_put_le16(struct fotabuf *buf, u16 data)
{
    u8 *pos = (u8 *) fotabuf_put(buf, 2);
    FOTA_PUT_LE16(pos, data);
}

static inline void fotabuf_put_le32(struct fotabuf *buf, u32 data)
{
    u8 *pos = (u8 *) fotabuf_put(buf, 4);
    FOTA_PUT_LE32(pos, data);
}

static inline void fotabuf_put_be16(struct fotabuf *buf, u16 data)
{
    u8 *pos = (u8 *) fotabuf_put(buf, 2);
    FOTA_PUT_BE16(pos, data);
}

static inline void fotabuf_put_be24(struct fotabuf *buf, u32 data)
{
    u8 *pos = (u8 *) fotabuf_put(buf, 3);
    FOTA_PUT_BE24(pos, data);
}

static inline void fotabuf_put_be32(struct fotabuf *buf, u32 data)
{
    u8 *pos = (u8 *) fotabuf_put(buf, 4);
    FOTA_PUT_BE32(pos, data);
}

static inline void fotabuf_put_data(struct fotabuf *buf, const void *data,
                                     size_t len)
{
    if (data) {
        os_memcpy(fotabuf_put(buf, len), data, len);
    }
}

static inline void fotabuf_put_buf(struct fotabuf *dst,
                                    const struct fotabuf *src)
{
    fotabuf_put_data(dst, fotabuf_head(src), fotabuf_len(src));
}

static inline void fotabuf_set(struct fotabuf *buf, const void *data, size_t len)
{
    buf->buf = (u8 *) data;
    buf->flags = FOTABUF_FLAG_EXT_DATA;
    buf->size = buf->used = len;
}

static inline void fotabuf_put_str(struct fotabuf *dst, const char *str)
{
    fotabuf_put_data(dst, str, os_strlen(str));
}

#endif /* FOTABUF_H */
