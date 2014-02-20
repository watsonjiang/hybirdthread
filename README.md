
This is an implementation of a M to N thread model.
It mix the advantage of user-space thread and kernel-space thread.
Differ from tranditional pth and pthread api, you can get dispatch 
your code to a seperate native pthread by simplely wrapping code with 
ht_hand_out() and ht_get_back() function calls.

example:
   //TBD

