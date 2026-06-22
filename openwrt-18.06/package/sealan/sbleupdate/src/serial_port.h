#ifndef SERIAL_PORT_H_
#define SERIAL_PORT_H_

#include <stddef.h>
#include <stdint.h>

typedef struct serial_port serial_port_t;

struct serial_port {
#ifdef _WIN32
    void *handle;
#else
    int fd;
#endif
};

int serial_open_port(serial_port_t *port, const char *name, uint32_t baudrate);
void serial_close_port(serial_port_t *port);
int serial_write_all(serial_port_t *port, const uint8_t *data, size_t len, uint32_t timeout_ms);
int serial_read_some(serial_port_t *port, uint8_t *data, size_t capacity, size_t *bytes_read, uint32_t timeout_ms);
int serial_read_exact(serial_port_t *port, uint8_t *data, size_t len, uint32_t timeout_ms);
void serial_sleep_ms(uint32_t delay_ms);
void serial_request_shutdown(void);
void serial_reset_shutdown(void);
int serial_shutdown_requested(void);

#endif
