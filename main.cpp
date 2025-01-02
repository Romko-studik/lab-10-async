#include "options_parser.h"
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

char buf[4 * 1024 * 1024];
// Worst case scenario every char is non-printable and we need to escape it
char cvt_buf[4 * 4 * 1024 * 1024];

int readbuffer(int fd, char *buffer, ssize_t size, int *status) {
  ssize_t read_bytes = 0;

  while (read_bytes < size) {
    ssize_t read_now = read(fd, buffer + read_bytes, size - read_bytes);
    if (read_now == -1) {
      if (errno == EINTR)
        continue;
      else {
        *status = errno;
        return -1;
      }
    } else if (read_now == 0)
      break;
    else
      read_bytes += read_now;
  }
  return read_bytes;
}

int writebuffer(int fd, const char *buffer, ssize_t size, int *status) {
  ssize_t written_bytes = 0;

  while (written_bytes < size) {
    ssize_t written_now =
        write(fd, buffer + written_bytes, size - written_bytes);
    if (written_now == -1) {
      if (errno == EINTR)
        continue;
      else {
        *status = errno;
        return -1;
      }
    } else
      written_bytes += written_now;
  }
  return 0;
}

ssize_t cvt_copy_buffer(const char *source, char *target, ssize_t size) {
  ssize_t written = 0;

  for (ssize_t i = 0; i < size; i++) {
    if (isprint(source[i]) || isspace(source[i])) {
      target[written] = source[i];
      written++;
    } else {
      target[written] = '\\';
      target[written + 1] = 'x';
      target[written + 2] = "0123456789ABCDEF"[(source[i] >> 4) & 0xf];
      target[written + 3] = "0123456789ABCDEF"[source[i] & 0xf];
      written += 4;
    }
  }

  return written;
}

void cat(int fd, bool show_non_printable = false) {
  ssize_t n;
  int status;

  char *output_buf = show_non_printable ? cvt_buf : buf;

  while ((n = readbuffer(fd, buf, sizeof(buf), &status)) > 0) {
    if (show_non_printable) {
      n = cvt_copy_buffer(buf, cvt_buf, n);
    }
    if (writebuffer(1, output_buf, n, &status) != 0) {
      std::string s("cat: write error\n");
      write(2, s.c_str(), s.size());
      return;
    }
  }
  if (n < 0) {
    std::string s("cat: read error\n");
    write(2, s.c_str(), s.size());
    return;
  }
}

int main(int argc, char *argv[]) {
  command_line_options_t command_line_options{argc, argv};
  std::vector<int> fds;
  for (auto &filename : command_line_options.get_filenames()) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
      std::string s("cat: cannot open file\n");
      write(2, s.c_str(), s.size());
      return 1;
    }
    fds.push_back(fd);
  }
  for (auto &fd : fds) {
    cat(fd, command_line_options.get_A_flag());
    close(fd);
  }
  return 0;
}
