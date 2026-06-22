#include "serial_port.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE g_shutdown_event = NULL;

static int serial_ensure_shutdown_event(void)
{
    if (g_shutdown_event == NULL) {
        g_shutdown_event = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (g_shutdown_event == NULL) {
            return -1;
        }
    }

    return 0;
}

void serial_request_shutdown(void)
{
    if (serial_ensure_shutdown_event() == 0) {
        SetEvent(g_shutdown_event);
    }
}

void serial_reset_shutdown(void)
{
    if (serial_ensure_shutdown_event() == 0) {
        ResetEvent(g_shutdown_event);
    }
}

int serial_shutdown_requested(void)
{
    if (serial_ensure_shutdown_event() != 0) {
        return 0;
    }

    return WaitForSingleObject(g_shutdown_event, 0) == WAIT_OBJECT_0;
}

static int serial_wait_overlapped(HANDLE file_handle, OVERLAPPED *ov, uint32_t timeout_ms)
{
    HANDLE handles[2];
    DWORD wait_rc;

    if (serial_ensure_shutdown_event() != 0) {
        return -1;
    }

    handles[0] = ov->hEvent;
    handles[1] = g_shutdown_event;

    wait_rc = WaitForMultipleObjects(2, handles, FALSE, timeout_ms);
    if (wait_rc == WAIT_OBJECT_0) {
        return 0;
    }

    CancelIoEx(file_handle, ov);
    WaitForSingleObject(ov->hEvent, INFINITE);

    if (wait_rc == WAIT_OBJECT_0 + 1) {
        return -2;
    }

    return 1;
}

static DCB serial_make_dcb(uint32_t baudrate)
{
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    dcb.BaudRate = baudrate;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    return dcb;
}

static int serial_make_port_name(const char *name, char *out_name, size_t out_size)
{
    if ((name == NULL) || (out_name == NULL) || (out_size == 0U)) {
        return -1;
    }

    if ((strncmp(name, "\\\\.\\", 4) == 0) || (strncmp(name, "\\\\?\\", 4) == 0)) {
        return snprintf(out_name, out_size, "%s", name) < (int)out_size ? 0 : -1;
    }

    if ((_strnicmp(name, "COM", 3) == 0)) {
        return snprintf(out_name, out_size, "\\\\.\\%s", name) < (int)out_size ? 0 : -1;
    }

    return snprintf(out_name, out_size, "%s", name) < (int)out_size ? 0 : -1;
}

