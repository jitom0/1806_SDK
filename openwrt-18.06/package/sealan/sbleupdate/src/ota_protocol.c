#include "ota_protocol.h"

#include <stdio.h>

#ifdef _WIN32
#define OTA_FOPEN(path, mode, fp)     (fopen_s(&(fp), (path), (mode)) == 0)
#else
#define OTA_FOPEN(path, mode, fp)     (((fp) = fopen((path), (mode))) != NULL)
#endif

uint16_t ota_crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    size_t i;

    while (len-- > 0U) {
        crc ^= (uint16_t)(*data++) << 8;
        for (i = 0; i < 8U; ++i) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

uint32_t ota_crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    size_t i;

    crc = ~crc;
    while (len-- > 0U) {
        crc ^= *data++;
        for (i = 0; i < 8U; ++i) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }

    return ~crc;
}

uint32_t ota_crc32_file(const char *path, uint32_t *size_out)
{
    FILE *fp = NULL;
    uint8_t buffer[1024];
    size_t nread;
    uint32_t crc = 0U;
    uint32_t total = 0U;

    if (!OTA_FOPEN(path, "rb", fp)) {
        return 0U;
    }

    while ((nread = fread(buffer, 1U, sizeof(buffer), fp)) > 0U) {
        crc = ota_crc32_update(crc, buffer, nread);
        total += (uint32_t)nread;
    }

    fclose(fp);
    if (size_out != NULL) {
        *size_out = total;
    }

    return crc;
}

void ota_write_u16_le(uint8_t *ptr, uint16_t value)
{
    ptr[0] = (uint8_t)(value & 0xFFU);
    ptr[1] = (uint8_t)((value >> 8) & 0xFFU);
}

void ota_write_u32_le(uint8_t *ptr, uint32_t value)
{
    ptr[0] = (uint8_t)(value & 0xFFU);
    ptr[1] = (uint8_t)((value >> 8) & 0xFFU);
    ptr[2] = (uint8_t)((value >> 16) & 0xFFU);
    ptr[3] = (uint8_t)((value >> 24) & 0xFFU);
}

uint16_t ota_read_u16_le(const uint8_t *ptr)
{
    return (uint16_t)ptr[0] | ((uint16_t)ptr[1] << 8);
}

uint32_t ota_read_u32_le(const uint8_t *ptr)
{
    return (uint32_t)ptr[0]
         | ((uint32_t)ptr[1] << 8)
         | ((uint32_t)ptr[2] << 16)
         | ((uint32_t)ptr[3] << 24);
}

size_t ota_build_frame(uint8_t type, uint8_t flags, uint16_t seq,
                       const uint8_t *payload, uint16_t payload_len,
                       uint8_t *out_frame, size_t out_capacity)
{
    uint16_t hdr_crc;
    uint32_t data_crc;
    size_t frame_len;

    frame_len = 10U + (size_t)payload_len + 4U;
    if ((payload_len > OTA_PROTOCOL_MAX_PAYLOAD) || (out_capacity < frame_len)) {
        return 0U;
    }

    out_frame[0] = OTA_SOF0;
    out_frame[1] = OTA_SOF1;
    out_frame[2] = type;
    out_frame[3] = flags;
    ota_write_u16_le(&out_frame[4], seq);
    ota_write_u16_le(&out_frame[6], payload_len);
    hdr_crc = ota_crc16_ccitt(&out_frame[2], 6U);
    ota_write_u16_le(&out_frame[8], hdr_crc);

    if ((payload_len > 0U) && (payload != NULL)) {
        size_t i;
        for (i = 0U; i < payload_len; ++i) {
            out_frame[10U + i] = payload[i];
        }
    }

    data_crc = ota_crc32_update(0U, payload, payload_len);
    ota_write_u32_le(&out_frame[10U + payload_len], data_crc);
    return frame_len;
}

int ota_parse_frame(const uint8_t *frame, size_t frame_len, ota_frame_view_t *view)
{
    uint16_t payload_len;
    uint16_t hdr_crc;
    uint16_t hdr_crc_expected;
    uint32_t data_crc;
    uint32_t data_crc_expected;
    size_t expected_len;

    if ((frame == NULL) || (view == NULL) || (frame_len < 14U) || (frame_len > OTA_PROTOCOL_MAX_FRAME_SIZE)) {
        return -1;
    }

    if ((frame[0] != OTA_SOF0) || (frame[1] != OTA_SOF1)) {
        return -1;
    }

    payload_len = ota_read_u16_le(&frame[6]);
    expected_len = 10U + (size_t)payload_len + 4U;
    if (expected_len != frame_len) {
        return -1;
    }

    hdr_crc = ota_read_u16_le(&frame[8]);
    hdr_crc_expected = ota_crc16_ccitt(&frame[2], 6U);
    if (hdr_crc != hdr_crc_expected) {
        return -1;
    }

    data_crc = ota_read_u32_le(&frame[10U + payload_len]);
    data_crc_expected = ota_crc32_update(0U, &frame[10], payload_len);
    if (data_crc != data_crc_expected) {
        return -1;
    }

    view->type = frame[2];
    view->flags = frame[3];
    view->seq = ota_read_u16_le(&frame[4]);
    view->payload = &frame[10];
    view->payload_len = payload_len;
    return 0;
}
