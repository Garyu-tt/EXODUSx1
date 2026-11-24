import pathlib, socket, ssl, struct, subprocess, tempfile, time, sys, os

SOURCE = pathlib.Path(__file__).resolve().parent

def run(*args):
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def certs():
    run('openssl','req','-x509','-newkey','ec','-pkeyopt','ec_paramgen_curve:P-256','-nodes','-days','1','-subj','/CN=EXR-test-CA','-keyout','ca.key','-out','ca.pem','-addext','basicConstraints=critical,CA:TRUE','-addext','keyUsage=critical,keyCertSign,cRLSign')
    for name in ('server','client','unpaired'):
        run('openssl','req','-new','-newkey','ec','-pkeyopt','ec_paramgen_curve:P-256','-nodes','-subj','/CN=localhost','-keyout',name+'.key','-out',name+'.csr')
        pathlib.Path('ext').write_text('subjectAltName=DNS:localhost\nextendedKeyUsage=serverAuth\nkeyUsage=critical,digitalSignature\nbasicConstraints=critical,CA:FALSE\n' if name=='server' else 'extendedKeyUsage=clientAuth\nkeyUsage=critical,digitalSignature\nbasicConstraints=critical,CA:FALSE\n')
        run('openssl','x509','-req','-in',name+'.csr','-CA','ca.pem','-CAkey','ca.key','-CAcreateserial','-days','1','-extfile','ext','-out',name+'.pem')

def connect(port, name='client'):
    ctx=ssl.create_default_context(cafile='ca.pem'); ctx.minimum_version=ssl.TLSVersion.TLSv1_3
    if name: ctx.load_cert_chain(name+'.pem',name+'.key')
    return ctx.wrap_socket(socket.create_connection(('127.0.0.1',port),timeout=8),server_hostname='localhost')

def frame(op): return struct.pack('!2sBBHH',b'EX',1,0,len(op),0)+op

def exact(s,n):
    b=b''
    while len(b)<n:
        v=s.recv(n-len(b))
        if not v: raise EOFError('short reply')
        b+=v
    return b

def reply(s):
    magic,ver,status,n,res=struct.unpack('!2sBBHH',exact(s,8))
    assert (magic,ver,res)==(b'EX',1,0) and n<=511
    return status,exact(s,n)

def closed(s):
    try: assert s.recv(1)==b''
    except (ssl.SSLError,ConnectionResetError): pass

def main():
    binary=str(pathlib.Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory() as d:
        cwd=os.getcwd(); os.chdir(d)
        try:
            certs()
            p=subprocess.Popen([binary,'ca.pem','server.pem','server.key','client.pem'],stdout=subprocess.PIPE,text=True)
            try:
                line=p.stdout.readline(); assert line.startswith('READY '),line; port=int(line.split()[1])
                with connect(port) as s:
                    for b in frame(b'system.status'): s.sendall(bytes([b]))
                    assert reply(s)==(0,b'scope=host_reference\nhardware_verified=false\n')
                    s.sendall(frame(b'battery.read')+frame(b'capabilities.list'))
                    assert reply(s)==(1,b'')
                    assert reply(s)==(0,b'capabilities.list\nsystem.status\n')
                for name in (None,'unpaired'):
                    try:
                        with connect(port,name) as s:
                            s.sendall(frame(b'system.status')); closed(s)
                    except (ssl.SSLError,ConnectionResetError,BrokenPipeError): pass
                with connect(port) as s:
                    s.sendall(b'EX\x02\x00\x00\x01\x00\x00'); closed(s)
                with connect(port) as s:
                    s.sendall(b'E'); time.sleep(3.3); closed(s)
                with socket.create_connection(('127.0.0.1',port),timeout=8) as raw:
                    t=time.monotonic(); closed(raw); assert 4.5<=time.monotonic()-t<8
                with connect(port) as s:
                    s.sendall(frame(b'system.status')); assert reply(s)[0]==0
                    p.terminate(); assert p.wait(timeout=3)==0; closed(s)
                print('PASS: TCP/mTLS, split and coalesced frames, ACL, unpaired/no cert, invalid frame, partial and handshake deadlines, reconnect, SIGTERM')
            finally:
                if p.poll() is None: p.kill(); p.wait()
        finally: os.chdir(cwd)

if __name__=='__main__': main()
