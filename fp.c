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
struct termios state;

int getch()
{
	int buf;
	struct termios old;

	fflush(stdout);

	int res = tcgetattr(0, &old);
	old.c_lflag &= ~ICANON & ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	res = tcsetattr(0, TCSANOW, &old);
	buf = getchar();
	old.c_lflag |= ICANON | ECHO;
	res = tcsetattr(0, TCSANOW, &old);
	return buf;
}

void restore(void)
{
	tcsetattr(0, TCSANOW, &state);
}

void getchhnd(int sig)
{
	exit(1);
}

struct collision strsub(const char *outside, const char *inside)
{
	struct collision res = {0, 0};

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
	if (left->c.fullness != right->c.fullness)
		return left->c.fullness < right->c.fullness ? -1 : 1;
	if (left->c.offset != right->c.offset)
		return left->c.offset < right->c.offset ? 1 : -1;
	return -strcmp(left->name, right->name);
}

void loadpath(const char *path)
{
	errno = 0;
	DIR *dir = opendir(path);
	for (struct dirent *d; (d = readdir(dir));)
	{
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
	for (size_t i = 0; i < 5; ++i)
		fprintf(stderr, "`%s'\n", results[i].name);
	fprintf(stderr, "\033[6A\033[%luC", 2 + bufi);
	fflush(stderr);
}

int main(int argc, char **argv)
{
	if (argc != 2 || !*argv[1])
	{
		fprintf(stderr, "usage: %s path\n", *argv);
		return 1;
	}

	tcgetattr(0, &state);
	for (int sig = 1; sig <= SIGRTMAX; ++sig)
		signal(sig, getchhnd);
	atexit(restore);

	loadpath(argv[1]);

	char buf[NAME_MAX+1] = {0};
	size_t bufi = 0;

	render("", 0);
	for (int ch; (ch = getch());)
	{
		switch (ch)
		{
		case -1: case 4: case '\n': case '\r':
			goto end;
		case 127:
		{
			if (!bufi)
				goto cont;
			buf[--bufi] = 0;
		} break;
		default:
		{
			if (bufi == NAME_MAX)
				goto cont;
			buf[bufi++] = ch;
		} break;
		}
		render(buf, bufi);
cont:;
	}
end:
	fprintf(stderr, "\033[6B\rYou've chosen `");
	fflush(stderr);
	fprintf(stdout, "%s", results[0].name);
	fflush(stdout);
	fprintf(stderr, "'!\n");
}
