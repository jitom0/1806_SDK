#ifndef OTA_PROTOCOL_H_
#define OTA_PROTOCOL_H_

#include <stdint.h>
#include <stddef.h>

#define OTA_SOF0                       0x55
#define OTA_SOF1                       0xAA
#define OTA_PROTOCOL_VERSION           0x01
#define OTA_PROTOCOL_HELLO_MAGIC       0x3141544FU
#define OTA_PROTOCOL_MAX_PAYLOAD       490U
#define OTA_PROTOCOL_MAX_FRAME_SIZE    (OTA_PROTOCOL_MAX_PAYLOAD + 14U)

#define OTA_FRAME_TYPE_HELLO           0x01
#define OTA_FRAME_TYPE_DATA            0x02
#define OTA_FRAME_TYPE_EOF             0x03
#define OTA_FRAME_TYPE_ABORT           0x04
#define OTA_FRAME_TYPE_ACK             0x81
#define OTA_FRAME_TYPE_NACK            0x82

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t seq;
    const uint8_t *payload;
    uint16_t payload_len;
} ota_frame_view_t;

uint16_t ota_crc16_ccitt(const uint8_t *data, size_t len);
uint32_t ota_crc32_update(uint32_t crc, const uint8_t *data, size_t len);
uint32_t ota_crc32_file(const char *path, uint32_t *size_out);
void ota_write_u16_le(uint8_t *ptr, uint16_t value);
void ota_write_u32_le(uint8_t *ptr, uint32_t value);
uint16_t ota_read_u16_le(const uint8_t *ptr);
uint32_t ota_read_u32_le(const uint8_t *ptr);
size_t ota_build_frame(uint8_t type, uint8_t flags, uint16_t seq,
                       const uint8_t *payload, uint16_t payload_len,
                       uint8_t *out_frame, size_t out_capacity);
int ota_parse_frame(const uint8_t *frame, size_t frame_len, ota_frame_view_t *view);

#endif
