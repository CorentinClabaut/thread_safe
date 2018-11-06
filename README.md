# thread_safe
Simple implementation of a thread safe queue

This Queue enables to synchronize several threads allowing them to safelly push and pop data.
The pop will be blocking until a data is available, and every push will notify one waiting thread.

Any thread can signal the other thread not to use the queue anymore by calling it's release method.

Once release has been called, all the thread waiting for a data to pop will throw an exception. 
Any further attempt to push or pop to the queue will also throw an exception.
