#include <stdio.h>
#include "hybird.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <queue>
#include <string>

std::queue<std::string> msg_queue;
pth_mutex_t msg_queue_lock = PTH_MUTEX_INIT;
pth_cond_t msg_queue_cond_not_empty = PTH_COND_INIT;

void * handler(void * arg)
{
   int fd = (int)arg;
   char buf[100] = {0};
   while(1)
   {
      printf("4%$%%%%%%%%%\n");
      int len = pth_read(fd, buf, 100);
      if(len == 0)
      {
         close(fd);
         return NULL;
      }
      buf[len] = 0;
      pth_write(fd, buf, len);  //echo server
      pth_mutex_acquire(&msg_queue_lock, FALSE, NULL);
      msg_queue.push(std::string(buf));
      pth_cond_notify(&msg_queue_cond_not_empty, FALSE);
      pth_mutex_release(&msg_queue_lock);
   }
   return NULL;
}

void * replication(void * arg)
{
   while(1)
   {
      printf("1@@@@@@@@@\n");
      pth_mutex_acquire(&msg_queue_lock, FALSE, NULL);
      pth_cond_await(&msg_queue_cond_not_empty, &msg_queue_lock, NULL);
      std::string v = msg_queue.front();
      msg_queue.pop();
      pth_mutex_release(&msg_queue_lock);
      printf("replicate %s", v.c_str());
   }
}

int main(int argc, const char *argv[])
{
   sockaddr_in sar;
   protoent *pe;
   sockaddr_in peer_addr;
   int peer_len;
   int sa, sw;
   hybird_init(2);
   signal(SIGPIPE, SIG_IGN);
   pth_attr_t attr = pth_attr_new();
   pth_attr_set(attr, PTH_ATTR_NAME, "replicator");
   pth_attr_set(attr, PTH_ATTR_STACK_SIZE, 256*1024);
   pth_attr_set(attr, PTH_ATTR_JOINABLE, FALSE);
   pth_spawn(attr, replication, NULL);

   pth_attr_set(attr, PTH_ATTR_NAME, "handler");
   pe = getprotobyname("tcp");
   sa = socket(AF_INET, SOCK_STREAM, pe->p_proto);
   sar.sin_family = AF_INET;
   sar.sin_addr.s_addr = INADDR_ANY;
   sar.sin_port = htons(9999);
   bind(sa, (struct sockaddr*)&sar, sizeof(struct sockaddr_in));
   listen(sa, 10);

   for(;;)
   {
      printf("3#####i\n");
      peer_len = sizeof(peer_addr);
      sw = pth_accept(sa, (struct sockaddr *)&peer_addr, &peer_len);
      pth_spawn(attr, handler, (void*)sw);
   }

   return 0;
}
