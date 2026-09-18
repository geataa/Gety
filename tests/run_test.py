import http.server
import socketserver
import os
import sys
import threading
import subprocess
import time

PORT = 8999
TEST_FILE = "test_data.bin"
FILE_SIZE = 5 * 1024 * 1024 # 5 MB

class RangeHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Accept-Ranges', 'bytes')
        super().end_headers()

    def do_GET(self):
        path = self.translate_path(self.path)
        if not os.path.exists(path) or not os.path.isfile(path):
            self.send_error(404, "File not found")
            return

        file_size = os.path.getsize(path)
        range_header = self.headers.get('Range')

        if not range_header:
            super().do_GET()
            return

        try:
            # Parse Range: bytes=start-end
            range_type, range_val = range_header.strip().split('=')
            if range_type != 'bytes':
                self.send_error(400, "Bad Request")
                return

            parts = range_val.split('-')
            start = int(parts[0]) if parts[0] else 0
            end = int(parts[1]) if parts[1] else (file_size - 1)

            if start >= file_size or end >= file_size or start > end:
                self.send_response(416, "Range Not Satisfiable")
                self.send_header('Content-Range', f'bytes */{file_size}')
                self.end_headers()
                return

            length = end - start + 1

            self.send_response(206, "Partial Content")
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Content-Range', f'bytes {start}-{end}/{file_size}')
            self.send_header('Content-Length', str(length))
            self.end_headers()

            with open(path, 'rb') as f:
                f.seek(start)
                bytes_left = length
                while bytes_left > 0:
                    chunk = f.read(min(bytes_left, 64 * 1024))
                    if not chunk:
                        break
                    self.wfile.write(chunk)
                    bytes_left -= len(chunk)
        except Exception as e:
            # Client disconnect
            pass

def main():
    # 1. Create deterministic test file
    print(f"Creating {FILE_SIZE} bytes test file: {TEST_FILE}...")
    with open(TEST_FILE, "wb") as f:
        pattern = bytes([i % 256 for i in range(1024)])
        for _ in range(FILE_SIZE // len(pattern)):
            f.write(pattern)

    # 2. Start HTTP server in background thread
    server = socketserver.TCPServer(("127.0.0.1", PORT), RangeHTTPRequestHandler)
    server_thread = threading.Thread(target=server.serve_forever)
    server_thread.daemon = True
    server_thread.start()
    print(f"HTTP Server running at http://127.0.0.1:{PORT} with Range support.")

    # 3. Run test_engine.exe
    time.sleep(0.5)
    test_exe = os.path.abspath("build/Release/test_engine.exe")
    print(f"Running test executable: {test_exe}...")
    proc = subprocess.run([test_exe, str(PORT)], capture_output=False)

    # 4. Shutdown server
    server.shutdown()
    server.server_close()

    # 5. Cleanup
    if os.path.exists(TEST_FILE):
        os.remove(TEST_FILE)

    if proc.returncode == 0:
        print("\n>>> ALL ENGINE TESTS PASSED WITH 100% SUCCESS! <<<")
        sys.exit(0)
    else:
        print(f"\n>>> TEST FAILED WITH EXIT CODE: {proc.returncode} <<<")
        sys.exit(proc.returncode)

if __name__ == "__main__":
    main()
