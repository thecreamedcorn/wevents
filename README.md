# wevents
This is a simple header only library for c++ event handling. It is modled slightly after the signal slots mechanism of qt but with more feature rich and dynamic. The basis is that one can create signals that can be subscribed to and handled by diffrent invokable objects. Just a note, for some reason this library has a lot of issues on windows because they have a strange implementation of both std::function and also standard multi-threading stuff but the library works perfectly fine on linux. I have not tested it on OSX but I assume it would also work well on that.

## Overview of Code
### WSignal
This class is an special object that is able to be connected to just about any callable interface (ie method, function, lambda, etc.) as long as the callable interface has a compatable signature. So a WSignal template object of type WSignal<int, int> would only be able to be connected to callable interfaces whose signature is void(int, int).
After being connected to somthing one will then be able to call the WSignal objects emit method whose argument number and types are determined by the template arguments of that WSignal objects type. Once emit is called it will forward its arguments to all the callable interfaces the WSignal object is connected to in linear fasion (unless one specifys for a connection to be invoked asycrosouly).

### The connect Method
the connect method has multiple diffrent overloads but the basic gist is that it takes some a signal object and connects it to some invokable interface and in addition will return an invokable object that when called will destroy that connection. 
There are 4  overloads
* WSignal to object method - this connects a signal to the method of some object. That object's class must also be a child of WSlotObject in order for this to work as some extra code needs to be added to the object to make sure that automatic connection destruction works propely if either object goes out of scope or is deleted.
* WSignal to function or lambda - this connects a signal to some method or lambda
* WSignal to function or lambda based on object lifetime - this connects a signal to some method or lambda but the connection is destroyed when a specified object goes out of scope or is destroyed
* WSignal to WSignal - This basically allows for a signal to be forwarded to anouther WSignal template object of the same type
Every overload also take an optional ConOps (standing for connection options) type that allows for a connection to have certain behaviors. The two behaviors are adding a mutex which means that the mutex will be opened and cloed when trying to envoke that connection and also asycronous in which case the connection will be envoked and instead of waiting for that process to end before invoking the next connection the connection's handler is executed in a seperate thread.

### The WProperty type
This class is an example of what can be acheived using this event system and is also usefull for general event driven programs. It is essentially a wrapper for any variable value that can be bound to other WProperties and will be notified or notify bound properties when it's value changes.
If you would like to see an example of how such an object would be used check out the method testWProperty() in the file example.cpp.
