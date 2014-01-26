#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <corosync/cpg.h>


static int my_node = 0;

double sec( struct timespec t ) {
	return (double)t.tv_sec + (double)t.tv_nsec / 1000000000L;
}

struct t_msg {
  int      msg_id;

  struct timespec sender_time;
  pid_t    sender_pid;
  int      sender_node;
};

struct t_msg my_msg;

static cpg_handle_t handle;

#define PING 1
#define PONG 2

/**
 * Multicast a message
 */
static cs_error_t mcast(int msg) {
    struct iovec iov;

    my_msg.msg_id = msg;

    iov.iov_base = &my_msg;
    iov.iov_len = sizeof( my_msg );

    /*printf("mcast[msg_id=%d, time=%g, node=%d, pid=%d]\n",
    		my_msg.msg_id,
    		sec( my_msg.sender_time),
    		my_msg.sender_node,
    		my_msg.sender_pid);*/


    return cpg_mcast_joined (handle, CPG_TYPE_AGREED, &iov, 1);
}


static void cpg_deliver_fn(
   cpg_handle_t handle,
   const struct cpg_name *group_name,
   uint32_t nodeid,
   uint32_t pid,
   void *msg,
   size_t msg_len) {
   const struct t_msg *this = (struct t_msg*) msg;

   /*printf("len=%d, msg_id=%d, node=%d, pid=%d\n",
		   msg_len,
		   this->msg_id,
		   this->sender_node,
		   this->sender_pid);*/

   if( this->msg_id == PING ) {
	  cs_error_t res;

	  my_msg.sender_time = this->sender_time;
      my_msg.sender_node = this->sender_node;
      my_msg.sender_pid = this->sender_pid;

      if( this->sender_node != my_node )
    	  res = mcast( PONG );
   }
   else {
      if( getpid() == this->sender_pid && my_node == this->sender_node ) {
    	  struct timespec now;
    	  double diff;
		  clock_gettime(0, &now);

		  diff = sec( now ) - sec( this->sender_time );

		  printf("diff=%g\n", diff);
	  }
  }
}


static cpg_callbacks_t callbacks = {
    .cpg_deliver_fn = (cpg_deliver_fn_t)cpg_deliver_fn,
    .cpg_confchg_fn = (cpg_confchg_fn_t)NULL
};


int main (int argc, char *argv[]) {
  cs_error_t res;
  static struct cpg_name group;

  group.length = 7;
  strcpy( group.value, "mygroup");
  

  if( (res = cpg_initialize(&handle, &callbacks)) != CS_OK ) {
    printf("cpg_initialize returns=%d\n", res );
    return 1;
  }

  res = cpg_local_get ( handle, &my_node);
  res = cpg_join (handle, &group);

  printf("Node=%d\n", my_node);

  if( argc > 1 ) {
	  clock_gettime(0, &my_msg.sender_time );
	  my_msg.sender_node = my_node;
	  my_msg.sender_pid  = getpid();

	  res = mcast( PING );
  }

  {
    int fd;

    res = cpg_fd_get (handle, &fd);

    while(1) {
      res = cpg_dispatch(handle,CS_DISPATCH_ALL);
    }
  }

}
