/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <libgeom.h>
#include <geom/stripe/g_stripe.h>

#include "core/geom.h"
#include "misc/subr.h"


uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_STRIPE_VERSION;

static intmax_t stripesize = 4096;

static void stripe_main(struct gctl_req *req, unsigned flags);
static void stripe_clear(struct gctl_req *req);
static void stripe_dump(struct gctl_req *req);
static void stripe_label(struct gctl_req *req);

struct g_command class_commands[] = {
	{ "clear", G_FLAG_VERBOSE, stripe_main, G_NULL_OPTS },
	{ "create", G_FLAG_VERBOSE | G_FLAG_LOADKLD, NULL,
	    {
		{ 's', "stripesize", &stripesize, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    }
	},
	{ "destroy", G_FLAG_VERBOSE, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    }
	},
	{ "dump", 0, stripe_main, G_NULL_OPTS },
	{ "label", G_FLAG_VERBOSE | G_FLAG_LOADKLD, stripe_main,
	    {
		{ 'h', "hardcode", NULL, G_TYPE_NONE },
		{ 's', "stripesize", &stripesize, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    }
	},
	{ "stop", G_FLAG_VERBOSE, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    }
	},
	G_CMD_SENTINEL
};

static int verbose = 0;

void usage(const char *name);
void
usage(const char *name)
{

	fprintf(stderr, "usage: %s create [-hv] [-s stripesize] <name> <prov> <prov> [prov [...]]\n", name);
	fprintf(stderr, "       %s destroy [-fv] <name> [name [...]]\n", name);
	fprintf(stderr, "       %s label [-hv] [-s stripesize] <name> <prov> <prov> [prov [...]]\n", name);
	fprintf(stderr, "       %s stop [-fv] <name> [name [...]]\n", name);
	fprintf(stderr, "       %s clear [-v] <prov> [prov [...]]\n", name);
	fprintf(stderr, "       %s dump <prov> [prov [...]]\n", name);
}

static void
stripe_main(struct gctl_req *req, unsigned flags)
{
	const char *name;

	if ((flags & G_FLAG_VERBOSE) != 0)
		verbose = 1;

	name = gctl_get_asciiparam(req, "verb");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument.", "verb");
		return;
	}
	if (strcmp(name, "label") == 0)
		stripe_label(req);
	else if (strcmp(name, "clear") == 0)
		stripe_clear(req);
	else if (strcmp(name, "dump") == 0)
		stripe_dump(req);
	else
		gctl_error(req, "Unknown command: %s.", name);
}

static void
stripe_label(struct gctl_req *req)
{
	struct g_stripe_metadata md;
	intmax_t *stripesizep;
	off_t compsize, msize;
	u_char sector[512];
	unsigned i, ssize, secsize;
	const char *name;
	char param[16];
	int *hardcode, *nargs, error;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs <= 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}
	hardcode = gctl_get_paraml(req, "hardcode", sizeof(*hardcode));
	if (hardcode == NULL) {
		gctl_error(req, "No '%s' argument.", "hardcode");
		return;
	}

	/*
	 * Clear last sector first to spoil all components if device exists.
	 */
	compsize = 0;
	secsize = 0;
	for (i = 1; i < (unsigned)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);

		msize = g_get_mediasize(name);
		ssize = g_get_sectorsize(name);
		if (msize == 0 || ssize == 0) {
			gctl_error(req, "Can't get informations about %s: %s.",
			    name, strerror(errno));
			return;
		}
		msize -= ssize;
		if (compsize == 0 || (compsize > 0 && msize < compsize))
			compsize = msize;
		if (secsize == 0)
			secsize = ssize;
		else
			secsize = g_lcm(secsize, ssize);

		error = g_metadata_clear(name, NULL);
		if (error != 0) {
			gctl_error(req, "Can't store metadata on %s: %s.", name,
			    strerror(error));
			return;
		}
	}

	strlcpy(md.md_magic, G_STRIPE_MAGIC, sizeof(md.md_magic));
	md.md_version = G_STRIPE_VERSION;
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	strlcpy(md.md_name, name, sizeof(md.md_name));
	md.md_id = arc4random();
	md.md_all = *nargs - 1;
	stripesizep = gctl_get_paraml(req, "stripesize", sizeof(*stripesizep));
	if (stripesizep == NULL) {
		gctl_error(req, "No '%s' argument.", "stripesize");
		return;
	}
	if ((*stripesizep % secsize) != 0) {
		gctl_error(req, "Stripesize should be multiple of %u.",
		    secsize);
		return;
	}
	md.md_stripesize = *stripesizep;

	/*
	 * Ok, store metadata.
	 */
	for (i = 1; i < (unsigned)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);

		msize = g_get_mediasize(name) - g_get_sectorsize(name);
		if (compsize < msize) {
			fprintf(stderr,
			    "warning: %s: only %jd bytes from %jd bytes used.\n",
			    name, (intmax_t)compsize, (intmax_t)msize);
		}

		md.md_no = i - 1;
		if (!*hardcode)
			bzero(md.md_provider, sizeof(md.md_provider));
		else {
			if (strncmp(name, _PATH_DEV, strlen(_PATH_DEV)) == 0)
				name += strlen(_PATH_DEV);
			strlcpy(md.md_provider, name, sizeof(md.md_provider));
		}
		stripe_metadata_encode(&md, sector);
		error = g_metadata_store(name, sector, sizeof(sector));
		if (error != 0) {
			fprintf(stderr, "Can't store metadata on %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Metadata value stored on %s.\n", name);
	}
}

static void
stripe_clear(struct gctl_req *req)
{
	const char *name;
	char param[16];
	unsigned i;
	int *nargs, error;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 1) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	for (i = 0; i < (unsigned)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);

		error = g_metadata_clear(name, G_STRIPE_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Can't clear metadata on %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Metadata cleared on %s.\n", name); 
	}
}

static void 
stripe_metadata_dump(const struct g_stripe_metadata *md)
{

	printf("         Magic string: %s\n", md->md_magic); 
	printf("     Metadata version: %u\n", (u_int)md->md_version);
	printf("          Device name: %s\n", md->md_name);
	printf("            Device ID: %u\n", (u_int)md->md_id);
	printf("          Disk number: %u\n", (u_int)md->md_no);
	printf("Total number of disks: %u\n", (u_int)md->md_all);
	printf("          Stripe size: %u\n", (u_int)md->md_stripesize);
	printf("   Hardcoded provider: %s\n", md->md_provider);
}

static void
stripe_dump(struct gctl_req *req)
{
	struct g_stripe_metadata md, tmpmd;
	const char *name;
	char param[16];
	int *nargs, error, i;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 1) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);

		error = g_metadata_read(name, (u_char *)&tmpmd, sizeof(tmpmd),
		    G_STRIPE_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Can't read metadata from %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		stripe_metadata_decode((u_char *)&tmpmd, &md);
		printf("Metadata on %s:\n", name);
		stripe_metadata_dump(&md);
		printf("\n");
	}
}
