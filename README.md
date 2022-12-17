# Database
Overall structure of your code:
The server contains a main method that consists of the REPL and calls the 
sighandler to ignore SIGINT. The method also handles EOF. Then we have the 
signalhandler constructor, destructor, and a monitor signal, which is the 
running routine of signalhandler. We also have thread_clean_up and deteall, the
former is the cleaning routine of any client thread, and the latter is 
responsible for cancelling all threads in the list. To initialize a client 
thread, we have functions client constructor, destructor and the running routine
run_client. They will creat, execute and destroy a client thread. Last, we have
client control wait, stop, and release that are responsible for the server's
functionality of stopping and going for client threads. 

Helper functions and what they do:
No helper functions. 

Changes to any function signatures:
Changes to the search() method in db.c and db.h as mentioned in handout. 

Unresolved bugs:
