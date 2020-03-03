/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

char *tzutc = "UTC";
char *tzro = "Europe/Bucharest";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base)
{
	char *co, status;
	int descap, remcap;

	descap = -1;
	remcap = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "charge_full_design");
	if (co == NULL) {
		co = readfile(base, "energy_full_design");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &descap);
	free(co);

	co = readfile(base, "charge_now");
	if (co == NULL) {
		co = readfile(base, "energy_now");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &remcap);
	free(co);

	co = readfile(base, "status");
	if (!strncmp(co, "Discharging", 11)) {
		status = '-';
	} else if(!strncmp(co, "Charging", 8)) {
		status = '+';
	} else {
		status = '?';
	}

	if (remcap < 0 || descap < 0)
		return smprintf("invalid");

	return smprintf(" %.0f%%%c", ((float)remcap / (float)descap) * 100, status);
}

char *
gettemperature(char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf(" %02.0f°C", atof(co) / 1000);
}

char *
batteries()
{
	char *b0 = getbattery("/sys/class/power_supply/BAT0");
	char *b1 = getbattery("/sys/class/power_supply/BAT1");
	char *status = smprintf("B:%s%s", b0, b1);
	free(b0);
	free(b1);

	return status;
}

char *
temperatures()
{
	char *t0 = gettemperature("/sys/class/hwmon/hwmon0", "temp1_input");
	char *t1 = gettemperature("/sys/class/hwmon/hwmon1", "temp1_input");
	char *t2 = gettemperature("/sys/class/hwmon/hwmon2", "temp1_input");
	char *t3 = gettemperature("/sys/class/hwmon/hwmon3", "temp1_input");
	char *t4 = gettemperature("/sys/class/hwmon/hwmon4", "temp1_input");
	char *status = smprintf("T:%s%s%s%s%s", t0, t1, t2, t3, t4);
	free(t0);
	free(t1);
	free(t2);
	free(t3);
	free(t4);

	return status;
}

int
main(void)
{
	char *status;
	char *datetime;
	char *loads;
	char *mems;
	char *swap;
	char *bats;
	char *temps;
	struct sysinfo s;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	while (1) {
		sysinfo(&s);
		datetime = mktimes("%Y-%m-%d %H:%M %a", tzro);
		loads = smprintf("L: %.2f %.2f %.2f", s.loads[0] / 65536.0, s.loads[1] / 65536.0, s.loads[2] / 65536.0);
		mems = smprintf("M: %d%%", (int) ((1 - (s.freeram + s.bufferram) / (float) s.totalram) * 100));
		swap = smprintf("S: %d%%", (int) ((1 - ((float)s.freeswap) / s.totalswap) * 100));
		bats = batteries();
		temps = temperatures();
		status = smprintf("%s | %s | %s | %s | %s | %s", bats, temps, swap, mems, loads, datetime);
		free(temps);
		free(bats);
		free(swap);
		free(mems);
		free(loads);
		free(datetime);
		setstatus(status);
		free(status);
		sleep(1);
	}

	XCloseDisplay(dpy);

	return 0;
}

