#! /bin/sh
### BEGIN INIT INFO
# Provides:          log2mail
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
### END INIT INFO
#
# $Id: init.d,v 1.3 2001/02/09 15:13:54 pape Exp $

PATH=/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/sbin/log2mail
NAME=log2mail
DESC=log2mail

# daemontools' service dir
SERVICE=/service

test -f $DAEMON || exit 0

# running under daemontools?
if [ -L ${SERVICE}/log2mail ]; then
cat << EOT >&2

log2mail is controlled by supervise. Use svc(8) /service/log2mail.

EOT
	exit 0
fi

set -e

case "$1" in
  start)
	echo -n "Starting $DESC: "
	start-stop-daemon --start --quiet --pidfile /var/run/$NAME.pid \
		--chuid log2mail:adm \
		--exec $DAEMON -- -f /etc/log2mail/config
	echo "$NAME."
	;;
  stop)
	echo -n "Stopping $DESC: "
	start-stop-daemon --stop --quiet --oknodo \
		--exec $DAEMON
	echo "$NAME."
	;;
  #reload)
	#
	#	If the daemon can reload its config files on the fly
	#	for example by sending it SIGHUP, do it here.
	#
	#	If the daemon responds to changes in its config file
	#	directly anyway, make this a do-nothing entry.
	#
	# echo "Reloading $DESC configuration files."
	# start-stop-daemon --stop --signal 1 --quiet --pidfile \
	#	/var/run/$NAME.pid --exec $DAEMON
  #;;
  restart|force-reload)
	#
	#	If the "reload" option is implemented, move the "force-reload"
	#	option to the "reload" entry above. If not, "force-reload" is
	#	just the same as "restart".
	#
	echo -n "Restarting $DESC: "
	$0 stop
	$0 start
	echo "done."
	;;
  *)
	N=/etc/init.d/$NAME
	# echo "Usage: $N {start|stop|restart|reload|force-reload}" >&2
	echo "Usage: $N {start|stop|restart|force-reload}" >&2
	exit 1
	;;
esac

exit 0
