#define EZH_BAUD 115200

struct record {
  unsigned long ms;
  unsigned byte pulse_count;
  unsigned short air_temp;
  unsigned short water_temp;
#ifdef __i386__
} __attribute__((packed));
#else
};
#endif

struct records_header {
  unsigned byte record_count;
  unsigned long rtc;
  unsigned long micros;
  unsigned short flags;
#ifdef __i386__
} __attribute__((packed));
#else
};
#endif
