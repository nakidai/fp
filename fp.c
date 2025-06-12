#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <termios.h>
#include <unistd.h>


struct result
{
	struct collision
	{
		size_t offset, length, fullness;
	} c;
	char name[NAME_MAX+1];
} *results = 0;
size_t amount = 0;
int ischosen = 1, lines;
struct termios state;

int getch()
{
	int buf;
	struct termios old;

	fflush(stdout);

	tcgetattr(0, &old);
	old.c_lflag &= ~ICANON & ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &old);
	buf = getchar();
	old.c_lflag |= ICANON | ECHO;
	tcsetattr(0, TCSANOW, &old);
	return buf;
}

void restore(void)
{
	fprintf(stderr, "\033[%dB\r", lines + 1);
	if (ischosen)
	{
		fprintf(stderr, "You've chosen `");
		fflush(stderr);
		fprintf(stdout, "%s", results[0].name);
		fflush(stdout);
		fprintf(stderr, "'!\n");
	}
	tcsetattr(0, TCSANOW, &state);
}

void getchhnd(int sig)
{
	ischosen = 0;
	exit(1);
}

struct collision strsub(const char *outside, const char *inside)
{
	struct collision res = {0, 0, 0};

	size_t outlen = strlen(outside);
	size_t inlen = strlen(inside);

	for (size_t i = 0; i < outlen; ++i)
	{
		size_t j;
		for (j = 0; j < inlen && j < outlen - i; ++j)
			if (outside[i + j] != inside[j])
				break;
		if (res.length < j)
			res.offset = i,
			res.length = j,
			res.fullness = outlen - j;
	}
	return res;
}

/* it is inverted for generating descending array */
int colcmp(const void *_left, const void *_right)
{
	const struct result *left = _left, *right = _right;
	if (left->c.length != right->c.length)
		return left->c.length < right->c.length ? 1 : -1;
	if (left->c.offset != right->c.offset)
		return left->c.offset < right->c.offset ? -1 : 1;
	if (left->c.fullness != right->c.fullness)
		return left->c.fullness < right->c.fullness ? -1 : 1;
	return -strcmp(left->name, right->name);
}

int find(const char *name)
{
	for (size_t i = 0; i < amount; ++i)
		if (!strcmp(results[i].name, name))
			return 1;
	return 0;
}

void loadpath(const char *path)
{
	errno = 0;
	DIR *dir = opendir(path);
	if (!dir)
		err(1, "opendir(`%s')", path);
	for (struct dirent *d; (d = readdir(dir));)
	{
		if (find(d->d_name))
			continue;

		results = realloc(results, sizeof(*results) * ++amount);
		if (!results)
			err(1, "realloc()");
		strlcpy(results[amount - 1].name, d->d_name, sizeof(results->name));
	}
	if (errno)
		err(1, "readdir()");
}

void sortresults(const char *search)
{
	for (size_t i = 0; i < amount; ++i)
		results[i].c = strsub(results[i].name, search);
	qsort(results, amount, sizeof(struct result), colcmp);
}

void render(const char *buf, size_t bufi)
{
	sortresults(buf);
	fprintf(stderr, "\r: %s\n", buf);
	for (size_t i = 0; i < lines; ++i)
		fprintf(stderr, "`%s'\n", results[i].name);
	fprintf(stderr, "\033[%dA\033[%luC", lines + 1, 2 + bufi);
	fflush(stderr);
}

int main(int argc, char **argv)
{
	if (argc < 3 || !*argv[1])
	{
		fprintf(stderr, "usage: %s lines path [path [...]]\n", *argv);
		return 1;
	}

	for (int i = 2; i < argc; ++i)
		loadpath(argv[i]);
	lines = atoi(argv[1]);
	lines = lines < 0 ? 0 : lines;
	lines = lines > amount - 1 ? amount - 1 : lines;
	render("", 0);

	tcgetattr(0, &state);
	for (int sig = 1; sig <= SIGRTMAX; ++sig)
		signal(sig, getchhnd);
	atexit(restore);

	char buf[NAME_MAX+1] = {0};
	size_t bufi = 0;

	/* TODO: maybe add support for multibyte characters */
	for (int ch; (ch = getch()); render(buf, bufi))
		switch (ch)
		{
		case -1: case 4: case '\n': case '\r':
			return 0;
		case 27:
			ischosen = 0;
			return 0;
		case 127:
		{
			if (!bufi)
				continue;
			buf[--bufi] = 0;
		} break;
		default:
		{
			if (bufi == NAME_MAX)
				continue;
			buf[bufi++] = ch;
		} break;
		}
}
