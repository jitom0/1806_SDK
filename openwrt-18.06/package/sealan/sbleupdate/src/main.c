#include "ota_protocol.h"
#include "serial_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define OTA_BAUDRATE_DEFAULT          1000000U
#define OTA_FRAME_TIMEOUT_MS          2000U
#define OTA_WRITE_TIMEOUT_MS          1000U
#define OTA_HELLO_ACK_TIMEOUT_MS      5000U
#define OTA_DATA_ACK_TIMEOUT_MS       10000U
#define OTA_EOF_ACK_TIMEOUT_MS        5000U

#ifdef _WIN32
#define OTA_FOPEN(path, mode, fp)     (fopen_s(&(fp), (path), (mode)) == 0)
#else
#define OTA_FOPEN(path, mode, fp)     (((fp) = fopen((path), (mode))) != NULL)
#endif

typedef struct {
    const char *port_name;
    const char *image_path;
    uint32_t baudrate;
} ota_cli_args_t;

static uint64_t get_time_ms(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
#endif
}

#ifdef _WIN32
static BOOL WINAPI ota_console_ctrl_handler(DWORD ctrl_type)
{
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            serial_request_shutdown();
            return TRUE;

        default:
            return FALSE;
    }
}
#endif

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s -p <serial_port> -f <firmware.bin> [-b baudrate]\n", prog);
}

static int parse_args(int argc, char **argv, ota_cli_args_t *args)
{
    int i;

    memset(args, 0, sizeof(*args));
    args->baudrate = OTA_BAUDRATE_DEFAULT;

    for (i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-p") == 0) && (i + 1 < argc)) {
            args->port_name = argv[++i];
        } else if ((strcmp(argv[i], "-f") == 0) && (i + 1 < argc)) {
            args->image_path = argv[++i];
        } else if ((strcmp(argv[i], "-b") == 0) && (i + 1 < argc)) {
            args->baudrate = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else {
            return -1;
        }
    }

    return (args->port_name != NULL && args->image_path != NULL) ? 0 : -1;
}

static int wait_for_text(serial_port_t *port, const char *needle, uint32_t timeout_ms)
{
    char buffer[256];
    size_t used = 0U;
    uint32_t waited = 0U;

    memset(buffer, 0, sizeof(buffer));
    while (waited < timeout_ms) {
        size_t rd = 0U;
        int rc = serial_read_some(port, (uint8_t *)&buffer[used], sizeof(buffer) - used - 1U, &rd, 50U);
        if (rc < 0) {
            return -1;
        }
        if (rc == 0) {
            used += rd;
            buffer[used] = '\0';
            if (strstr(buffer, needle) != NULL) {
                return 0;
            }
            if (used > 200U) {
                memmove(buffer, &buffer[used - 100U], 100U);
                used = 100U;
                buffer[used] = '\0';
            }
        }
        waited += 50U;
    }

    return -1;
}

static int read_protocol_frame(serial_port_t *port, uint8_t *frame, size_t *frame_len)
{
    size_t got = 0U;
    uint8_t byte = 0U;

    while (1) {
        size_t rd = 0U;
        int rc = serial_read_some(port, &byte, 1U, &rd, OTA_FRAME_TIMEOUT_MS);
        if (rc != 0) {
            return -1;
        }
        if (byte != OTA_SOF0) {
            continue;
        }

        rc = serial_read_some(port, &byte, 1U, &rd, OTA_FRAME_TIMEOUT_MS);
        if (rc != 0) {
            return -1;
        }
        if (byte == OTA_SOF1) {
            frame[0] = OTA_SOF0;
            frame[1] = OTA_SOF1;
            got = 2U;
            break;
        }
    }

    while (got < 10U) {
        size_t rd = 0U;
        int rc = serial_read_some(port, frame + got, OTA_PROTOCOL_MAX_FRAME_SIZE - got, &rd, OTA_FRAME_TIMEOUT_MS);
        if (rc != 0) {
            return -1;
        }
        got += rd;
    }

    {
        size_t expected_len = 10U + (size_t)ota_read_u16_le(&frame[6]) + 4U;
        if (expected_len > OTA_PROTOCOL_MAX_FRAME_SIZE) {
            return -1;
        }
        while (got < expected_len) {
            size_t rd = 0U;
            int rc = serial_read_some(port, frame + got, expected_len - got, &rd, OTA_FRAME_TIMEOUT_MS);
            if (rc != 0) {
                return -1;
            }
            got += rd;
        }
    }

    *frame_len = got;
    return 0;
}

