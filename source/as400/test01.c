#define _MULTI_THREADED
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

pthread_key_t        tlsKey = 0;

void globalDestructor(void *value)
{
  printf("In the data destructor\n");
  free(value);
  pthread_setspecific(tlsKey, NULL);
}


int main(int argc, char **argv)
{
  int                   rc=0;
  int                   i=0;

  printf("Enter Testcase - %s\n", argv[0]);

  printf("Create a thread local storage key\n");
  rc = pthread_key_create(&tlsKey, globalDestructor);
  checkResults("pthread_key_create()\n", rc);
  /* The key can now be used from all threads */

  printf("- The key can now be used from all threads\n");
  printf("- in the process to storage thread local\n");
  printf("- (but global to all functions in that thread)\n");
  printf("- storage\n");

  printf("Delete a thread local storage key\n");
  rc = pthread_key_delete(tlsKey);
  /* The key and any remaining values are now gone. */
  printf("Main completed\n");
  return 0;
}