int serial_open_port(serial_port_t *port, const char *name, uint32_t baudrate)
{
    DCB dcb;
    COMMTIMEOUTS timeouts;
    char port_name[64];

    if ((port == NULL) || (name == NULL)) {
        return -1;
    }

    if (serial_ensure_shutdown_event() != 0) {
        return -1;
    }

    if (serial_make_port_name(name, port_name, sizeof(port_name)) != 0) {
        return -1;
    }

    port->handle = CreateFileA(port_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (port->handle == INVALID_HANDLE_VALUE) {
        port->handle = NULL;
        return -1;
    }

    dcb = serial_make_dcb(baudrate);
    if (!GetCommState((HANDLE)port->handle, &dcb)) {
        serial_close_port(port);
        return -1;
    }
    dcb = serial_make_dcb(baudrate);
    if (!SetCommState((HANDLE)port->handle, &dcb)) {
        serial_close_port(port);
        return -1;
    }

    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutConstant = 20;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = 20;
    timeouts.WriteTotalTimeoutMultiplier = 1;
    if (!SetCommTimeouts((HANDLE)port->handle, &timeouts)) {
        serial_close_port(port);
        return -1;
    }

    PurgeComm((HANDLE)port->handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return 0;
}

void serial_close_port(serial_port_t *port)
{
    if ((port != NULL) && (port->handle != NULL)) {
        CloseHandle((HANDLE)port->handle);
        port->handle = NULL;
    }
}

int serial_write_all(serial_port_t *port, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    size_t total = 0U;
    uint32_t start = GetTickCount();

    while (total < len) {
        DWORD written = 0;
        OVERLAPPED ov;
        BOOL write_ok;
        DWORD err;
        uint32_t elapsed = GetTickCount() - start;
        uint32_t remain = (elapsed >= timeout_ms) ? 0U : (timeout_ms - elapsed);

        if (serial_shutdown_requested()) {
            return -2;
        }
        if (remain == 0U) {
            return -1;
        }

        memset(&ov, 0, sizeof(ov));
        ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (ov.hEvent == NULL) {
            return -1;
        }

        write_ok = WriteFile((HANDLE)port->handle, data + total, (DWORD)(len - total), &written, &ov);
        if (!write_ok) {
            err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                int wait_rc = serial_wait_overlapped((HANDLE)port->handle, &ov, remain);
                if (wait_rc != 0) {
                    CloseHandle(ov.hEvent);
                    return wait_rc == -2 ? -2 : -1;
                }
                if (!GetOverlappedResult((HANDLE)port->handle, &ov, &written, FALSE)) {
                    CloseHandle(ov.hEvent);
                    return -1;
                }
            } else if (err == ERROR_OPERATION_ABORTED) {
                CloseHandle(ov.hEvent);
                return -2;
            } else {
                CloseHandle(ov.hEvent);
                return -1;
            }
        }

        CloseHandle(ov.hEvent);
        total += (size_t)written;
        if ((GetTickCount() - start) > timeout_ms) {
            return -1;
        }
    }

    return 0;
}

int serial_read_some(serial_port_t *port, uint8_t *data, size_t capacity, size_t *bytes_read, uint32_t timeout_ms)
{
    DWORD read_len = 0;
    OVERLAPPED ov;
    BOOL read_ok;
    DWORD err;

    if (bytes_read != NULL) {
        *bytes_read = 0U;
    }

    if (serial_shutdown_requested()) {
        return -2;
    }

    memset(&ov, 0, sizeof(ov));
    ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (ov.hEvent == NULL) {
        return -1;
    }

    read_ok = ReadFile((HANDLE)port->handle, data, (DWORD)capacity, &read_len, &ov);
    if (!read_ok) {
        err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            int wait_rc = serial_wait_overlapped((HANDLE)port->handle, &ov, timeout_ms);
            if (wait_rc != 0) {
                CloseHandle(ov.hEvent);
                return wait_rc == -2 ? -2 : (wait_rc == 1 ? 1 : -1);
            }
            if (!GetOverlappedResult((HANDLE)port->handle, &ov, &read_len, FALSE)) {
                CloseHandle(ov.hEvent);
                return -1;
            }
        } else if (err == ERROR_OPERATION_ABORTED) {
            CloseHandle(ov.hEvent);
            return -2;
        } else {
            CloseHandle(ov.hEvent);
            return -1;
        }
    }

    CloseHandle(ov.hEvent);

    if (read_len > 0U) {
        if (bytes_read != NULL) {
            *bytes_read = (size_t)read_len;
        }
        return 0;
    }

    return 1;
}

int serial_read_exact(serial_port_t *port, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    size_t total = 0U;
    uint32_t start = GetTickCount();

    while (total < len) {
        size_t chunk = 0U;
        int rc = serial_read_some(port, data + total, len - total, &chunk, 20U);
        if (rc == -2) {
            return -2;
        }
        if (rc < 0) {
            return -1;
        }
        if (rc == 0) {
            total += chunk;
        }
        if ((GetTickCount() - start) > timeout_ms) {
            return -1;
        }
    }

    return 0;
}

void serial_sleep_ms(uint32_t delay_ms)
{
    Sleep(delay_ms);
}

#else

#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static speed_t serial_baud_to_native(uint32_t baudrate)
{
    switch (baudrate) {
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
#ifdef B1000000
        case 1000000: return B1000000;
#endif
        default: return B115200;
    }
}

