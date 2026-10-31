"""Panel launcher: serve uvicorn on BOTH 127.0.0.1:8070 and [::1]:8070.

Windows resolves "localhost" to ::1 first; with only the IPv4 bind every
localhost request paid a flat ~2s for the refused IPv6 attempt before falling
back. Binding both loopbacks makes localhost connects instant.
"""
import socket
import uvicorn


def _listen(family: int, host: str) -> socket.socket:
    s = socket.socket(family, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((host, 8070))
    s.listen(128)
    return s


def main() -> None:
    socks = [_listen(socket.AF_INET, "127.0.0.1")]
    try:
        socks.append(_listen(socket.AF_INET6, "::1"))
    except OSError:
        pass  # no IPv6 loopback -> IPv4 only, same as before
    cfg = uvicorn.Config("server:app", host="127.0.0.1", port=8070)
    uvicorn.Server(cfg).run(sockets=socks)


if __name__ == "__main__":
    main()
