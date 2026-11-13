#!/usr/bin/env python3
"""Run DJGPP commands headless in DOSBox.

    python tools/dos.py WORKDIR "CMD" ["CMD" ...]

Drives: C: = WORKDIR, D: = build/tc/DJGPP (gcc 2.7.2.1, binutils 2.7, libc, CWSDPMI),
E: = build/tc/ALLEGRO. DJGPP=D:\\DJGPP.ENV, PATH=D:\\BIN. Build the tree with tools/setup.py.
Pass source names in lower case (gcc picks the language from the name as given: `foo.cc` is
C++, `FOO.CC` is taken for a linker input).

As a module: run(workdir, cmds, log=True) -> str. Each command runs through DJGPP's `redir`, so
its stdout and stderr go to C:\\_LOG.TXT. The text is returned and the file removed. Each run
writes its own config and batch file, so several runs can go at once in different work
directories. Never kill DOSBox processes wholesale: other sessions run their own.
"""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TC = os.path.join(ROOT, 'build', 'tc')
# DOSBOX overrides; otherwise dosbox on the PATH, else the GOG copy in gamedata/ (Windows)
DOSBOX = (os.environ.get('DOSBOX') or shutil.which('dosbox') or
          next((p for p in (os.path.join(ROOT, 'gamedata', 'DOSBOX', 'DOSBox.exe'),)
                if os.path.exists(p)), 'dosbox'))

CONF = """[sdl]
fullscreen=false
[dosbox]
memsize=64
machine=svga_s3
[cpu]
core=dynamic
cycles=max
[speaker]
pcspeaker=false
tandy=off
[mixer]
nosound=true
[midi]
mpu401=none
[dos]
xms=true
ems=false
umb=true
[autoexec]
mount c "{c}"
mount d "{d}"
mount e "{e}"
set DJGPP=D:\\DJGPP.ENV
set PATH=D:\\BIN;Z:\\
c:
"""

LOG = '_LOG.TXT'
BUILTINS = {'SET', 'CD', 'MD', 'MKDIR', 'DEL', 'COPY', 'C:', 'D:', 'E:'}


def run(workdir, cmds, log=True, timeout=600):
    """Run cmds in DOSBox with C: = workdir; returns the combined output when log is set."""
    workdir = os.path.abspath(workdir)
    os.makedirs(workdir, exist_ok=True)
    logpath = os.path.join(workdir, LOG)
    if os.path.exists(logpath):
        os.unlink(logpath)
    lines = []
    rsp = []
    for k, c in enumerate(cmds):
        # DOS cuts command lines at 127 characters; DJGPP programs read @response files, so the
        # arguments of a long command go into one (C:\_RSPn.TXT, removed afterwards)
        word = c.split(' ', 1)[0].upper()
        if word in BUILTINS:
            # shell built-ins (SET, CD, ...) must run in the session itself, not under redir
            lines.append(c)
            continue
        if len(c) > 100 and ' ' in c:
            prog, args = c.split(' ', 1)
            name = '_RSP%d.TXT' % k
            with open(os.path.join(workdir, name), 'w', newline='') as f:
                f.write(args.replace(' ', '\r\n') + '\r\n')
            rsp.append(name)
            shown = c
            c = '%s @%s' % (prog, name)
        else:
            shown = c
        if log:
            # DOSBox's shell has no ^ escape: keep redirection characters out of the echo
            shown = shown.replace('>', '}').replace('<', '{').replace('|', '!')[:110]
            lines.append('ECHO ## %s >> C:\\%s' % (shown, LOG))
            lines.append('REDIR -oa C:\\%s -eo %s' % (LOG, c))
        else:
            lines.append(c)
    # the commands go in a batch file: DOSBox 0.74's [autoexec] overflows at about 4 KB
    bat = os.path.join(workdir, '_RUN.BAT')
    with open(bat, 'w', newline='') as f:
        f.write('@ECHO OFF\r\n' + ''.join(l + '\r\n' for l in lines))
    fd, conf = tempfile.mkstemp(suffix='.conf')
    with os.fdopen(fd, 'w') as f:
        f.write(CONF.format(c=workdir, d=os.path.join(TC, 'DJGPP'), e=os.path.join(TC, 'ALLEGRO')))
        f.write('CALL _RUN.BAT\nexit\n')
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    try:
        extra = ['-noconsole'] if os.name == 'nt' else []
        subprocess.run([DOSBOX, '-conf', conf, '-exit'] + extra, env=env, timeout=timeout,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       cwd=os.path.dirname(conf))   # -noconsole writes stdout.txt/stderr.txt there
    finally:
        os.unlink(conf)
        for name in ['_RUN.BAT'] + rsp:
            if os.path.exists(os.path.join(workdir, name)):
                os.unlink(os.path.join(workdir, name))
    out = ''
    if log and os.path.exists(logpath):
        out = open(logpath, 'rb').read().decode('latin-1').replace('\r\n', '\n')
        os.unlink(logpath)
    return out


if __name__ == '__main__':
    print(run(sys.argv[1], sys.argv[2:]), end='')
