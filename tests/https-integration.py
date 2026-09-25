"""Local, network-independent certificate/framing tests for WPM's TLS transport."""
import argparse
import os
from pathlib import Path
import socket
import ssl
import subprocess
import threading

parser = argparse.ArgumentParser()
parser.add_argument('--probe', required=True)
parser.add_argument('--fixtures', required=True)
args = parser.parse_args()
fixtures = Path(args.fixtures)
payload = bytes(range(256)) * 1024


class Server:
    def __init__(self, certificate):
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version = ssl.TLSVersion.TLSv1_2
        self.context.maximum_version = ssl.TLSVersion.TLSv1_2
        self.context.load_cert_chain(fixtures / (certificate + '.pem'), fixtures / (certificate + '.key'))
        self.socket = socket.socket()
        self.socket.bind(('127.0.0.1', 0))
        self.port = self.socket.getsockname()[1]
        self.socket.listen()
        self.socket.settimeout(0.2)
        self.stopped = False
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        while not self.stopped:
            try:
                client, _ = self.socket.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            try:
                with self.context.wrap_socket(client, server_side=True) as stream:
                    stream.settimeout(5)
                    request = b''
                    while b'\r\n\r\n' not in request:
                        data = stream.recv(4096)
                        if not data or len(request) > 16384:
                            raise OSError('Invalid request')
                        request += data
                    path = request.split(b' ')[1].decode('ascii')
                    response = self.response(path)
                    stream.sendall(response)
                    if path == '/close':
                        try:
                            stream.unwrap()
                        except (ssl.SSLError, OSError):
                            pass
            except (ssl.SSLError, OSError):
                client.close()

    def response(self, path):
        if path == '/valid':
            return b'HTTP/1.1 200 OK\r\nContent-Length: ' + str(len(payload)).encode() + b'\r\n\r\n' + payload
        if path == '/chunked':
            return b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n' + (
                b'20000;test=yes\r\n' + payload[:131072] + b'\r\n20000\r\n' + payload[131072:] +
                b'\r\n0\r\nX-Trailer: okay\r\n\r\n')
        if path == '/interim':
            return b'HTTP/1.1 100 Continue\r\n\r\n' + self.response('/valid')
        if path in ('/close', '/abrupt'):
            return b'HTTP/1.1 200 OK\r\n\r\n' + payload
        if path == '/relative':
            return b'HTTP/1.1 302 Found\r\nLocation: valid\r\nContent-Length: 0\r\n\r\n'
        if path == '/cross-host':
            return b'HTTP/1.1 302 Found\r\nLocation: https://127.0.0.1:' + str(self.port).encode() + b'/valid\r\n\r\n'
        if path == '/downgrade':
            return b'HTTP/1.1 302 Found\r\nLocation: http://localhost:1/\r\n\r\n'
        if path == '/loop':
            return b'HTTP/1.1 302 Found\r\nLocation: /loop\r\n\r\n'
        if path == '/truncated':
            return b'HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort'
        if path == '/truncated-chunk':
            return b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n20\r\nshort'
        if path == '/ambiguous':
            return b'HTTP/1.1 200 OK\r\nContent-Length: 1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n'
        if path == '/duplicate':
            return b'HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nx'
        if path == '/overflow':
            return b'HTTP/1.1 200 OK\r\nContent-Length: 18446744073709551616\r\n\r\n'
        if path == '/bad-chunk':
            return b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nnope\r\n'
        if path == '/oversized-header':
            return b'HTTP/1.1 200 OK\r\nX-Test: ' + b'a' * 9000 + b'\r\n\r\n'
        if path == '/folded':
            return b'HTTP/1.1 200 OK\r\nContent-Length: 1\r\n X-Fold: x\r\n\r\nx'
        if path == '/encoded':
            return b'HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 1\r\n\r\nx'
        return b'HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n'

    def close(self):
        self.stopped = True
        self.socket.close()
        self.thread.join(timeout=6)


def check(server, path, succeeds, trust=True):
    destination = fixtures / 'download.bin'
    destination.write_bytes(b'unchanged')
    env = os.environ.copy()
    env.pop('WPM_TLS_CA_FILE', None)
    if trust:
        env['WPM_TLS_CA_FILE'] = str(fixtures / 'ca.pem')
    result = subprocess.run([args.probe, f'https://localhost:{server.port}{path}', str(destination)],
                            env=env, capture_output=True, text=True, timeout=30)
    assert (result.returncode == 0) == succeeds, (path, result.returncode, result.stdout, result.stderr)
    assert destination.read_bytes() == (payload if succeeds else b'unchanged'), path
    assert not list(fixtures.glob(destination.name + '*.download')), path
    print('PASS', path, 'trusted' if trust else 'untrusted')


servers = [Server(kind) for kind in ('valid', 'wrong-host', 'expired')]
try:
    for path in ('/valid', '/chunked', '/relative', '/interim', '/close'):
        check(servers[0], path, True)
    for path in ('/abrupt', '/cross-host', '/downgrade', '/loop', '/truncated', '/truncated-chunk',
                 '/ambiguous', '/duplicate', '/overflow', '/bad-chunk', '/oversized-header',
                 '/folded', '/encoded', '/missing'):
        check(servers[0], path, False)
    check(servers[0], '/valid', False, trust=False)
    check(servers[1], '/valid', False)
    check(servers[2], '/valid', False)
finally:
    for server in servers:
        server.close()
