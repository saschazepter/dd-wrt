/* vi: set sw=4 ts=4: */
/*
 * modinfo - retrieve module info
 * Copyright (c) 2008 Pascal Bellard
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//applet:IF_MODINFO(APPLET(modinfo, BB_DIR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_MODINFO) += modinfo.o modutils.o

//config:config MODINFO
//config:	bool "modinfo"
//config:	default y
//config:	help
//config:	  Show information about a Linux Kernel module

#include <fnmatch.h>
#include <sys/utsname.h> /* uname() */
#include "libbb.h"
#include "modutils.h"


enum {
	OPT_TAGS = (1 << 12) - 1, /* shortcut count */
	OPT_F = (1 << 12), /* field name */
	OPT_0 = (1 << 13), /* \0 as separator */
};

struct modinfo_env {
	char *field;
	int tags;
};

static void display(const char *data, const char *pattern, int flag)
{
	if (flag) {
		int n = printf("%s:", pattern);
		while (n++ < 16)
			bb_putchar(' ');
	}
	printf("%s%c", data, (option_mask32 & OPT_0) ? '\0' : '\n');
}

static void modinfo(const char *path, const char *version,
			const struct modinfo_env *env)
{
	static const char *const shortcuts[] = {
		"filename",
		"license",
		"author",
		"description",
		"version",
		"alias",
		"srcversion",
		"depends",
		"uts_release",
		"vermagic",
		"parm",
		"firmware",
	};
	size_t len;
	int j;
	char *ptr, *the_module;
	const char *field = env->field;
	int tags = env->tags;

	if (tags & 1) { /* filename */
		display(path, shortcuts[0], 1 != tags);
	}

	len = MAXINT(ssize_t);
	the_module = xmalloc_open_zipped_read_close(path, &len);
	if (!the_module) {
		if (path[0] == '/')
			return;
		/* Newer depmod puts relative paths in modules.dep */
		path = xasprintf("%s/%s/%s", CONFIG_DEFAULT_MODULES_DIR, version, path);
		the_module = xmalloc_open_zipped_read_close(path, &len);
		free((char*)path);
		if (!the_module)
			return;
	}

	if (field)
		tags |= OPT_F;
	for (j = 1; (1<<j) & (OPT_TAGS + OPT_F); j++) {
		const char *pattern;

		if (!((1<<j) & tags))
			continue;
		pattern = field;
		if ((1<<j) & OPT_TAGS)
			pattern = shortcuts[j];
		ptr = the_module;
		while (1) {
			char *after_pattern;

			ptr = memchr(ptr, *pattern, len - (ptr - (char*)the_module));
			if (ptr == NULL) /* no occurance left, done */
				break;
			after_pattern = is_prefixed_with(ptr, pattern);
			if (after_pattern && *after_pattern == '=') {
				/* field prefixes are 0x80 or 0x00 */
				if ((ptr[-1] & 0x7F) == 0x00) {
					ptr = after_pattern + 1;
					display(ptr, pattern, (1<<j) != tags);
					ptr += strlen(ptr);
				}
			}
			++ptr;
		}
	}
	free(the_module);
}

//usage:#define modinfo_trivial_usage
//usage:       "[-adlp0] [-F keyword] MODULE"
//usage:#define modinfo_full_usage "\n\n"
//usage:       "	-a		Shortcut for '-F author'"
//usage:     "\n	-d		Shortcut for '-F description'"
//usage:     "\n	-l		Shortcut for '-F license'"
//usage:     "\n	-p		Shortcut for '-F parm'"
//usage:     "\n	-F keyword	Keyword to look for"
//usage:     "\n	-0		NUL terminated output"
//usage:#define modinfo_example_usage
//usage:       "$ modinfo -F vermagic loop\n"

int modinfo_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int modinfo_main(int argc UNUSED_PARAM, char **argv)
{
	struct modinfo_env env;
	char name[MODULE_NAME_LEN];
	struct utsname uts;
	parser_t *parser;
	char *colon, *tokens[2];
	unsigned opts;
	unsigned i;

	env.field = NULL;
	opts = getopt32(argv, "^" "nladvAsDumpF:0" "\0" "-1"/*minimum one arg*/, &field);


	env.tags = opts & OPT_TAGS ? opts & OPT_TAGS : OPT_TAGS;
	argv += optind;

	uname(&uts);
	parser = config_open2(
		xasprintf("%s/%s/%s", CONFIG_DEFAULT_MODULES_DIR, uts.release, CONFIG_DEFAULT_DEPMOD_FILE),
		xfopen_for_read
	);

	while (config_read(parser, tokens, 2, 1, "# \t", PARSE_NORMAL)) {
		colon = last_char_is(tokens[0], ':');
		if (colon == NULL)
			continue;
		*colon = '\0';
		filename2modname(bb_basename(tokens[0]), name);
		for (i = 0; argv[i]; i++) {
			if (fnmatch(argv[i], name, 0) == 0) {
				modinfo(tokens[0], uts.release, &env);
				argv[i] = (char *) "";
			}
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		config_close(parser);

	for (i = 0; argv[i]; i++) {
		if (argv[i][0]) {
			modinfo(argv[i], uts.release, &env);
		}
	}

	return 0;
}
