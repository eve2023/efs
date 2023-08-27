/*
   Copyright (c) 2023 Eve
   Licensed under the GNU General Public License version 3 (GPLv3)
*************************************************************************
   EFS.C: Eve's Fruit Server
*************************************************************************
EFS is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any
later version.

EFS is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along
with EFS. If not, see <https://www.gnu.org/licenses/>.
*************************************************************************
*/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <tcl.h>
#include <time.h>
#include <unistd.h>

#ifndef PORT
#define PORT 8080
#endif
// TODO document issues with file size and solve this
#define BUFFER_SIZE 1000 * 1024

struct vn_gets_buf
{
  short ln_so; // init: /
  short ln_eo; // init: 0, 0 for incomplete
  short read_eo; // init: 0
  char dat[32762];
};

void vn_gets(int fd, struct vn_gets_buf *buf);

char *vn_gets_to_string(struct vn_gets_buf *buf);

ssize_t vn_read(int fd, char *buf, ssize_t buffer_size);

void vn_puts(int fd, const void *buf, size_t len);

char *vn_string_map(char *kvs[], char *s);

char *get_content_type(char *suffix)
{
  char *content_types[] = {"html", "text/html", "htm", "text/html",
			   "css", "text/css", "js", "application/javascript",
			   "txt", "text/plain", "c", "text/plain",
			   "png", "image/png", "svg", "image/svg+xml"};
  return vn_string_map(content_types, suffix);
}

void write_header(int client_socket,
		 const char *content_type,
		 const int content_length)
{
  char rsp_hdr[512];
  char content_length_str[10];

  strcpy(rsp_hdr, "HTTP/1.1 200 OK\r\n"
	 "Content-Type: ");
  strcat(rsp_hdr, content_type);
  strcat(rsp_hdr, "; charset=utf-8\r\n"
	 "Content-Length: ");
  snprintf(content_length_str, sizeof(content_length_str), "%d", content_length);
  strcat(rsp_hdr, content_length_str);
  strcat(rsp_hdr, "\r\n"
	 "Connection: close\r\n"
	 "\r\n");

  write(client_socket, rsp_hdr, strlen(rsp_hdr));
}

// TODO remove dependency on Tcl
void gen_response_content(char *filepath,
			  char **content_type,
			  ssize_t *content_len,
			  char **content)
{
  Tcl_Interp *interp;
  Tcl_Obj *resultObj;

  // Initialize Tcl interpreter
  interp = Tcl_CreateInterp();
  if (Tcl_Init(interp) == TCL_ERROR) {
    fprintf(stderr, "Tcl_Init error: %s\n", Tcl_GetStringResult(interp));
    return;
  }

  // Evaluate the Tcl script containing the returnTwoStrings function
  if (Tcl_EvalFile(interp, "efs.tcl") != TCL_OK) {
    fprintf(stderr, "Tcl_EvalFile error: %s\n", Tcl_GetStringResult(interp));
    return;
  }

  Tcl_Obj *objv[] = {
    Tcl_NewStringObj("gen_response_content", -1),
    Tcl_NewStringObj(filepath, -1),
  };
    
  if (Tcl_EvalObjv(interp, 2, objv, 0) != TCL_OK)
    {
      fprintf(stderr, "Tcl_EvalObjv error: %s\n", Tcl_GetStringResult(interp));
      return;
    }

  // Get result object
  resultObj = Tcl_GetObjResult(interp);

  // Parse returned list into separate strings
  Tcl_Obj *string1, *string2;
  if (Tcl_ListObjIndex(interp, resultObj, 0, &string1) != TCL_OK ||
      Tcl_ListObjIndex(interp, resultObj, 1, &string2) != TCL_OK) {
    fprintf(stderr, "Tcl_ListObjIndex error: %s\n", Tcl_GetStringResult(interp));
    return;
  }

  *content_type = strdup(Tcl_GetString(string1));

  int len;
  char *string2_s = Tcl_GetStringFromObj(string2, &len);
  *content = malloc(len);
  memcpy(*content, string2_s, len);
  *content_len = len;

  // Clean up and exit
  Tcl_DeleteInterp(interp);
}

void handle_get(int client_socket, char *filepath)
{
  char buf[BUFFER_SIZE];
  char *content_type;
  char *content;
  ssize_t content_len;

#ifdef WEBSITE_MAINTENANCE
  char *m_r = "周末网站暂停服务，休息休息，工作日继续开发吧. Halting development on weekends.";
  write_header(client_socket, "text/html", strlen(m_r));
  vn_puts(client_socket, m_r, strlen(m_r));
  return;
#endif
  
  gen_response_content(filepath + 1, &content_type, &content_len, &content);

  if (content_len == 0)
    {
      // TODO put this into my string library
      int fd = open(filepath + 1, O_RDONLY);
      // if content_len == 0, then fd != -1
      ssize_t n = vn_read(fd, buf, sizeof(buf));

      content_len = n;
      content = buf;
      close(fd);
    }

  // TODO investigate m4a play issue
  write_header(client_socket, content_type, content_len);
  vn_puts(client_socket, content, content_len);

  free(content_type);
  if (content != buf)
    {
      free(content);
    }

 clean_client_socket:
  shutdown(client_socket, SHUT_RDWR);
  close(client_socket);
}

