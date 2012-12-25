#define EZH_BAUD 115200

struct record {
  unsigned long millis;
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
  unsigned long rtc_secs;
  unsigned long millis;
  unsigned short flags;
#ifdef __i386__
} __attribute__((packed));
#else
};
#endif
