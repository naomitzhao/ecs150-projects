#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <deque>

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "FileService.h"
#include "MySocket.h"
#include "MyServerSocket.h"
#include "dthread.h"

using namespace std;

int PORT = 8080;
int THREAD_POOL_SIZE = 1;
int BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "/dev/null";

int requests;
pthread_mutex_t requestLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bufferHasSpace = PTHREAD_COND_INITIALIZER;
pthread_cond_t hasRequest = PTHREAD_COND_INITIALIZER;

vector<HttpService *> services;

HttpService *find_service(HTTPRequest *request) {
  // don't find service if path contains ..
  if ((request->getPath()).find("..") != string::npos) {
    return NULL;
  }
   // find a service that is registered for this path prefix
  for (unsigned int idx = 0; idx < services.size(); idx++) {
    if (request->getPath().find(services[idx]->pathPrefix()) == 0) {
      return services[idx];
    }
  }

  return NULL;
}


void invoke_service_method(HttpService *service, HTTPRequest *request, HTTPResponse *response) {
  stringstream payload;

  // invoke the service if we found one
  if (service == NULL) {
    // not found status
    response->setStatus(404);
  } else if (request->isHead()) {
    service->head(request, response);
  } else if (request->isGet()) {
    service->get(request, response);
  } else {
    // The server doesn't know about this method
    response->setStatus(501);
  }
}

void handle_request(MySocket *client) {
  HTTPRequest *request = new HTTPRequest(client, PORT);
  HTTPResponse *response = new HTTPResponse();
  stringstream payload;
  
  // read in the request
  bool readResult = false;
  try {
    payload << "client: " << (void *) client;
    sync_print("read_request_enter", payload.str());
    readResult = request->readRequest();
    sync_print("read_request_return", payload.str());
  } catch (...) {
    // swallow it
  }    
    
  if (!readResult) {
    // there was a problem reading in the request, bail
    delete response;
    delete request;
    sync_print("read_request_error", payload.str());
    return;
  }
  
  HttpService *service = find_service(request);
  invoke_service_method(service, request, response);

  // send data back to the client and clean up
  payload.str(""); payload.clear();
  payload << " RESPONSE " << response->getStatus() << " client: " << (void *) client;
  sync_print("write_response", payload.str());
  cout << payload.str() << endl;
  client->write(response->response());
    
  delete response;
  delete request;

  payload.str(""); payload.clear();
  payload << " client: " << (void *) client;
  sync_print("close_connection", payload.str());
  client->close();
  delete client;
}

struct ThreadArgs {
  deque<MySocket*>* connections;
};

/**
 * helper thread routine to handle connections
 */
void *threadHandler(void *arg) {
  struct ThreadArgs *threadArgs = (struct ThreadArgs *) arg;
  deque<MySocket*>* connections = threadArgs->connections;
  
  while (true) {
    // lock
    int ret = dthread_mutex_lock(&requestLock);
    if (ret == -1) {
      throw("could not lock");
    }

    // go to sleep if no requests
    while (requests == 0) {
      dthread_cond_wait(&hasRequest, &requestLock);
    }
    
    // get the first connection in the queue (fifo)
    MySocket* connection = (*connections)[0];
    (*connections).pop_front();
    requests --;

    // signal that there is space in the buffer
    ret = dthread_cond_signal(&bufferHasSpace);
    if (ret == -1) {
      throw("could not signal");
    }

    // unlock
    ret = dthread_mutex_unlock(&requestLock);
    if (ret == -1) {
      throw("could not lock");
    }
    
    // handle the request (does not require lock)
    handle_request(connection);
  }
  
  // dthread_mutex_unlock(&requestLock);
  delete threadArgs;
  return NULL;
}

int main(int argc, char *argv[]) {

  signal(SIGPIPE, SIG_IGN);
  int option;

  while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1) {
    switch (option) {
    case 'd':
      BASEDIR = string(optarg);
      break;
    case 'p':
      PORT = atoi(optarg);
      break;
    case 't':
      THREAD_POOL_SIZE = atoi(optarg);
      break;
    case 'b':
      BUFFER_SIZE = atoi(optarg);
      break;
    case 's':
      SCHEDALG = string(optarg);
      break;
    case 'l':
      LOGFILE = string(optarg);
      break;
    default:
      cerr<< "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
      exit(1);
    }
  }

  set_log_file(LOGFILE);

  sync_print("init", "");
  MyServerSocket *server = new MyServerSocket(PORT);
  MySocket *client;

  // The order that you push services dictates the search order
  // for path prefix matching
  services.push_back(new FileService(BASEDIR));
  deque<MySocket*> connections;  // queue (fifo) to hold incoming requests after accepting

  vector<pthread_t> children;  // helper threads
  for (int idx = 0; idx < THREAD_POOL_SIZE; idx ++) {
    pthread_t threadId;

    // pass the connections queue to each helper thread
    struct ThreadArgs *threadArgs = new struct ThreadArgs;
    threadArgs->connections = &connections;

    // create the helper thread
    int ret = dthread_create(&threadId, NULL, threadHandler, threadArgs);
    if (ret == -1) {
      throw("could not create thread");
    }

    // add helper thread to children list
    children.push_back(threadId);
    threadArgs = NULL;
  }
  
  // server runs indefinitely
  while(true) {
    sync_print("waiting_to_accept", "");
    client = server->accept();
    sync_print("client_accepted", "");

    // lock
    int ret = dthread_mutex_lock(&requestLock);
    if (ret == -1) {
      throw("could not unlock");
    }

    // if the buffer is "full", go to sleep until it isn't
    while (requests == BUFFER_SIZE) {
      dthread_cond_wait(&bufferHasSpace, &requestLock);
    }
    
    // add the new connection
    connections.push_back(client);
    requests ++;

    // wake up all sleeping helper threads
    ret = dthread_cond_broadcast(&hasRequest);
    if (ret == -1) {
      throw("could not broadcast");
    }
    
    // unlock
    ret = dthread_mutex_unlock(&requestLock);
    if (ret == -1) {
      throw("could not unlock");
    }
  }
  
}
