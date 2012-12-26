// Buffer manager

/**
 Copy the contents of a record from `src' to `dst'.
*/
void record_copy(struct record* src, struct record* dst) {
   (*dst).ms = (*src).ms;
   (*dst).pulse_count = (*src).pulse_count;
}

/**
 Internal access to the buffer's memory block.
 
 Set `set' to 1 to record a new buffer location and length. 
 The current index will be set to 0.
 
 Set `set' to 0 to simply get the current buffer location,
 length, and index.
 
 Returns the number of items in the buffer, which is 
 equivalent to the index for the next write operation
*/
long _buf(struct record** buf, long* len, long* i, int set) {
  static struct record* thebuffer = 0;
  static long index = 0;
  static long length = 0;
  
  if (set) {
    if (buf) thebuffer = *buf;
    if (i) index = *i;
    if (len) length = *len;
    return index;
  } 
  else {
    if (buf) *buf = thebuffer;
    if (len) *len = length;
    if (i) *i = index;
    return index;
  }
}

/**
 Initialize the buffer to the specified length
 
 The new buffer is guaranteed to hold up to `len' items,
 but it may hold more: If the buffer is already longer than 
 the requested length, it is left unchanged.
 
 The contents of the memory occupied by the new buffer 
 are undefined, but it will act as if it is empty.
*/
void buf_init(long len) {
  struct record* buf;
  long current_len;
  _buf(&buf, &current_len, 0, 0);
  if (!buf || current_len < len) {
    if (buf) free(buf);
    buf = (struct record*) malloc(len * sizeof(struct record));
    current_len = len;
  }
  
  long i = 0;
  _buf(&buf, &current_len, &i, 1);
}

/**
 Add a record to the buffer.
 
 Returns 1 if successful, 0 if the buffer is full
*/
int buf_add(struct record* rec) {
  struct record* buf;
  long i, len;
  i = _buf(&buf, &len, 0, 0);
  if (i < len) {
    record_copy(rec, buf + i);
    i++;
    _buf(&buf, &len, &i, 1);
    return 1;
  }
  else {
    return 0;
  }
}

/**
 Determine if the buffer is full.
 */
int buf_full() {
  i = _buf(&buf, &len, 0, 0);
  struct record* buf;
  long i, len;
  return i >= len;
}
