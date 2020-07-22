#include <iostream>
using namespace std;

#include <libdipole/communicator.h>
#include "backend_idl.h"

int main() {
  Dipole::Communicator comm;
  Dipole::ObjectPtr ptr = comm.connect("ws://localhost:8080/", "hello");
  shared_ptr<HelloPtr> hello_o = Dipole::ptr_cast<HelloPtr>(&comm, ptr);
  cout << "start" << endl;
  for (int i = 0; i < 5; i++) {
    Greetings g = hello_o->sayHello("hi");
    cout << "got back: " << endl;
    cout << g.language << " " << g.text
	 << " " << get_enum_value_string(g.color)
	 << endl;
  }
      hello_o->sayHello("HI");
      
#if 0
  cout << "got back: " << hello_o->sayAloha("hawaii") << endl;

  auto hellocb_o = make_shared<HelloCBImpl>();
  string hellocb_o_id = comm.add_object(hellocb_o);
  Dipole::ObjPtr hellocb_o_ptr = communicator.get_object_ptr(hellocb_o_id);
  HelloCBPtr hellocb_ptr = Dipole::ptr_cast<HelloCB>(hellocb_o_ptr);

  hello_o->register_hello_cb(hellocb_ptr);
  cout << "got back: " << hello_o->sayHello() << endl;
  cout << "got back: " << hello_p->sayAloha("hawaii") << endl;
#endif
}
