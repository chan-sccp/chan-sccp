#
# Regular cron jobs for the chan-sccp-b package
#
0 4	* * *	root	[ -x /usr/bin/chan-sccp-b_maintenance ] && /usr/bin/chan-sccp-b_maintenance
