struct channel {
  void * chan;
  struct proc * sleeptable[NPROC];
  uint num_sleeping;
  struct channel * next;
};


