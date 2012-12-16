#define EZH_BAUD 115200

struct record {
  unsigned long ms;
  unsigned short pulse_count;
#ifdef __i386__
} __attribute__((packed));
#else
};
#endif

struct records_header {
  unsigned long rtc;
  unsigned short record_count;
  unsigned short flags;
#ifdef __i386__
} __attribute__((packed));
#else
};
#endif
