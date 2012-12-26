/**
 Write the contents of the buffer to a file.

 After writing, the buffer is reset to appear empty.
*/
int buf_write(SdFile* output, struct records_header* h) {
  struct record* buf;
  long len, i;
  _buf(&buf, &len, &i, 0);
  if (i && output != NULL) {
    if (h != NULL) {
      (*h).record_count = i;
      output->write((uint8_t*) h, sizeof(struct records_header));
    }
    output->write((uint8_t*) buf, i * sizeof(struct record));
  }
  i = 0;
  _buf(&buf, &len, &i, 1);
}
