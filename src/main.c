#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libgen.h>
#include <stdbool.h>
#include <errno.h>

#define VALID_ARGS_COUNT       4
#define READ_BLOCK             4096
#define WRITE_BLOCK            4096
#define MIN_RUNNING_PROC_COUNT 1
#define TEMP_OUTPUT_FILE       "/tmp/output"

void print_error          (const char*, const char*, const char*);
int  print_file           (const char*                          );
bool is_dir               (const char*                          );
int  substr_in_file_count (const char*, const char*, int*       );
int  dir_find_substr      (const char*, const char*, const int  );
int  new_find_process     (const char*, const char*, const int  );

char *module_name;
int  *curr_processes_running;
int   output_file;

int main(int argc, char *argv[])
{
  module_name = basename(argv[0]);

  const char *source_dir     = argv[1];
  const char *found_sequence = argv[2];
  const int   max_procceses  = atoi(argv[3]);

  if (curr_processes_running == MAP_FAILED)
  {
    print_error(module_name, NULL, "args count error");
    return 1;
  }

  if (!is_dir(source_dir))
  {
    print_error(module_name, source_dir, "not a dir");
    return 1;
  }

  if (max_procceses < MIN_RUNNING_PROC_COUNT)
  {
    print_error(module_name, NULL, "invalid max running processes count value");
    return 1;
  }

  curr_processes_running = mmap(
    0,
    sizeof(int*),
    PROT_READ | PROT_WRITE,
    MAP_ANONYMOUS | MAP_SHARED,
    -1, 0
  );

  if (curr_processes_running == MAP_FAILED)
  {
    print_error(module_name, "memory mapping error", strerror(errno));
    return 1;
  }

  *curr_processes_running = 0;

  output_file = open(TEMP_OUTPUT_FILE, O_CREAT | O_WRONLY | O_EXCL, 0640);
  if (output_file == -1)
  {
    print_error(module_name, TEMP_OUTPUT_FILE, strerror(errno));
    return 1;
  }

  dir_find_substr(source_dir, found_sequence, max_procceses);

  if (close(output_file) == -1)
  {
    print_error(module_name, TEMP_OUTPUT_FILE, strerror(errno));
    return 1;
  }

  if (munmap(curr_processes_running, sizeof(int*)))
  {
    print_error(module_name, "memory unmapping error", strerror(errno));
    return 1;
  }

  print_file(TEMP_OUTPUT_FILE);

  if (remove(TEMP_OUTPUT_FILE) == -1)
  {
    print_error(module_name, TEMP_OUTPUT_FILE, strerror(errno));
    return 1;
  }

  return 0;
}

void print_error(
  const char *module_name,
  const char* additional_info,
  const char *error_msg)
{
  if (additional_info)
  {
    fprintf(stderr, "%s: %s: %s\n", module_name, additional_info, error_msg);
  }
  else
  {
    fprintf(stderr, "%s: %s\n", module_name, error_msg);
  }
}

int print_file(const char *file_path)
{
  int fd = open(file_path, O_RDONLY);

  if (fd == -1)
  {
    print_error(module_name, file_path, strerror(errno));
    return 1;
  }

  char buf[READ_BLOCK + 1];
  int bytes_read;

  while ((bytes_read = read(fd, buf, READ_BLOCK)) > 0)
  {
    buf[bytes_read] = '\0';

    printf("%s", buf);
  }

  if (close(fd) == -1)
  {
    print_error(module_name, file_path, strerror(errno));
    return 1;
  }

  return 0;
}

bool is_dir(const char *dir_path)
{
  struct stat temp_stat;

  if (lstat(dir_path, &temp_stat) == -1)
  {
    print_error(module_name, dir_path, strerror(errno));
    return false;
  }

  return S_ISDIR(temp_stat.st_mode);
}

