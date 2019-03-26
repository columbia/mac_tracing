void main(int argc, char *argv[]) {
  int done = 0; // Be 1 when receives a kill signal.
  int nworkers = atoi(argv[1]);
  pthread_create(..., NULL, listener, NULL);
  for (i = 0; i < nworkers; ++i)
    pthread_create(..., NULL, worker, NULL);
  ...; // Wait for threads to exit.
}
void *listener(void *arg) {
  ...; // Call bind() and listen().
  while (!done && poll(...)) {
      int sock = accept(...);
      worklist.add(sock);
  }
}
void *worker(void *arg) {
  while(!done && int sock = worklist.get()) {
    recv(sock, buf, ...);
    process_req(buf);
  } 
}