void handle_put_send_err(int client_socket, int e_errno)
{
  char response[512];
  snprintf(response, 512, "HTTP/1.1 500 Internal Server Error\r\n"
	   "Content-Type: text/plain\r\n"
	   "\r\n"
	   "File upload failed, ERRNO=%d\r\n",
	   e_errno);
  vn_puts(client_socket, response, strlen(response));
}

/*
  TODO
  Currently it only supports uploading static content.
  We want to support uploading dynamic web apps.
  Provide a POSIX interface where a user can upload a C webserver source,
  compile it, and run the "servlet" like an app.
 */
void handle_put(int client_socket, struct vn_gets_buf *buf, char *filepath)
{
  char boundary[100];

  // Step 1: find this line & extract boundary
  // Content-Type: multipart/form-data; boundary=-abc
  while (vn_gets(client_socket, buf), buf->ln_so < buf->ln_eo)
    {
      char *s = vn_gets_to_string(buf);

      if (strstr(s, "Content-Type: multipart/form-data") == s)
	{
	  regex_t re;
	  regmatch_t ma[2];

	  int ret = regcomp(&re, "boundary=([^;, \r\n]+)", REG_EXTENDED);
	  if (ret)
	    {
	      printf("Failed to compile regex\n");
	      handle_put_send_err(client_socket, 1);
	      return;
	    }

	  ret = regexec(&re, s, 2, ma, 0);
	  if (ret == 0)
	    {
	      strncpy(boundary, s + ma[1].rm_so, ma[1].rm_eo - ma[1].rm_so);
	      boundary[ma[1].rm_eo - ma[1].rm_so] = '\0';
	    }
	  else
	    {
	      printf("Reg not matched\n");
	      handle_put_send_err(client_socket, 2);
	      return;
	    }

	  regfree(&re);
	  break;
	}

      free(s);
    }

  if (buf->ln_so == buf->ln_eo)
    {
      printf("Parse multipart: Early ending step 1\n");
      handle_put_send_err(client_socket, 3);
      return;
    }

  // Step 2: find boundary line
  while (vn_gets(client_socket, buf), buf->ln_so < buf->ln_eo)
    {
      if (strncmp(buf->dat + buf->ln_so + 2, boundary, strlen(boundary)) == 0)
	break;
    }

  if (buf->ln_so == buf->ln_eo)
    {
      printf("Parse multipart: Early ending step 2\n");
      handle_put_send_err(client_socket, 4);
      return;
    }

  // Step 3: find empty line
  while (vn_gets(client_socket, buf), buf->ln_so < buf->ln_eo)
    {
      if (buf->ln_eo - buf->ln_so == 2)
	break;
    }

  if (buf->ln_so == buf->ln_eo)
    {
      printf("Parse multipart: Early ending step 3\n");
      handle_put_send_err(client_socket, 5);
      return;
    }

  // Step 4: find boundary line
  // TODO add lock for file
  FILE *fp = fopen(filepath + 1, "wb");
  if (fp == NULL)
    {
      fprintf(stderr, "Failed to open file '%s'\n", filepath + 1);
      handle_put_send_err(client_socket, 6);
      return;
    }

  while (vn_gets(client_socket, buf), buf->ln_so < buf->ln_eo)
    {
      if (strncmp(buf->dat + buf->ln_so + 2, boundary, strlen(boundary)) == 0)
	break;

      size_t wlen = fwrite(buf->dat + buf->ln_so, buf->ln_eo - buf->ln_so, 1, fp);
      if (wlen != 1)
	{
	  fprintf(stderr, "Failed to write to file\n");
	  handle_put_send_err(client_socket, 7);
	  goto cleanup;
	}
    }

  char *rok = "OK";
  write_header(client_socket, "text/html", strlen(rok));
  vn_puts(client_socket, rok, strlen(rok));

 cleanup:
  fclose(fp);
}

void *client_handler(void *arg)
{
  int client_fd = *((int *)arg);

  // Set the client socket to non-blocking mode
  int flags = fcntl(client_fd, F_GETFL, 0);
  fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

  struct vn_gets_buf buf;

  buf.ln_eo = 0;
  buf.read_eo = 0;

  vn_gets(client_fd, &buf);
  char *s = vn_gets_to_string(&buf);

  char *verb = strtok(s, " ");
  char *path = strtok(NULL, " ");
  if (verb && path)
    if (strcmp(verb, "GET") == 0)
      {
	handle_get(client_fd, path);
      }
    else if (strcmp(verb, "PUT") == 0)
      {
	handle_put(client_fd, &buf, path);
      }

  free(s);
  close(client_fd);
  free(arg);
  return NULL;
}

void efs_serve(void)
{
  int server_fd;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_len = sizeof(client_addr);
  pthread_t client_thread;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
      perror("bind");
      close(server_fd);
      exit(EXIT_FAILURE);
    }

  if (listen(server_fd, 10) == -1)
    {
      perror("listen");
      exit(EXIT_FAILURE);
    }

  printf("Listening on port %d...\n", PORT);
  while (1)
    {
      int *client_fd_ptr = malloc(sizeof(int));
      *client_fd_ptr = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
      if (*client_fd_ptr == -1)
	{
	  perror("accept");
	  free(client_fd_ptr);
	  continue;
	}

      // handle_connection(client_socket, NULL);
      if (pthread_create(&client_thread, NULL, client_handler, client_fd_ptr) != 0)
	{
	  perror("pthread_create");
	  close(*client_fd_ptr);
	  free(client_fd_ptr);
        }
      else
	{
	  pthread_detach(client_thread);
	}
    }

  close(server_fd);
}
