import pytest
import subprocess
import threading
from http.server import SimpleHTTPRequestHandler
import socketserver
import time
import os
import sys

# Server setup for testing extraction
@pytest.fixture(scope="module")
def http_server():
    class Handler(SimpleHTTPRequestHandler):
        def log_message(self, format, *args):
            pass # quiet
    
    httpd = socketserver.TCPServer(("127.0.0.1", 0), Handler)
    port = httpd.server_address[1]
    
    t = threading.Thread(target=httpd.serve_forever, daemon=True)
    t.start()
    
    yield port
    
    httpd.shutdown()
    httpd.server_close()

@pytest.mark.skipif(sys.platform != "win32", reason="Requires Windows to run .exe")
def test_extract_cyrillic(http_server, tmp_path):
    port = http_server
    
    # Write test file
    html_content = '<div data-qa="резюме">Опыт работы: 5 лет. Навыки: Ассемблер.</div>'.encode('utf-8')
    with open("test_cyr.html", "wb") as f:
        f.write(html_content)
        
    out_file = str(tmp_path / "out.txt")
    
    exe_path = os.path.abspath("build/nanoweb-con.exe")
    
    res = subprocess.run([
        exe_path,
        "--extract",
        f"http://127.0.0.1:{port}/test_cyr.html",
        "data-qa",
        "резюме",
        out_file
    ], capture_output=True, timeout=10)
    
    assert res.returncode == 0, f"Exit code {res.returncode}, err: {res.stderr}"
    
    assert os.path.exists(out_file), "Output file was not created"
    with open(out_file, "rb") as f:
        content = f.read().decode('utf-8')
        
    assert "Опыт работы: 5 лет." in content
    assert "Навыки: Ассемблер." in content

