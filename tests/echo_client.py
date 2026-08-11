import socket
import sys
import time


HOST = "127.0.0.1"
PORT = 25280
PAYLOAD = b"skyuv-echo-baseline\n"


def connect_with_retry() -> socket.socket:
    deadline = time.monotonic() + 5
    while True:
        try:
            return socket.create_connection((HOST, PORT), timeout=1)
        except OSError:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.1)


def main() -> int:
    with connect_with_retry() as client:
        client.sendall(PAYLOAD)
        received = client.recv(1024)
    if received != PAYLOAD:
        print(f"echo 内容不一致：{received!r}", file=sys.stderr)
        return 1
    print("skyuv TCP echo 验证通过")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