int substr_in_file_count(
  const char *file_path,
  const char *string2find,
  int *total_bytes_read)
{
  int fd = open(file_path, O_RDONLY);

  if (fd == -1)
  {
    print_error(module_name, file_path, strerror(errno));
    return -1;
  }

  struct stat temp_stat;
  if (lstat(file_path, &temp_stat) == -1)
  {
    print_error(module_name, file_path, strerror(errno));
    return -1;
  }

  int mem_size = temp_stat.st_size;
  if (!mem_size)
  {
    return -1;
  }

  *total_bytes_read = mem_size;

  char *mapped_mem = mmap (0, mem_size, PROT_READ, MAP_SHARED, fd, 0);

  if (mapped_mem == MAP_FAILED)
  {
    print_error(module_name, NULL, strerror(errno));
    return -1;
  }

  char *temp = mapped_mem;

  int result = 0;

  /*
    Search in mapped memory using pointer arithmetic.
    Because of memmem uses memblock size (2nd arg),
    we need to adjust it. For each iteration we add to it
    difference between initial mapped mem address and
    last equality address.
  */
  while ((temp = memmem(
    temp,
    mem_size + (mapped_mem - temp),
    string2find,
    strlen(string2find))) != NULL)
  {
    result++;
    temp++;
  }

  if (munmap(mapped_mem, mem_size))
  {
    print_error(module_name, NULL, strerror(errno));
    return -1;
  }

  if (close(fd) == -1)
  {
    print_error(module_name, file_path, strerror(errno));
    return -1;
  }

  return result;
}

int dir_find_substr(
  const char *curr_dir_path,
  const char *string2find,
  const int max_processes_running)
{
  DIR *curr_dir = opendir(curr_dir_path);

  if (!curr_dir)
  {
    print_error(module_name, curr_dir_path, strerror(errno));
    return 1;
  }

  struct dirent *temp_dirent;

  while ((temp_dirent = readdir(curr_dir)) != NULL)
  {
    if (!strcmp(".", temp_dirent->d_name) || !strcmp("..", temp_dirent->d_name))
    {
      continue;
    }

    if (temp_dirent->d_type == DT_REG)
    {
      char full_path[PATH_MAX];
      char unresovled_path[PATH_MAX];

      strcpy(unresovled_path, curr_dir_path);
      strcat(unresovled_path, "/");
      strcat(unresovled_path, temp_dirent->d_name);

      realpath(unresovled_path, full_path);

      new_find_process(unresovled_path, string2find, max_processes_running);
    }
    else if (temp_dirent->d_type == DT_DIR)
    {
      char new_dir_path[PATH_MAX];

      strcpy(new_dir_path, curr_dir_path);
      strcat(new_dir_path, "/");
      strcat(new_dir_path, temp_dirent->d_name);

      dir_find_substr(new_dir_path, string2find, max_processes_running);
    }
  }

  //check readdir return value
  // if (errno)
  // {
  //   print_error(module_name, curr_dir_path, strerror(errno));
  //   return 1;
  // }

  if (closedir(curr_dir) == -1)
  {
    print_error(module_name, curr_dir_path, strerror(errno));
    return 1;
  }

  return 0;
}

int new_find_process(
  const char *file_path,
  const char *string2find,
  const int max_processes_running)
{
  if (*curr_processes_running == max_processes_running)
  {
    wait(NULL);
  }

  pid_t fd = fork();

  if (fd == -1)
  {
    print_error(module_name, NULL, strerror(errno));
    return 1;
  }

  if (fd == 0)
  {
    (*curr_processes_running)++;

    int bytes_read;
    int count = substr_in_file_count(file_path, string2find, &bytes_read);

    if (count > 0)
    {
      char buf[WRITE_BLOCK];

      sprintf(buf, "%d %s %d %d\n", getpid(), file_path, bytes_read, count);

      if (write(output_file, buf, strlen(buf)) == -1)
      {
        print_error(module_name, TEMP_OUTPUT_FILE, strerror(errno));
        exit(1);
      }
    }

    (*curr_processes_running)--;
    exit(0);
  }

  while (wait(NULL) != -1)
  {
    //do nothing
  }
}
