# IMPORTANT: Set the config to use the right socket path (in this case: tmp/waschfreiheit_pts)

socat -d PTY,link=/tmp/waschfreiheit_pts,echo=0 "EXEC:python3 test.py normal ...,pty,raw",echo=0
#socat -d PTY,link=/tmp/waschfreiheit_pts,echo=0 "EXEC:python3 test.py one_dead ...,pty,raw",echo=0
#socat -d PTY,link=/tmp/waschfreiheit_pts,echo=0 "EXEC:python3 test.py irresponsive ...,pty,raw",echo=0
