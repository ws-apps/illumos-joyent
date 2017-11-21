/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2017 Joyent, Inc.
 */

#include <sys/time.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <port.h>
#include <poll.h>

#include <sys/event.h>

static int
setup_rw_port(int port, const struct kevent *kev)
{
	int err = 0;
	int source, events;
	uintptr_t object;
	void *user;

	source = PORT_SOURCE_FD;
	object = kev->ident;
	events = (kev->filter == EVFILT_READ) ? POLLIN : POLLOUT;

	if ((kev->flags & (EV_DISABLE|EV_DELETE)) != 0) {
		fprintf(stderr, "port dissociate: %d, %d, %"PRId64"\n",
		    port, source, object);
		(void) port_dissociate(port, source, object);
		fprintf(stderr, "port dissociated\n");
		return (0);
	}

	if ((kev->flags & (EV_ADD|EV_ENABLE)) == 0) {
		errno = EINVAL;
		return (-1);
	}

	user = malloc(sizeof (struct kevent));
	if (user == NULL)
			return (-1);
	bcopy(kev, user, sizeof (struct kevent));

	fprintf(stderr, "port associate: %d, %d, %"PRId64", %x, %p\n",
	    port, source, object, events, user);
	err = port_associate(port, source, object, events, user);
	fprintf(stderr, "port associated: %d\n", err);
	if (err != 0)
		free(user);

	return (err);
}

static int
setup_timer_port(int port, const struct kevent *kev)
{
	int err;
	port_notify_t pn;
	struct sigevent evp;
	struct itimerspec its;
	timer_t timer;

	its.it_value.tv_sec = kev->data / 1000;
	its.it_value.tv_nsec = (kev->data % 1000) * 1000000;
	if ((kev->flags & EV_ONESHOT) == 0)
		its.it_interval = its.it_value;

	pn.portnfy_port = port;
	pn.portnfy_user = malloc(sizeof (struct kevent));
	if (pn.portnfy_user == NULL)
		return (-1);
	bcopy(kev, pn.portnfy_user, sizeof (struct kevent));

	evp.sigev_notify = SIGEV_PORT;
	evp.sigev_value.sival_ptr = &pn;

	err = timer_create(CLOCK_REALTIME, &evp, &timer);
	if (err != 0)
		goto fail_create;

	err = timer_settime(timer, 0, &its, NULL);
	if (err != 0)
		goto fail_settime;

	fprintf(stderr, "timer created: %d %p\n", kev->data,
	    pn.portnfy_user);
	return (0);

fail_settime:
	(void) timer_delete(timer);

fail_create:
	free(pn.portnfy_user);

	fprintf(stderr, "timer creation failed: %d %p\n", kev->data,
	    pn.portnfy_user);
	return (err);
}

static void
port_to_kevent(port_event_t *pe, struct kevent *kev)
{
	bcopy(pe->portev_user, kev, sizeof (struct kevent));
	free(pe->portev_user);
	pe->portev_user = NULL;
}

int kqueue(void)
{
	return (port_create());
}

int kevent(int kq, const struct kevent *changelist, int nchanges,
    struct kevent *eventlist, int nevents, struct timespec *timeout)
{
	uint_t nget = 1;
	int i, err;

	fprintf(stderr, "kevent: %d changes, %d events\n", nchanges, nevents);
	if (changelist == NULL)
		goto event;

	for (i = 0; i != nchanges; i++) {
		const struct kevent *kev = &changelist[i];

		fprintf(stderr, "kevent change: %"PRId64", %d, %d, %d, %"PRIx64", %p\n",
		    kev->ident, kev->filter, kev->flags, kev->fflags, kev->data, kev->udata);

		switch (kev->filter) {
		case EVFILT_READ:
		case EVFILT_WRITE:
			err = setup_rw_port(kq, kev);
			if (err != 0)
				return (-1);
			break;

		case EVFILT_TIMER:
			err = setup_timer_port(kq, kev);
			break;

		case EVFILT_SIGNAL:
		default:
			errno = EINVAL;
			err = -1;
		}
		if (err != 0)
			return (-1);
	}

event:
	if (nevents == 0)
		return (0);

	if (eventlist != NULL) {
		port_event_t *list = calloc(nevents, sizeof (port_event_t));

		if (list == NULL)
			return (-1);

		fprintf(stderr, "port_getn: getting %d events\n", nevents);
		err = port_getn(kq, list, nevents, &nget, timeout);
		if (err != 0 && err != ETIME)
			return (err);
		fprintf(stderr, "port_getn: got %d events (err = %d)\n",
		    nget, err);

		for (i = 0; i != nget; i++) {
			struct kevent *kev = &eventlist[i];
			port_event_t *pe = &list[i];

			port_to_kevent(pe, kev);
			fprintf(stderr, "port event: %d, %d, %"PRId64", %d, %p\n",
			    kq, pe->portev_source, pe->portev_object,
			    pe->portev_events, pe->portev_user);
			fprintf(stderr, "kevent event: %"PRId64", %d, %d, %d, %"PRIx64
			    ", %p\n", kev->ident, kev->filter, kev->flags, kev->fflags,
			    kev->data, kev->udata);

			if ((pe->portev_source == PORT_SOURCE_FD)) {
				if ((kev->flags & EV_ONESHOT) == 0) {
					fprintf(stderr, "port reassociate:\n");
					err = setup_rw_port(kq, kev);
					fprintf(stderr, "port reassociated: %d\n", err);
					if (err != 0)
						return (err);
				}
			}
		}
	}

	return (nget);
}
