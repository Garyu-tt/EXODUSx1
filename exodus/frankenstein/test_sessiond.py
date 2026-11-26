import os, pathlib, select, signal, subprocess, sys, tempfile

with tempfile.TemporaryDirectory() as tmp:
    path = pathlib.Path(tmp) / 'config'
    def write(text):
        stage = path.with_suffix('.new')
        stage.write_text(text)
        stage.chmod(0o600)
        os.replace(stage, path)
    write('generation=1\nmode=game\n')
    with subprocess.Popen([sys.argv[1], str(path)], stdout=subprocess.PIPE, text=True) as proc:
        def expect(event, gen, mode):
            assert select.select([proc.stdout], [], [], 5)[0], 'daemon timeout'
            line = proc.stdout.readline().strip()
            assert line == f'{event} generation={gen} desired={mode} active=unknown pid={proc.pid}', line
        def reload(text, event, gen, mode):
            write(text)
            proc.send_signal(signal.SIGHUP)
            expect(event, gen, mode)
        try:
            expect('ready', 1, 'game')
            reload('generation=2\nmode=desktop\n', 'reloaded', 2, 'desktop')
            reload('generation=3\nmode=broken\n', 'rejected', 2, 'desktop')
            reload('generation=1\nmode=game\n', 'rejected', 2, 'desktop')
            reload('generation=18446744073709551616\nmode=game\n', 'rejected', 2, 'desktop')
            reload('generation=4\nmode=game\nextra=x\n', 'rejected', 2, 'desktop')
            write('generation=5\nmode=game\n'); path.chmod(0o666)
            proc.send_signal(signal.SIGHUP); expect('rejected', 2, 'desktop')
            path.unlink(); path.symlink_to('/dev/null')
            proc.send_signal(signal.SIGHUP); expect('rejected', 2, 'desktop')
            reload('generation=6\nmode=game\n', 'reloaded', 6, 'game')
            proc.send_signal(signal.SIGUSR1); expect('status', 6, 'game')
            proc.send_signal(signal.SIGTERM); expect('stopped', 6, 'game')
            assert proc.wait(timeout=5) == 0
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
print('PASS: live reload, rejected malformed/stale/unsafe configurations, same PID, clean shutdown')