int serial_open_port(serial_port_t *port, const char *name, uint32_t baudrate)
{
    struct termios tio;
    speed_t speed;

    if ((port == NULL) || (name == NULL)) {
        return -1;
    }

    port->fd = open(name, O_RDWR | O_NOCTTY | O_SYNC);
    if (port->fd < 0) {
        return -1;
    }

    memset(&tio, 0, sizeof(tio));
    if (tcgetattr(port->fd, &tio) != 0) {
        serial_close_port(port);
        return -1;
    }

    speed = serial_baud_to_native(baudrate);
    cfsetospeed(&tio, speed);
    cfsetispeed(&tio, speed);

    tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS8;
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(port->fd, TCSANOW, &tio) != 0) {
        serial_close_port(port);
        return -1;
    }

    tcflush(port->fd, TCIOFLUSH);
    return 0;
}

void serial_close_port(serial_port_t *port)
{
    if ((port != NULL) && (port->fd >= 0)) {
        close(port->fd);
        port->fd = -1;
    }
}

int serial_write_all(serial_port_t *port, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    size_t total = 0U;
    struct timespec start;

    clock_gettime(CLOCK_MONOTONIC, &start);
    while (total < len) {
        ssize_t written = write(port->fd, data + total, len - total);
        struct timespec now;
        uint64_t elapsed_ms;

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += (size_t)written;

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed_ms = (uint64_t)(now.tv_sec - start.tv_sec) * 1000ULL
                   + (uint64_t)(now.tv_nsec - start.tv_nsec) / 1000000ULL;
        if (elapsed_ms > timeout_ms) {
            return -1;
        }
    }

    return 0;
}

int serial_read_some(serial_port_t *port, uint8_t *data, size_t capacity, size_t *bytes_read, uint32_t timeout_ms)
{
    fd_set readfds;
    struct timeval tv;
    int rc;

    if (bytes_read != NULL) {
        *bytes_read = 0U;
    }

    FD_ZERO(&readfds);
    FD_SET(port->fd, &readfds);
    tv.tv_sec = (long)(timeout_ms / 1000U);
    tv.tv_usec = (long)((timeout_ms % 1000U) * 1000U);

    rc = select(port->fd + 1, &readfds, NULL, NULL, &tv);
    if (rc < 0) {
        return -1;
    }
    if (rc == 0) {
        return 1;
    }

    {
        ssize_t rd = read(port->fd, data, capacity);
        if (rd < 0) {
            if (errno == EINTR) {
                return 1;
            }
            return -1;
        }

        if (bytes_read != NULL) {
            *bytes_read = (size_t)rd;
        }
    }

    return 0;
}

int serial_read_exact(serial_port_t *port, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    size_t total = 0U;
    struct timespec start;

    clock_gettime(CLOCK_MONOTONIC, &start);
    while (total < len) {
        size_t chunk = 0U;
        struct timespec now;
        uint64_t elapsed_ms;
        int rc = serial_read_some(port, data + total, len - total, &chunk, 20U);
        if (rc < 0) {
            return -1;
        }
        if (rc == 0) {
            total += chunk;
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed_ms = (uint64_t)(now.tv_sec - start.tv_sec) * 1000ULL
                   + (uint64_t)(now.tv_nsec - start.tv_nsec) / 1000000ULL;
        if (elapsed_ms > timeout_ms) {
            return -1;
        }
    }

    return 0;
}

void serial_sleep_ms(uint32_t delay_ms)
{
    struct timespec req;
    req.tv_sec = (time_t)(delay_ms / 1000U);
    req.tv_nsec = (long)((delay_ms % 1000U) * 1000000U);
    nanosleep(&req, NULL);
}

void serial_request_shutdown(void)
{
}

void serial_reset_shutdown(void)
{
}

int serial_shutdown_requested(void)
{
    return 0;
}

#endif
