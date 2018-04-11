/*
4. Написать программу поиска заданной пользователем комбинации из m байт
(m <255) во всех файлах заданного каталога. Главный процесс открывает каталог и
запускает для каждого файла каталога отдельный процесс поиска заданной
комбинации из m байт. Каждый процесс выводит на экран свой pid, полный путь к
файлу, общее число просмотренных байт и результаты (сколько раз найдена
комбинация) поиска (все в одной строке). Число одновременно работающих
процессов не должно превышать N (вводится пользователем). Проверить работу
программы для каталога /etc и строки «ifconfig».
*/

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

#include "SLList.h"

#define VALID_ARGS_COUNT 4
#define READ_BLOCK       4096
#define WRITE_BLOCK      4096
#define TEMP_OUTPUT_FILE "/tmp/output"

void print_error (const char*, const char*, const char*);
bool is_dir (const char*);
int read_dir_to_list (const char*, struct SLList*);
int substr_in_str_count(const char*, const char*);
int substr_in_file_count(const char*, const char*, int*);
int print_file(const char*);

char *module_name;

int main(int argc, char *argv[])
{
  module_name = basename(argv[0]);

  const char *source_dir     = argv[1];
  const char *found_sequence = argv[2];
  const int   max_procceses  = atoi(argv[3]);

  struct SLList *files_list  = malloc(sizeof(struct SLList *));
  if (!files_list)
  {
    print_error(module_name, NULL, "memory allocation error");
    return 1;
  }

  int output_file = open(TEMP_OUTPUT_FILE, O_CREAT | O_WRONLY | O_EXCL, 0640);
  if (output_file == -1)
  {
    print_error(module_name, TEMP_OUTPUT_FILE, strerror(errno));
    return 1;
  }

  sll_init(files_list);

  read_dir_to_list(source_dir, files_list);

  int *curr_processes_running = mmap(0,
                                sizeof(int*),
                                PROT_READ | PROT_WRITE,
                                MAP_ANONYMOUS | MAP_SHARED,
                                -1, 0);

  if (curr_processes_running == (void*)-1)
  {
    print_error(module_name, NULL, strerror(errno));
    return 1;
  }

  int *all_processes = malloc(sizeof(int*) * sll_size(files_list));

  if (!all_processes)
  {
    print_error(module_name, NULL, "memory allocation error");
    return 1;
  }

  int curr_process = 0;

  struct Node* temp = files_list->head;

  *curr_processes_running = 0;

  while(temp->next != NULL)
  {
    if (*curr_processes_running < max_procceses)
    {
      int fd = fork();

      if (fd == -1)
      {
        print_error(module_name, NULL, strerror(errno));
        return 1;
      }

      if (fd == 0)
      {
        *curr_processes_running = *curr_processes_running + 1;

        int bytes;
        int count = substr_in_file_count(temp->next->value, found_sequence, &bytes);

        if (count > 0)
        {
          char buf[WRITE_BLOCK];

          sprintf(buf, "%d %s %d %d\n", getpid(), temp->next->value, bytes, count);

          if (write(output_file, buf, strlen(buf)) == -1)
          {
            print_error(module_name, TEMP_OUTPUT_FILE, strerror(errno));
            return 1;
          }
        }

        *curr_processes_running = *curr_processes_running - 1;
        exit(0);
      }
      else
      {
        all_processes[curr_process++] = fd;
      }

      usleep(10);
      temp = temp->next;
    }
  }

  int i;
  for (i = 0; i < curr_process; i++)
  {
    if (waitpid(all_processes[i], 0, 0) == -1)
    {
      print_error(module_name, NULL, "waitpid failed.");
      return 1;
    }
  }

  munmap(curr_processes_running, sizeof(int*));

  sll_terminate(files_list);

  free(all_processes);
  free(files_list);

  if (close(output_file) == -1)
  {
    print_error(module_name, TEMP_OUTPUT_FILE, strerror(errno));
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

int read_dir_to_list(const char *curr_dir_path, struct SLList *source_list)
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

      sll_push(source_list, full_path);
    }
    else if (temp_dirent->d_type == DT_DIR)
    {
      char new_dir_path[PATH_MAX];

      strcpy(new_dir_path, curr_dir_path);
      strcat(new_dir_path, "/");
      strcat(new_dir_path, temp_dirent->d_name);

      read_dir_to_list(new_dir_path, source_list);
    }
  }

  //check readdir return value
  if (errno)
  {
    print_error(module_name, curr_dir_path, strerror(errno));
    return 1;
  }

  if (closedir(curr_dir) == -1)
  {
    print_error(module_name, curr_dir_path, strerror(errno));
    return 1;
  }

  return 0;
}

bool is_dir(const char *dir_path)
{
  struct stat temp_stat;

  if (lstat(dir_path, &temp_stat) == -1)
  {
    print_error(module_name, NULL, strerror(errno));
    return false;
  }

  return S_ISDIR(temp_stat.st_mode);
}

void print_error(const char *module_name, const char* additional_info, const char *error_msg)
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

int substr_in_str_count(const char *source_string, const char *string2find)
{
  int result = 0;
  const char *temp = source_string;

  while ((temp = strstr(temp, string2find)) != NULL)
  {
    result++;
    temp++;
  }

  return result;
}

int substr_in_file_count(const char *file_path, const char *string2find, int *total_bytes_read)
{
  *total_bytes_read = 0;
  int result = 0;
  char buf[READ_BLOCK + 1];

  int fd = open(file_path, O_RDONLY);

  if (fd == -1)
  {
    print_error(module_name, file_path, strerror(errno));
    return 0;
  }

  int bytes_read;

  while ((bytes_read = read(fd, buf, READ_BLOCK)) > 0)
  {
    buf[bytes_read] = '\0';
    *total_bytes_read += bytes_read;

    result += substr_in_str_count(buf, string2find);
  }

  if (close(fd) == -1)
  {
    print_error(module_name, file_path, strerror(errno));
    return 0;
  }

  return result;
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