static int wait_for_ack(serial_port_t *port, uint16_t seq, uint16_t *next_seq_out, uint32_t timeout_ms)
{
    uint8_t frame[OTA_PROTOCOL_MAX_FRAME_SIZE];
    uint32_t waited = 0U;

    while (waited < timeout_ms) {
        ota_frame_view_t view;
        size_t frame_len;

        if (read_protocol_frame(port, frame, &frame_len) != 0) {
            waited += OTA_FRAME_TIMEOUT_MS;
            continue;
        }
        if (ota_parse_frame(frame, frame_len, &view) != 0) {
            waited += OTA_FRAME_TIMEOUT_MS;
            continue;
        }

        if (view.type == OTA_FRAME_TYPE_NACK) {
            if (view.payload_len >= 8U) {
                fprintf(stderr, "Device NACK: seq=%u status=%u next_seq=%u received=%lu\n",
                        (unsigned)view.seq,
                        (unsigned)view.payload[0],
                        (unsigned)ota_read_u16_le(&view.payload[2]),
                        (unsigned long)ota_read_u32_le(&view.payload[4]));
            } else {
                fprintf(stderr, "Device NACK: seq=%u payload_len=%u\n",
                        (unsigned)view.seq,
                        (unsigned)view.payload_len);
            }
            return -1;
        }

        if ((view.type == OTA_FRAME_TYPE_ACK) && (view.seq == seq) && (view.payload_len >= 4U)) {
            if (next_seq_out != NULL) {
                *next_seq_out = ota_read_u16_le(&view.payload[2]);
            }
            return 0;
        }

        waited += OTA_FRAME_TIMEOUT_MS;
    }

    return -1;
}

static uint32_t ack_timeout_for_frame(uint8_t type)
{
    switch (type) {
        case OTA_FRAME_TYPE_HELLO:
            return OTA_HELLO_ACK_TIMEOUT_MS;

        case OTA_FRAME_TYPE_DATA:
            return OTA_DATA_ACK_TIMEOUT_MS;

        case OTA_FRAME_TYPE_EOF:
            return OTA_EOF_ACK_TIMEOUT_MS;

        default:
            return OTA_FRAME_TIMEOUT_MS;
    }
}

static int send_frame_and_wait_ack(serial_port_t *port, uint8_t type, uint16_t seq, const uint8_t *payload, uint16_t payload_len)
{
    uint8_t frame[OTA_PROTOCOL_MAX_FRAME_SIZE];
    size_t frame_len;
    uint16_t next_seq = 0U;

    frame_len = ota_build_frame(type, 0U, seq, payload, payload_len, frame, sizeof(frame));
    if (frame_len == 0U) {
        return -1;
    }

    if (serial_write_all(port, frame, frame_len, OTA_WRITE_TIMEOUT_MS) != 0) {
        return -1;
    }

    /* 打印发送的内容 */
    // printf("Sent frame: type=0x%02X, seq=%u, len=%zu\n", type, seq, frame_len);
    // for (size_t i = 0; i < frame_len; i++) {
    //     printf("%02X ", frame[i]);
    // }
    // printf("\n\n");

    if (wait_for_ack(port, seq, &next_seq, ack_timeout_for_frame(type)) != 0) {
        return -1;
    }

    if ((type == OTA_FRAME_TYPE_HELLO && next_seq != 1U) ||
        (type == OTA_FRAME_TYPE_DATA && next_seq != (uint16_t)(seq + 1U)) ||
        (type == OTA_FRAME_TYPE_EOF && next_seq != seq)) {
        return -1;
    }

    return 0;
}

static int send_hello(serial_port_t *port, uint32_t image_size, uint32_t image_crc32)
{
    uint8_t payload[16];
    ota_write_u32_le(&payload[0], OTA_PROTOCOL_HELLO_MAGIC);
    ota_write_u32_le(&payload[4], image_size);
    ota_write_u32_le(&payload[8], image_crc32);
    ota_write_u16_le(&payload[12], (uint16_t)OTA_PROTOCOL_MAX_PAYLOAD);
    ota_write_u16_le(&payload[14], OTA_PROTOCOL_VERSION);
    return send_frame_and_wait_ack(port, OTA_FRAME_TYPE_HELLO, 0U, payload, sizeof(payload));
}

