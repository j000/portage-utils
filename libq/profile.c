typedef void *(q_profile_callback_t)(void *, char *);

static void *
q_profile_walk_at(int dir_fd, const char *dir, const char *file,
                  q_profile_callback_t callback, void *data)
{
	FILE *fp;
	int subdir_fd, fd;
	size_t buflen, linelen;
	char *buf;

	/* Pop open this profile dir */
	subdir_fd = openat(dir_fd, dir, O_RDONLY|O_CLOEXEC|O_PATH);
	if (subdir_fd < 0)
		return data;

	/* Then open the file */
	fd = openat(subdir_fd, file, O_RDONLY|O_CLOEXEC);
	if (fd < 0)
		goto walk_parent;

	fp = fdopen(fd, "r");
	if (!fp) {
		close(fd);
		goto walk_parent;
	}

	/* hand feed the file to the callback */
	buf = NULL;
	while (getline(&buf, &buflen, fp) != -1)
		data = callback(data, buf);
	free(buf);

	/* does close(fd) for us */
	fclose(fp);

	/* Now walk the parents */
 walk_parent:
	fd = openat(subdir_fd, "parent", O_RDONLY|O_CLOEXEC);
	if (fd < 0)
		goto done;
	fp = fdopen(fd, "r");
	if (!fp) {
		close(fd);
		goto done;
	}

	buf = NULL;
	while ((linelen = getline(&buf, &buflen, fp)) != -1) {
		char *s;

		rmspace_len(buf, linelen);

		s = strchr(buf, '#');
		if (s)
			*s = '\0';

		data = q_profile_walk_at(subdir_fd, buf, file, callback, data);
	}
	free(buf);

	/* does close(fd) for us */
	fclose(fp);

 done:
	if (subdir_fd != AT_FDCWD)
		close(subdir_fd);

	return data;
}

static void *
q_profile_walk(const char *file, q_profile_callback_t callback, void *data)
{
	/* Walk the profiles and read the file in question */
	data = q_profile_walk_at(AT_FDCWD, CONFIG_EPREFIX "etc/make.profile", file, callback, data);
	return q_profile_walk_at(AT_FDCWD, CONFIG_EPREFIX "etc/portage/make.profile", file, callback, data);
}
