#include <iostream>
#include <sstream>
using namespace std;

#include <unistd.h> // sleep

#include <kvan/uuid.h>
#include <kvan/json-io.h>
#include <libdipole/communicator.h>
#include <libdipole/proto.h>
#include <libdipole/remote-methods.h>

Dipole::Object::~Object()
{
}

Dipole::Communicator* Dipole::Communicator::comm{nullptr};

Dipole::Communicator::Communicator()
{
  RemoteMethods::set_communicator(this);
}

void Dipole::Communicator::set_listen_port(int listen_port)
{
  this->listen_port = listen_port;
}

string Dipole::Communicator::add_object(shared_ptr<Object> o,
					const string& object_id_)
{
  string object_id = object_id_ == "" ? uuid::generate_uuid_v4() : object_id_;
  auto it = objects.find(object_id);
  if (it != objects.end()) {
    ostringstream m;
    m << "Communicator::add_object: object_id " << object_id << " was already taken";
    throw runtime_error(m.str());
  }
  objects[object_id] = o;
  return object_id;
}

shared_ptr<Dipole::Object>
Dipole::Communicator::find_object(const string& object_id)
{
  auto it = objects.find(object_id);
  if (it == objects.end()) {
    ostringstream m;
    m << "Dipole::Communicator::find_object: can't find object " << object_id;
    throw runtime_error(m.str());
  }
  return (*it).second;
}
    
shared_ptr<ix::WebSocket>
Dipole::Communicator::connect(const string& ws_url, const string& object_id)
{
  auto webSocket = make_shared<ix::WebSocket>();
  webSocket->setUrl(ws_url);
  webSocket->setOnMessageCallback([this, webSocket](const ix::WebSocketMessagePtr& msg) {
      cerr << "got something" << endl;
      if (msg->type == ix::WebSocketMessageType::Message)
        {
	  this->dispatch(webSocket, msg->str);
        }
    });

  // Now that our callback is setup, we can start our background thread and receive messages
  webSocket->start();
  while (webSocket->getReadyState() != ix::ReadyState::Open) {
    cerr << "in progress..." << endl;
    sleep(2);
  }

  return webSocket;
}

void Dipole::Communicator::dispatch(shared_ptr<ix::WebSocket> ws, const string& msg)
{
  cout << "Dipole::Communicator::dispatch: received: " << msg << endl;
  cout << "Dipole::Communicator::dispatch: waiters size: " << waiters.size() << endl;

  auto msg_type = get_message_type(msg);
  switch (msg_type) {
  case message_type_t::METHOD_CALL:
    dispatch_method_call(ws, msg);
    break;
  case message_type_t::METHOD_CALL_RETURN:
  case message_type_t::METHOD_CALL_EXCEPTION:
    dispatch_response(msg_type, msg);
    break;
  }
}

void Dipole::Communicator::dispatch_method_call(shared_ptr<ix::WebSocket>  ws,
						const string& msg)
{
  cout << "Dipole::Communicator::dispatch_method_call: waiters: " << waiters.size() << endl;
  string method_signature = get_method_signature(msg);
  auto method_call = RemoteMethods::find_method(method_signature);
  string res_msg;
  method_call->do_call(msg, &res_msg, ws);
  cout << "Dipole::Communicator::dispatch_method_call response: " << res_msg << endl;
  ws->sendBinary(res_msg);
}

void Dipole::Communicator::dispatch_response(message_type_t msg_type,
					     const string& msg)
{
  auto orig_message_id = get_orig_message_id(msg);
  this->signal_response(orig_message_id, msg_type, msg);
}

pair<Dipole::message_type_t, string>
Dipole::Communicator::wait_for_response(const string& message_id)
{
  cout << "Dipole::Communicator::wait_for_response: waiters size: " << waiters.size() << endl;
  auto it = waiters.find(message_id);
  if (it != waiters.end()) {
    ostringstream m;
    m << "Dipole::Communicator::wait_for_response: message_id is already waited for: " << message_id;
    throw runtime_error(m.str());
  }
  waiters[message_id] = make_shared<Waiter>();
  pair<message_type_t, string> ret;
  waiters[message_id]->blocking_get(&ret);
  waiters.erase(message_id);
  return ret;
}

void Dipole::Communicator::signal_response(const string& message_id,
					   message_type_t msg_type,
					   const string& msg)
{
  cout << "Dipole::Communicator::signal_response: waiters size: " << waiters.size() << endl;
  auto it = waiters.find(message_id);
  if (it == waiters.end()) {
    ostringstream m;
    m << "Dipole::Communicator::signal_response: unknown message_id: " << message_id;
    throw runtime_error(m.str());
  }
  (*it).second->blocking_put(make_pair(msg_type, msg));
}

void Dipole::Communicator::check_response(message_type_t msg_type,
					  const string& msg)
{
  switch (msg_type) {
  case Dipole::message_type_t::METHOD_CALL_RETURN:
    break;
  case Dipole::message_type_t::METHOD_CALL_EXCEPTION:
    {
      Dipole::ExceptionResponse eres;
      from_json(&eres, msg);
      throw Dipole::RemoteException(eres.remote_exception_text);
    }
    break;
  case Dipole::message_type_t::METHOD_CALL:
    throw runtime_error("HelloPtr::sayHello: unexcepted message type");
    break;
  }
}

void Dipole::Communicator::run()
{
  ix::WebSocketServer server(listen_port);
  server.setOnConnectionCallback([&server, this]
				 (std::shared_ptr<ix::WebSocket> webSocket,
				  std::shared_ptr<ix::ConnectionState> connectionState) {
				   //std::cout << "Remote ip: " << connectionInfo->remoteIp << std::endl;
				   webSocket->setOnMessageCallback([webSocket, connectionState, &server, this](const ix::WebSocketMessagePtr& msg) {
				       if (msg->type == ix::WebSocketMessageType::Open) {
					 std::cout << "New connection" << std::endl;
					 
					 // A connection state object is available, and has a default id
					 // You can subclass ConnectionState and pass an alternate factory
					 // to override it. It is useful if you want to store custom
					 // attributes per connection (authenticated bool flag, attributes, etc...)
					 std::cout << "id: " << connectionState->getId() << std::endl;
					 
					 // The uri the client did connect to.
					 std::cout << "Uri: " << msg->openInfo.uri << std::endl;
					 
					 std::cout << "Headers:" << std::endl;
					 for (auto it : msg->openInfo.headers) {
					   std::cout << it.first << ": " << it.second << std::endl;
					 }
				       } else if (msg->type == ix::WebSocketMessageType::Message) {
					 string res_s;
					 this->dispatch(webSocket, msg->str);
				       } else if (msg->type == ix::WebSocketMessageType::Close) {
					 cout << "connection closed" << endl;
				       } else {
					 throw runtime_error("handle_ws_messages: unknown message");
				       }
				     });				     
				 });
  
  auto res = server.listen();
  if (!res.first) {
    // error
    throw runtime_error("main: can't listen");
  }
  
  server.start();
  server.wait();    
}
