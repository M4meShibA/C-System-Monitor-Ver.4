import json
import socket
import threading
import time


class LiveMonitor:

    def __init__(
        self,
        host="127.0.0.1",
        port=5000,
        max_history=500
    ):
        self.host = host
        self.port = port
        self.max_history = max_history

        self.monitor_data = []

        self.running = False
        self.thread = None

        self.lock = threading.Lock()

    def start(self):

        if self.running:
            return

        self.running = True

        self.thread = threading.Thread(
            target=self._receive_loop,
            daemon=True
        )

        self.thread.start()

    def stop(self):

        self.running = False

        if self.thread is not None:
            self.thread.join(timeout=2)

    def _receive_loop(self):

        while self.running:

            client_socket = None

            try:

                client_socket = socket.socket(
                    socket.AF_INET,
                    socket.SOCK_STREAM
                )

                client_socket.settimeout(5.0)

                client_socket.connect(
                    (self.host, self.port)
                )

                print(
                    f"Connected to C System Monitor: "
                    f"{self.host}:{self.port}"
                )

                buffer = ""

                while self.running:

                    try:

                        data = client_socket.recv(4096)

                    except socket.timeout:

                        continue

                    if not data:

                        print(
                            "C server disconnected. "
                            "Reconnecting..."
                        )

                        break

                    buffer += data.decode(
                        "utf-8",
                        errors="replace"
                    )

                    while "\n" in buffer:

                        message, buffer = buffer.split(
                            "\n",
                            1
                        )

                        message = message.strip()

                        if not message:
                            continue

                        try:

                            values = json.loads(
                                message
                            )

                            monitor_item = {
                                "timestamp": values["timestamp"],
                                "cpu": float(values["cpu"]),
                                "memory": float(values["memory"]),
                                "disk": float(values["disk"]),
                                "network_rx_kbps": float(values["network_rx_kbps"]),
                                "network_tx_kbps": float(values["network_tx_kbps"])
                            }

                            with self.lock:

                                self.monitor_data.append(
                                    monitor_item
                                )

                                if len(self.monitor_data) > self.max_history:

                                    self.monitor_data = (
                                        self.monitor_data[
                                            -self.max_history:
                                        ]
                                    )

                            print(
                                "Received monitor data:",
                                monitor_item
                            )

                        except (
                            json.JSONDecodeError,
                            KeyError,
                            TypeError,
                            ValueError
                        ) as e:

                            print(
                                f"Invalid monitor data: {e}"
                            )

            except ConnectionRefusedError:

                print(
                    "C server is not available. "
                    "Retrying..."
                )

            except OSError as e:

                print(
                    f"Socket error: {e}"
                )

            except Exception as e:

                print(
                    f"Unexpected error: {e}"
                )

            finally:

                if client_socket is not None:

                    try:
                        client_socket.close()

                    except OSError:
                        pass

            if self.running:

                time.sleep(1)

    def get_data(self):

        with self.lock:

            return list(
                self.monitor_data
            )
