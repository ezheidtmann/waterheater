/**
 Write the contents of the buffer to a file.
 
 After writing, the buffer is reset to appear empty.
*/
int buf_write(HardwareSerial output) {
  struct record* buf;
  long len, i;
  _buf(&buf, &len, &i, 0);
  if (i) {
    output.write((uint8_t*) buf, i * sizeof(struct record));
  }
  i = 0;
  _buf(&buf, &len, &i, 1);
}

int buf_print(HardwareSerial output) {
  struct record* buf;
  long len, i, j;
  _buf(&buf, &len, &i, 0);
  for (j = 0; j < i; ++j) {
    output.print(buf[j].ms);
    output.print(", ");
    output.print(buf[j].val);
    output.println();
  }
  i = 0;
  _buf(&buf, &len, &i, 1);
}