static int send_firmware(serial_port_t *port, const char *image_path, uint32_t image_size)
{
    FILE *fp = NULL;
    uint8_t chunk[OTA_PROTOCOL_MAX_PAYLOAD];
    uint16_t seq = 1U;
    uint32_t sent = 0U;

    if (!OTA_FOPEN(image_path, "rb", fp)) {
        return -1;
    }

    while (sent < image_size) {
        size_t remain = (size_t)(image_size - sent);
        size_t chunk_len = remain > sizeof(chunk) ? sizeof(chunk) : remain;
        if (fread(chunk, 1U, chunk_len, fp) != chunk_len) {
            fclose(fp);
            return -1;
        }
        if (send_frame_and_wait_ack(port, OTA_FRAME_TYPE_DATA, seq, chunk, (uint16_t)chunk_len) != 0) {
            fclose(fp);
            return -1;
        }

        sent += (uint32_t)chunk_len;
        ++seq;
        fprintf(stdout, "\rProgress: %lu / %lu", (unsigned long)sent, (unsigned long)image_size);
        fflush(stdout);
    }

    fclose(fp);
    fputc('\n', stdout);
    return seq;
}

static int send_eof(serial_port_t *port, uint16_t seq, uint32_t image_size, uint32_t image_crc32)
{
    uint8_t payload[8];
    ota_write_u32_le(&payload[0], image_size);
    ota_write_u32_le(&payload[4], image_crc32);
    return send_frame_and_wait_ack(port, OTA_FRAME_TYPE_EOF, seq, payload, sizeof(payload));
}

int main(int argc, char **argv)
{
    ota_cli_args_t args;
    serial_port_t port;
    uint32_t image_size = 0U;
    uint32_t image_crc32;
    int next_seq;
    const char *cmd = "AT+OTASTART\r\n";
    uint64_t start_time;
    uint64_t end_time;

#ifdef _WIN32
    port.handle = NULL;
    serial_reset_shutdown();
    if (!SetConsoleCtrlHandler(ota_console_ctrl_handler, TRUE)) {
        fprintf(stderr, "Failed to install console control handler\n");
        return 1;
    }
#else
    port.fd = -1;
#endif

    if (parse_args(argc, argv, &args) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    image_crc32 = ota_crc32_file(args.image_path, &image_size);
    if (image_size == 0U) {
        fprintf(stderr, "Failed to read firmware image: %s\n", args.image_path);
        return 1;
    }

    if (serial_open_port(&port, args.port_name, args.baudrate) != 0) {
        fprintf(stderr, "Failed to open serial port: %s\n", args.port_name);
        return 1;
    }

    printf("Sending command: %s", cmd);
    if (serial_write_all(&port, (const uint8_t *)cmd, strlen(cmd), OTA_WRITE_TIMEOUT_MS) != 0 ||
        wait_for_text(&port, "+OTA:WAIT-HANDSHAKE", 3000U) != 0) {
        fprintf(stderr, "Device did not enter OTA mode\n");
        serial_close_port(&port);
        return 1;
    }

    serial_sleep_ms(20U);
    start_time = get_time_ms();

    if (send_hello(&port, image_size, image_crc32) != 0) {
        fprintf(stderr, "HELLO handshake failed\n");
        serial_close_port(&port);
        return 1;
    }

    next_seq = send_firmware(&port, args.image_path, image_size);
    if (next_seq < 0 || send_eof(&port, (uint16_t)next_seq, image_size, image_crc32) != 0) {
        fprintf(stderr, "OTA transfer failed\n");
        serial_close_port(&port);
        return 1;
    }

    end_time = get_time_ms();

    fprintf(stdout, "OTA transfer completed in %.2f seconds, device should reboot now.\n", (double)(end_time - start_time) / 1000.0);
    serial_close_port(&port);
#ifdef _WIN32
    SetConsoleCtrlHandler(ota_console_ctrl_handler, FALSE);
#endif
    return 0;
}
