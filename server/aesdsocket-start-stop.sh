#!/bin/sh
DAEMON=/usr/bin/aesdsocket
case "$1" in
    start)
        # arranca el daemon; los args después de -- van a la app
        start-stop-daemon --start --exec "$DAEMON" -- -d
        ;;
    stop)
        # SIGTERM -> tu handler hace el cleanup (borra /var/tmp/aesdsocketdata)
        start-stop-daemon --stop --signal TERM --exec "$DAEMON"
        ;;
    restart)
        start-stop-daemon --stop --signal TERM --exec "$DAEMON"
        sleep 1
        start-stop-daemon --start --exec "$DAEMON" -- -d
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac
exit 0
